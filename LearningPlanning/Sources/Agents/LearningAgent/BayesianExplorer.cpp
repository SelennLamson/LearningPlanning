/**
 * @author Thomas Lamson
 * Contact: thomas.lamson@cea.fr
 *
 * This Bayesian Explorer Agent uses hill climbing over a Revision Probability to select the best
 * plan of actions in order to learn the rules of the domain faster. Meta-actions are also
 * available during learning, like removing an entity from the problem, or the reset action.
 */

#include <ctime>

#include "Agents/LearningAgent/BayesianExplorer.h"
#include "Agents/LearningAgent/LearningAgent.h"

UnknownRule::UnknownRule(float rawProb, shared_ptr<Domain> domain, size_t instSize, Literal inGroundedAction) {
	nAll = 0;
	set<Predicate> preds = domain->getPredicates();
	foreach(pred, preds)
		nAll += (int)powf((float)instSize, (float)pred->arity);

	pAny = 1.0f - powf(rawProb, 1.0f / (float)nAll);

	groundedAction = inGroundedAction;
}

float UnknownRule::computeProb(State state, /*r*/ float& expectedGain) {
	expectedGain = 0.0f;
	float prob = 1.0f;
	float falseAnyFacts = (float)nAll - (float)state.facts.size();

	foreach(pair, pNfs)
		if (!state.contains(pair->first)) {
			falseAnyFacts--; // if fact is in pNfs, it is not "any" fact
			prob *= 1.0f - pair->second;
			expectedGain += pair->second;
		}

	prob *= powf(1.0f - pAny, falseAnyFacts);

	expectedGain += falseAnyFacts * pAny;
	expectedGain *= prob / (1.0f - prob);

	return prob;
}

void UnknownRule::corroborateFailure(State state) {
	float dummy = 0.0f;
	float pFail = 1.0f - computeProb(state, dummy);

	if (pFail == 0.0f) return;

	// Fact is an "any" fact, but it gains a specific value as it is verified in current experiment
	foreach(fact, state.facts)
		if (!in(pNfs, *fact)) pNfs[*fact] = pAny;

	foreach(pair, pNfs)
		if (!in(state.facts, pair->first))
			pNfs[pair->first] = pair->second / pFail;

	pAny /= pFail;
}

BayesianExplorer::BayesianExplorer(bool verbose) : ExplorerAgentBase(verbose) {
	bayesianConfig = config->getSubconfig("bayesian_explorer");

	random = bayesianConfig->getBool("random");
	useStagnation = bayesianConfig->getBool("use_stagnation");
	usePassthrough = bayesianConfig->getBool("use_passthrough");
	
	gamma = bayesianConfig->getFloat("gamma");
	startPu = bayesianConfig->getFloat("start_pu");
	explorationTimeLimit = bayesianConfig->getFloat("exploration_time_limit");
	passthroughThreshold = bayesianConfig->getFloat("passthrough_threshold");
	metaProbability = bayesianConfig->getFloat("meta_probability");
	factRemovalDiscount = bayesianConfig->getFloat("fact_removal_discount");
	randomDiscount = bayesianConfig->getFloat("random_discount");
	focusSpecificRules = bayesianConfig->getFloat("focus_specific_rules");

	//estimatedPreconditionsPerRule = bayesianConfig->getInt("estimated_preconditions_per_rule");
	estimatedRulesPerAction = bayesianConfig->getInt("estimated_rules_per_action");
	randomPlans = bayesianConfig->getInt("random_plans");
	randomExperiments = bayesianConfig->getInt("random_experiments");
	randomActionTrials = bayesianConfig->getInt("random_action_trials");
	planDepth = bayesianConfig->getInt("plan_depth");
	stagnationThreshold = bayesianConfig->getInt("stagnation_threshold");

	saveMotivationTrace = bayesianConfig->getBool("save_motivation_trace");
	motivationTraceFileName = bayesianConfig->getString("motivation_trace_file_name");
}

void BayesianExplorer::init(shared_ptr<Domain> inDomain, vector<Term> inInstances, Goal inGoal, shared_ptr<vector<Trace>> inTrace) {
	Agent::init(inDomain, inInstances, inGoal, inTrace);
	prepareActionSubstitutions();
	deletedInstances.clear();
}

void BayesianExplorer::setRules(vector<shared_ptr<ActionRule>> newRules) {
	rules = newRules;
}

void BayesianExplorer::setActionLiterals(set<Literal> baseActionLiterals) {
	actionLiterals.clear();
	actionPredicates.clear();
	vector<Term> allInsts = instances + domain->getConstants();
	foreach(lit, baseActionLiterals) {
		actionPredicates.insert(lit->pred);
		vector<Substitution> subs = Substitution().expandUncovered(lit->parameters, allInsts, true);
		foreach(sub, subs)
			actionLiterals.insert(sub->apply(*lit));
	}

	foreach(action, actionLiterals)
		if (!in(unknownRules, *action))
			unknownRules[*action] = UnknownRule(bayesianConfig->getFloat("start_pu"), domain, allInsts.size(), *action);
}

Literal BayesianExplorer::getNextAction(State state) {
	iteration++;

	if (iteration % 50 == 0 && saveMotivationTrace)
		saveMotivationTraceFile();

	/*if (plan.size() == 0 && deletedInstances.size() > 0) {
		deletedInstances.clear();
		return Literal(domain->getActionPredByName("reset"));
	}*/

	if (plan.size() == 0 && domain->removedFacts.size() > 0) {
		domain->removedFacts = {};
		return Literal(domain->getActionPredByName("remove-fact"));
	}

	if (plan.size() == 0) {
		generateRandomPlan(state);
	}

	/*debug << state << endl;
	foreachindex(i, plan) {
		debug << plan[plan.size() - i - 1];
		if (i < plan.size() - 1) debug << "->";
	}
	debug << endl;*/


	if (plan.size() > 0) {
		Literal nextAction = plan.back();
		lastRevProb = revisionProbs.back();
		plan.pop_back();
		revisionProbs.pop_back();

		if (nextAction.pred.name != "delete" && nextAction.pred.name != "reset" && nextAction.pred.name != "remove-fact") {
			Experiment exp = Experiment(state, nextAction);

			allExperiments.insert(exp);

			if (plan.size() == 0) {
				if (targetRule) {
					if (in(experimentsPerRule, targetRule)) experimentsPerRule[targetRule].insert(exp);
					else experimentsPerRule[targetRule] = { exp };
				}

				if (in(experimentsPerAction, exp.action.pred)) experimentsPerAction[exp.action.pred].insert(exp);
				else experimentsPerAction[exp.action.pred] = { exp };
			}

			log << "Experiments: " << allExperiments.size() << endl;
		}

		if (nextAction.pred.name == "reset")
			deletedInstances.clear();
		else if (nextAction.pred.name == "delete")
			deletedInstances.insert(nextAction.parameters[0]);

		return nextAction;
	}
	return Literal();
}

void BayesianExplorer::updateProblem(vector<Term> inInstances, Goal inGoal, vector<Literal> inHeadstart) {
	Agent::updateProblem(inInstances, inGoal, inHeadstart);
	plan.clear();
	revisionProbs.clear();
	
	allExperiments.clear();
	experimentsPerAction.clear();
	experimentsPerRule.clear();

	deletedInstances.clear();
	prepareActionSubstitutions();
}

float BayesianExplorer::computePu(Experiment e, /*r*/ float& expectedGain) {
	expectedGain = 0.0f;
	if (e.action.pred.name == "remove-fact" || e.action.pred.name == "delete" || e.action.pred.name == "reset") return 0.0f;
	return unknownRules[e.action].computeProb(e.state, expectedGain);
}

float BayesianExplorer::computePu(Experiment e) {
	float dummy = 0.0f;
	return computePu(e, dummy);
}

void BayesianExplorer::corroborateRules(Trace trace) {
	if (trace.instAct.pred.name == "remove-fact" || trace.instAct.pred.name == "delete" || trace.instAct.pred.name == "reset") return;
	float cumulatedChange = 0.0f;

	// Preparing variables and computing trace effects
	vector<Term> allInsts = instances + domain->getConstants();
	set<ActionRule*> rulesForCurrentAction;

	set<Literal> added, removed;
	trace.state.difference(trace.newState, &added, &removed);
	State effects = State(added + removed);

	map<ActionRule*, vector<Unverified>> posSigmas, negSigmas;
	map<ActionRule*, float> protRTs;
	map<ActionRule*, float> covRTs;

	// Precomputing non-conditional terms (Prot and Cov|Prot)
	float covMT = 1.0f;
	foreach(rit, rules) {
		ActionRule* rule = &**rit;

		if (Literal::compatible(rule->actionLiteral, trace.instAct)) {
			rulesForCurrentAction.insert(rule);

			set<Unverified> posSet, negSet;
			rule->processEffects(posSet, negSet, trace.state, trace.instAct, effects, allInsts);
			vector<Unverified> pos = toVec(posSet), neg = toVec(negSet);

			posSigmas[rule] = pos;
			negSigmas[rule] = neg;

			//cout << "Corroboration 1: CD with " << neg.size() << " disjunctions." << endl;
			protRTs[rule] = rule->cdProb(rule->precondsNecessities, rule->constsNecessities, neg);
			if (protRTs[rule] == 0.0f) return;

			//assertMsg(isProb(protRTs[rule]), "protRTrule not a probability");

			float nCovRT = 1.0f;
			vector<Unverified> conditionalCds = neg;
			foreach(disj, pos) {
				//cout << "Corroboration 2: DGCD with " << conditionalCds.size() << " conditional disjunctions." << endl;
				nCovRT *= rule->dgcdProb(rule->precondsNecessities, rule->constsNecessities, *disj, conditionalCds);
				conditionalCds.push_back(*disj);
			}
			covRTs[rule] = 1.0f - nCovRT;

			//assertMsg(isProb(covRTs[rule]), "covRTrule not a probability");

			covMT *= nCovRT;
		}
	}

	float pUe = computePu(Experiment(trace.state, trace.instAct));

	float k = (float)estimatedRulesPerAction;
	float l = (float)rulesForCurrentAction.size();
	float pUeff = l < k ? 1.0f / (k - l) : 1.0f;

	//assertMsg(isProb(pUe), "pUe not a probability");
	//assertMsg(isProb(pUeff), "pUeff not a probability");

	unknownRules[trace.instAct].corroborateFailure(trace.state);

	covMT *= 1.0f - pUe * pUeff;
	covMT = 1.0f - covMT;

	//assertMsg(isProb(covMT), "covMT not a probability");

	vector<Literal> preconds;
	vector<Term> constants;
	map<Literal, float> updatedPrecondNecs;
	map<Term, float> updatedConstNecs;

	// Computing conditional terms and updated necessities
	foreach(rit, rulesForCurrentAction) {
		ActionRule* rule = *rit;

		preconds.clear();
		constants.clear();
		foreach(precPair, rule->precondsNecessities)
			preconds.push_back(precPair->first);
		foreach(constPair, rule->constsNecessities)
			constants.push_back(constPair->first);

		updatedPrecondNecs.clear();
		updatedConstNecs.clear();

		while (preconds.size() > 0 || constants.size() > 0) {
			Literal niL;
			Term niT;
			bool lit;
			float currentNec;
			if (preconds.size() > 0) {
				niL = preconds.back();
				preconds.pop_back();
				lit = true;
				currentNec = rule->precondsNecessities[niL];

				if (currentNec == 0.0f || currentNec == 1.0f || protRTs[rule] == 0.0f || covMT == 0.0f) {
					updatedPrecondNecs[niL] = currentNec;
					continue;
				}
			}
			else {
				niT = constants.back();
				constants.pop_back();
				lit = false;
				currentNec = rule->constsNecessities[niT];

				if (currentNec == 0.0f || currentNec == 1.0f || protRTs[rule] == 0.0f || covMT == 0.0f) {
					updatedConstNecs[niT] = currentNec;
					continue;
				}
			}

			vector<Unverified> filteredNeg, filteredPos;

			foreach(disj, negSigmas[rule]) {
				if ((!lit || !in(disj->first, niL)) && (lit || !in(disj->second, niT)))
					filteredNeg.push_back(*disj);
			}

			foreach(disj, posSigmas[rule]) {
				if ((!lit || !in(disj->first, niL)) && (lit || !in(disj->second, niT)))
					filteredPos.push_back(*disj);
			}
			
			float protRTgivenNk = rule->cdProb(rule->precondsNecessities, rule->constsNecessities, filteredNeg);

			float covRTgivenNk = 1.0f;
			float nCovRTgivenNk = 1.0f;
			vector<Unverified> conditionalCds = filteredNeg;
			foreach(disj, filteredPos) {
				nCovRTgivenNk *= rule->dgcdProb(rule->precondsNecessities, rule->constsNecessities, *disj, conditionalCds);
				conditionalCds.push_back(*disj);
			}
			covRTgivenNk = 1.0f - nCovRTgivenNk;

			float covMTwithoutR = 1.0f - pUe * pUeff;
			foreach(ritt, rulesForCurrentAction)
				if (*ritt != rule)
					covMTwithoutR *= 1.0f - covRTs[*ritt];
			covMTwithoutR = 1.0f - covMTwithoutR;
			
			if (lit) {
				updatedPrecondNecs[niL] = protRTgivenNk * (covRTgivenNk + nCovRTgivenNk * covMTwithoutR) / protRTs[rule] / covMT * rule->precondsNecessities[niL];
				updatedPrecondNecs[niL] = clamp(updatedPrecondNecs[niL], 0.0f, 0.95f);
			}
			else {
				updatedConstNecs[niT] = protRTgivenNk * (covRTgivenNk + nCovRTgivenNk * covMTwithoutR) / protRTs[rule] / covMT * rule->constsNecessities[niT];
				updatedConstNecs[niT] = clamp(updatedConstNecs[niT], 0.0f, 0.95f);
			}
		}

		// Registering updated necessities as new values
		rule->precondsNecessities = updatedPrecondNecs;
		rule->constsNecessities = updatedConstNecs;
	}
}

void BayesianExplorer::informRevision(bool knowledgeRevised) {
	if (knowledgeRevised) {
		revisions++;
		stepsWithoutRevision = 0;
		plan.clear();
	}
	else {
		stepsWithoutRevision++;
	}

	if (lastRevProb != -1.0f) {
		if (knowledgeRevised)
			positiveProbs.push_back(lastRevProb);
		else
			negativeProbs.push_back(lastRevProb);
	}
	else if (knowledgeRevised)
		revsNoProb++;

	if (positiveProbs.size() > 0) {
		posMean = 0.0f;
		foreach(p, positiveProbs)
			posMean += *p;
		posMean /= 1.0f * positiveProbs.size();
	}

	if (negativeProbs.size() > 0) {
		negMean = 0.0f;
		foreach(p, negativeProbs)
			negMean += *p;
		negMean /= 1.0f * negativeProbs.size();
	}

	//cout << "Pos: " << posMean << " - Neg: " << negMean << " - R.R.: " << revsNoProb << " - I.R.: " << positiveProbs.size() << endl;
	
	statsRevProb = lastRevProb;
	statsRevPos = knowledgeRevised;
}

float BayesianExplorer::revisionProbability(State state, Literal action, bool makeTrace) {
	vector<Term> allInsts = instances + domain->getConstants();

	/// <image url="$(SolutionDir)Images/RevisionProbability.PNG" />

	float puExp = computePu(Experiment(state, action));

	map<ActionRule*, float> fulfilmentProbabilities;
	set<ActionRule*> prematchingRules;

	// Pre-computing fulfillment probabilities and prematching rules
	bool prematches = false;
	bool corresponds = false;
	map<ActionRule*, set<Substitution>> subsPerRule;
	set<Substitution> generatedSubs;
	foreach(r, rules) {
		if ((*r)->actionLiteral.pred == action.pred) corresponds = true;
		else continue;
		
		generatedSubs.clear();
		fulfilmentProbabilities[&**r] = (*r)->fulfilmentProbability(state, action, allInsts, prematches, generatedSubs);
		subsPerRule[&**r] = generatedSubs;

		if (prematches)
			prematchingRules.insert(&**r);
	}

	// Computing P(Rev_M,E) as conjunction of U_E and Rev_R,E
	float pRev = 1.0f - puExp;
	map<ActionRule*, pair<bool, float>> resultsPerRule;
	foreach(pair, subsPerRule)
		if (in(prematchingRules, pair->first)) {
			pRev *= fulfilmentProbabilities[pair->first];
			resultsPerRule[pair->first] = { true, fulfilmentProbabilities[pair->first] };
		}
		else {
			pRev *= 1.0f - fulfilmentProbabilities[pair->first];
			resultsPerRule[pair->first] = { false, fulfilmentProbabilities[pair->first] };
		}
	pRev = 1.0f - pRev;

	if (makeTrace) makeMotivationTraceJsonObject(state, action, pRev, subsPerRule, resultsPerRule);

	return pRev;
}

float BayesianExplorer::expectedInformationGain(State state, Literal action) {
	vector<Term> allInsts = instances + domain->getConstants();

	float gain = 0.0f;
	bool dummy;
	Substitution sub;

	map<ActionRule*, float> prs;
	map<ActionRule*, float> nkis;

	float prodPr = 1.0f;
	foreach(rit, rules) {
		shared_ptr<ActionRule> rule = *rit;
		if (rule->actionLiteral.pred != action.pred) continue;

		set<Substitution> subs;
		float pr = rule->fulfilmentProbability(state, action, allInsts, dummy, subs);
		prs[&*rule] = pr;
		prodPr *= (1.0f - pr);

		float sumNki = 0.0f;

		foreach(precond, rule->preconditions)
			if (!state.contains(sub.apply(*precond)))
				sumNki += rule->precondsNecessities[*precond];
		foreach(precond, rule->removedPreconditions)
			if (!state.contains(sub.apply(*precond)))
				sumNki += rule->precondsNecessities[*precond];
		foreach(pair, rule->constsNecessities) {
			Opt<Term> opt = sub.get(pair->first);
			if (opt.there && opt.obj != pair->first)
				sumNki += pair->second;
		}
		nkis[&*rule] = sumNki;
	}

	float puGain = 0.0f;
	float pu = computePu(Experiment(state, action), puGain);
	float pp = 1.0f - (1.0f - pu) * prodPr;

	foreach(pair, prs) {
		float pr = pair->second;
		float snki = nkis[pair->first];

		float ppNki = 1.0f - pu;
		foreach(otherPair, prs)
			if (otherPair->first != pair->first)
				ppNki *= (1.0f - otherPair->second);
		ppNki = 1.0f - ppNki;

		gain += snki * (pr * abs(1.0f - ppNki / pp) + (1.0f - pr) * abs(1.0f - (1.0f - ppNki)/(1.0f - pp)));
	}

	return gain;
}

// 0: no meta-action	1: reset	2: delete
int BayesianExplorer::metaActionType() {
	if (static_cast<float>(rand()) / static_cast<float>(RAND_MAX) >= metaProbability) return 0;
	float deleteProb = (1.0f - bayesianConfig->getFloat("base_reset_prob")) / (1.0f + (float)deletedInstances.size());
	if (deletedInstances.size() < instances.size() && static_cast<float>(rand()) / static_cast<float>(RAND_MAX) < deleteProb) return 2;
	return 1;
}

void BayesianExplorer::generateRandomPlan(State state) {
	chrono::steady_clock::time_point startTime = chrono::high_resolution_clock::now();

	vector<Term> allInsts = instances + domain->getConstants();
	plan.clear();
	revisionProbs.clear();

	set<Literal> experiments = getAvailableExperiments(deletedInstances, state);
	Literal experiment;

	if (experiments.size() == 0) {
		experiment = domain->getActionPredByName("reset")();
		plan.push_back(experiment);
		revisionProbs.push_back(-1.0f);
		return;
	}
	else {
		experiment = *select_randomly(experiments.begin(), experiments.end());
		switch (metaActionType()) {
		case 0:
			break;
		case 1:
			experiment = domain->getActionPredByName("reset")();
			break;
		case 2:
			set<Term> notDeleted = getNotDeleted();
			Term toDelete = *select_randomly(notDeleted.begin(), notDeleted.end());
			experiment = domain->getActionPredByName("delete")(toDelete);
			break;
		}
		plan.push_back(experiment);
		revisionProbs.push_back(-1.0f);
	}

	// If random action, we don't look for a plan and return sampled action right away
	if (random || static_cast<float>(rand()) / static_cast<float>(RAND_MAX) < powf(randomDiscount, (float)revisions))
		return;

	if (stepsWithoutRevision > stagnationThreshold && useStagnation) {
		stepsWithoutRevision = 0;
		return;
	}

	float bestPlanUtility = revisionProbability(state, experiment);
	float currentUtility;
	float noRevisionProb;

	vector<Literal> currentPlan;
	vector<float> currentRevProbs;

	Predicate removeFactPred = domain->getActionPredByName("remove-fact");

	set<Predicate> mostSpecificRulesPredicates;
	float meanSpecif = 0.0f;
	foreach(rit, rules)
		meanSpecif += (*rit)->specificity() * 1.0f;
	meanSpecif /= (float)rules.size();
	foreach(rit, rules)
		if ((*rit)->specificity() * 1.0f > 0.5f * meanSpecif)
			mostSpecificRulesPredicates.insert((*rit)->actionLiteral.pred);
	bool limitToSpecifics = static_cast<float>(rand()) / static_cast<float>(RAND_MAX) < focusSpecificRules;

	bool foundBetterThanRandom = false;

	for (int p = 0; p < randomPlans; p++) {
		chrono::duration<double, std::milli> deltaTime = chrono::high_resolution_clock::now() - startTime;
		if (deltaTime.count() / 1000.0f > explorationTimeLimit) break;

		currentPlan.clear();
		currentRevProbs.clear();

		State currentState = state;
		set<Term> newDeleted = deletedInstances;
		currentUtility = 0.0f;
		noRevisionProb = 1.0f;

		for (int a = 0; a < planDepth; a++) {
			chrono::duration<double, std::milli> deltaTime = chrono::high_resolution_clock::now() - startTime;
			if (deltaTime.count() / 1000.0f > explorationTimeLimit) break;

			log << "\r" << "Best heuristic: " << bestPlanUtility << " - Steps: " << plan.size() << " - " << p * planDepth + a << "       ";

			// 1. Perform RANDOM_EXPERIMENTS experiments and check learning utility
			set<Literal> experiments;
			if (limitToSpecifics)
				experiments = getAvailableExperiments(newDeleted, currentState, mostSpecificRulesPredicates);
			else
				experiments = getAvailableExperiments(newDeleted, currentState);

			for (int e = 0; e < randomExperiments; e++) {
				chrono::duration<double, std::milli> deltaTime = chrono::high_resolution_clock::now() - startTime;
				if (deltaTime.count() / 1000.0f > explorationTimeLimit) break;
				
				bool removeFact = static_cast<float>(rand()) / static_cast<float>(RAND_MAX) > powf(factRemovalDiscount, (float)revisions);
				Literal toRemove = *select_randomly(currentState.facts.begin(), currentState.facts.end());

				if (experiments.size() == 0) break;
				Literal experiment = *select_randomly(experiments.begin(), experiments.end());
				//experiments.erase(experiment);

				if (removeFact) {
					vector<shared_ptr<ActionRule>> matchingRules;
					foreach(rit, rules)
						if ((*rit)->actionLiteral.unifies(experiment))
							matchingRules.push_back(*rit);

					if (matchingRules.size() > 0) {
						shared_ptr<ActionRule> rule = *select_randomly(matchingRules.begin(), matchingRules.end());
						Literal precondToRemove = *select_randomly(rule->preconditions.begin(), rule->preconditions.end());
						vector<Term> toRemoveParams;

						bool shouldReplace = true;

						foreachindex(pi, precondToRemove.parameters) {
							Term p = precondToRemove.parameters[pi];
							bool found = false;

							if (p.isVariable) {
								foreachindex(rpi, rule->actionLiteral.parameters)
									if (rule->actionLiteral.parameters[rpi] == p) {
										found = true;
										p = experiment.parameters[rpi];
										break;
									}
								if (!found) {
									Term inst = *select_randomly(allInsts.begin(), allInsts.end());
									p = inst;
								}
							}

							toRemoveParams.push_back(p);
						}

						Literal replaceRemove = Literal(precondToRemove.pred, toRemoveParams);

						if (shouldReplace || replaceRemove.grounded())
							toRemove = replaceRemove;
					}
				}

				vector<Literal> expPlan = currentPlan;

				if (removeFact)
					expPlan.insert(expPlan.begin(), Literal(removeFactPred, { Instance(toRemove.toString(), 0) }));
				expPlan.insert(expPlan.begin(), experiment);
				if (removeFact)
					expPlan.insert(expPlan.begin(), Literal(removeFactPred, vector<Term>{}));

				State expState = State(currentState);
				if (removeFact)
					expState.removeFact(toRemove);

				float pRev = revisionProbability(expState, experiment);
				//float pRev = expectedInformationGain(expState, experiment);

				//float utility = currentUtility + powf((float)GAMMA, a * 1.0f + 1.0f) * noRevisionProb * pRev;
				//float utility = currentUtility + powf((float)GAMMA, a * 1.0f + 1.0f) * pRev;
				float utility = powf(gamma, a * 1.0f + 1.0f) * pRev;

				if (bestPlanUtility == utility && expPlan.size() < plan.size()) {
					plan = expPlan;
					revisionProbs = vector<float>{ pRev } + currentRevProbs;

					foreachindex(i, expPlan) {
						debug << expPlan[expPlan.size() - i - 1];
						if (i < expPlan.size() - 1) debug << "->";
					}
					debug << " - Utility: " << utility << endl;

					foundBetterThanRandom = true;
				}
				else if (bestPlanUtility < utility) {
					bestPlanUtility = utility;
					plan = expPlan;
					revisionProbs = vector<float>{ pRev } + currentRevProbs;

					foreachindex(i, expPlan) {
						debug << expPlan[expPlan.size() - i - 1];
						if (i < expPlan.size() - 1) debug << "->";
					}
					debug << " - Utility: " << utility << endl;

					foundBetterThanRandom = true;
				}
			}

			if (rules.size() == 0) break;
			if (bestPlanUtility >= passthroughThreshold && usePassthrough) break;

			Literal chosenAction;
			float pRev = -1.0f;
			Opt<State> nextState;

			// 2. If first action of plan, and still below META_ACTION_PLANS, perform a meta-action
			int metaAction = metaActionType();
			if (a == 0 && metaAction > 0) {
				switch (metaAction) {
				case 1:
					newDeleted.clear();

					chosenAction = domain->getActionPredByName("reset")();
					nextState = domain->tryAction(currentState, instances, chosenAction, false);
					break;
				case 2:
					set<Term> notDeleted = getNotDeleted();
					Term toDelete = *select_randomly(notDeleted.begin(), notDeleted.end());
					newDeleted.insert(toDelete);

					chosenAction = domain->getActionPredByName("delete")(toDelete);
					nextState = domain->tryAction(currentState, instances, chosenAction, false);
					break;
				}
			}

			// 3. Else, perform a random action
			else {
				unsigned int trials = randomActionTrials;
				vector<Literal> selectFrom;
				foreach(pred, actionPredicates) {
					bool covered = false;
					foreach(rit, rules)
						if ((*rit)->actionLiteral.pred == *pred) {
							covered = true;
							break;
						}
					if (covered)
						foreach(lit, actionLiterals)
							if (lit->pred == *pred)
								selectFrom.push_back(*lit);
				}

				while (!nextState.there && trials > 0) {
					trials--;
					chosenAction = *select_randomly(selectFrom.begin(), selectFrom.end());
					nextState = domain->tryAction(currentState, instances, chosenAction, false);
				}

				pRev = revisionProbability(currentState, chosenAction);
				//pRev = expectedInformationGain(currentState, chosenAction);
				//currentUtility += powf((float)GAMMA, a * 1.0f + 1.0f) * noRevisionProb * pRev;
				//currentUtility += powf((float)GAMMA, a * 1.0f + 1.0f) * pRev;
				noRevisionProb *= 1 - pRev;
			}

			if (!nextState.there) break;
			currentPlan.insert(currentPlan.begin(), chosenAction);
			currentRevProbs.insert(currentRevProbs.begin(), pRev);

			currentState = nextState.obj;
		}

		if (rules.size() == 0) break;
		if (bestPlanUtility >= passthroughThreshold && usePassthrough) break;
	}

	if (foundBetterThanRandom && plan.size() == 1 && saveMotivationTrace) {
		revisionProbability(state, plan[0], true);
	}

	if (stepsWithoutRevision > stagnationThreshold && useStagnation) {
		stepsWithoutRevision = 0;
		log << "ESCAPING CURRENT STATE" << endl;
	}
}

void BayesianExplorer::prepareActionSubstitutions() {
	allActions.clear();
	vector<Action> actions = domain->getActions(true);
	vector<Term> allInsts = instances + domain->getConstants();
	foreach(act, actions) {
		vector<Substitution> subs = Substitution().expandUncovered(act->actionLiteral.parameters, allInsts, true);
		foreach(sub, subs)
			insertUnique(&allActions, sub->apply(act->actionLiteral));
	}
}

set<Literal> BayesianExplorer::getAvailableExperiments(set<Term> newDeleted, State state, set<Predicate> actionPreds) {
	set<Literal> available;
	bool valid;
	foreach(lit, actionLiterals) {
		if (!in(actionPreds, lit->pred)) continue;
		if (in(allExperiments, Experiment(state, *lit))) continue;

		valid = true;
		foreach(param, lit->parameters)
			if (in(newDeleted, *param)) {
				valid = false;
				break;
			}

		if (valid)
			available.insert(*lit);
	}
	return available;
}

set<Literal> BayesianExplorer::getAvailableExperiments(set<Term> newDeleted, State state) {
	return getAvailableExperiments(newDeleted, state, actionPredicates);
}

set<Term> BayesianExplorer::getNotDeleted() {
	set<Term> notDeleted;
	vector<Term> allInsts = instances + domain->getConstants();
	foreach(inst, allInsts)
		if (!in(deletedInstances, *inst))
			notDeleted.insert(*inst);
	return notDeleted;
}

string jsonStr(string str) {
	return "\"" + str + "\"";
}

string jsonField(string key, string val) {
	return jsonStr(key) + ":" + val;
}

string jsonArr(vector<string> vals) {
	return "[" + join(",", vals) + "]";
}

string jsonLit(Literal lit, float necessity = -1.0f) {
	vector<string> elems;
	elems.push_back(jsonStr((lit.positive ? "" : "-") + lit.pred.name));
	vector<string> params;
	foreach(p, lit.parameters)
		params.push_back(jsonStr(p->name));
	elems.push_back(jsonArr(params));
	if (necessity >= 0.0f)
		elems.push_back(to_string(necessity));
	return jsonArr(elems);
}

string jsonSub(Substitution sub) {
	vector<string> subs;
	map<Term, Term> mapping = sub.getMapping();
	foreach(pair, mapping)
		subs.push_back("[" + jsonStr(pair->first.name) + "," + jsonStr(pair->second.name) + "]");
	return jsonArr(subs);
}

void BayesianExplorer::makeMotivationTraceJsonObject(State state, Literal action, float revisionProb,
	map<ActionRule*, set<Substitution>> subsPerRule,
	map<ActionRule*, pair<bool, float>> resultsPerRule) {

	string object = "{";

	// Make JSON object
	vector<string> stateFacts;
	foreach(f, state.facts)
		stateFacts.push_back(jsonLit(*f));
	object += jsonField("state", jsonArr(stateFacts)) + ",";
	object += jsonField("action", jsonLit(action)) + ",";
	object += jsonField("revision", to_string(revisionProb)) + ",";
	
	vector<string> rulesObjs;
	foreach(pair, subsPerRule) {
		string ruleObj = "{";
		ActionRule* rule = pair->first;

		vector<string> preconds;
		vector<string> removedPreconds;
		foreach(precNec, rule->precondsNecessities)
			if (in(rule->preconditions, precNec->first))
				preconds.push_back(jsonLit(precNec->first, precNec->second));
			else
				removedPreconds.push_back(jsonLit(precNec->first, precNec->second));

		vector<string> constants;
		foreach(cstNec, rule->constsNecessities)
			constants.push_back("[" + jsonStr(cstNec->first.name) + "," + to_string(cstNec->second) + "]");

		vector<string> effects;
		foreach(a, rule->add)
			effects.push_back(jsonLit(*a));
		foreach(d, rule->del)
			effects.push_back(jsonLit(*d));

		ruleObj += jsonField("preconditions", jsonArr(preconds)) + ",";
		ruleObj += jsonField("removed_preconditions", jsonArr(removedPreconds)) + ",";
		ruleObj += jsonField("constants", jsonArr(constants)) + ",";

		ruleObj += jsonField("action", jsonLit(rule->actionLiteral)) + ",";
		ruleObj += jsonField("effects", jsonArr(effects)) + ",";

		ruleObj += jsonField("prematching", resultsPerRule[rule].first ? "true" : "false") + ",";
		ruleObj += jsonField("fulfilment", to_string(resultsPerRule[rule].second)) + ",";

		vector<string> substitutions;
		foreach(sub, pair->second)
			substitutions.push_back(jsonSub(*sub));
		ruleObj += jsonField("substitutions", jsonArr(substitutions));
		
		ruleObj += "}";
		rulesObjs.push_back(ruleObj);
	}
	object += jsonField("rules", jsonArr(rulesObjs));

	object += "}";
	motivationTraceObjects.push_back(object);
}

void BayesianExplorer::saveMotivationTraceFile() {
	string content = "[\n" + join(",\n", motivationTraceObjects) + "\n]";

	ofstream motivationFile;
	motivationFile.open("stats/" + motivationTraceFileName + ".json");
	motivationFile << content;
	motivationFile.close();
}
