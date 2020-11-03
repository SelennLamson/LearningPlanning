/**
 * @author Thomas Lamson
 * Contact: thomas.lamson@cea.fr
 *
 * This agent has access to the domain definition as any other agent, but it will not rely on it and will rather
 * try to learn it from traces and random execution of actions.
 *
 * Based on paper: "Incremental Learning of Relational Action Rules", Christophe Rodrigues, Pierre Gérard,
 *                 Céline Rouveirol, Henry Soldano, https://doi.org/10.1109/ICMLA.2010.73
 *
 * Openly available here: https://lipn.univ-paris13.fr/~rodrigues/icmla2010PP.pdf
 */

#include "Agents/LearningAgent/LearningAgent.h"
#include "Agents/ManualAgent.h"
#include "Utils.h"
#include "Logic/LogicEngine.h"

#include <ctime>
#include <iomanip>
#include <sstream>
#include <iostream>

bool checkSelfParenting(shared_ptr<ActionRule> current, shared_ptr<ActionRule> check) {
	foreach(parent, current->parents)
		if (*parent == check || checkSelfParenting(*parent, check)) return true;
	return false;
}

shared_ptr<Domain> domainFromRules(shared_ptr<Domain> initialDomain, set<shared_ptr<ActionRule>> edsRules) {
	vector<Action> initialActions = initialDomain->getActions();
	vector<Action> finalActions;

	foreach(rit, edsRules) {
		vector<Literal> truePreconds, falsePreconds, add, del;

		foreach(precond, (*rit)->preconditions)
			truePreconds.push_back(*precond);

		foreach(addEff, (*rit)->add)
			add.push_back(*addEff);

		foreach(delEff, (*rit)->del)
			del.push_back(*delEff);

		finalActions.push_back(Action((*rit)->actionLiteral, truePreconds, falsePreconds, add, del));
	}

	shared_ptr<Domain> newDomain = make_shared<Domain>(initialDomain->getTypes(), initialDomain->getPredicates(), initialDomain->getConstants(), finalActions);
	newDomain->removedFacts = initialDomain->removedFacts;
	return newDomain;
}

float computeVarDistBetweenDomains(shared_ptr<Domain> domain, set<shared_ptr<ActionRule>> rules) {
	vector<Action> actions = domain->getActions();
	int total = 0;

	foreach(act, actions) {
		int minDist = 100;

		foreach(rit, rules) {
			shared_ptr<ActionRule> rule = *rit;
			if (rule->actionLiteral.pred != act->actionLiteral.pred) continue;

			Substitution sigma;
			foreachindex(pi, act->actionLiteral.parameters) {
				Term sourceParam = act->actionLiteral.parameters[pi];
				Term targetParam = rule->actionLiteral.parameters[pi];

				if (sourceParam.isVariable) {
					sigma.set(sourceParam, targetParam);
				}
			}

			vector<Substitution> sigmaTheta = sigma.expandUncovered(act->parameters, rule->parameters, true);
			foreach(st, sigmaTheta) {
				Substitution sub = *st;
				Substitution invSub = sub.inverse();
				int dist = 0;

				bool notSameEffects = false;
				foreach(addeff, act->add)
					if (!in(rule->add, sub.apply(*addeff)))
						notSameEffects = true;
				if (notSameEffects) continue;
				foreach(deleff, act->del)
					if (!in(rule->del, sub.apply(*deleff)))
						notSameEffects = true;
				if (notSameEffects) continue;
				foreach(addeff, rule->add)
					if (!in(act->add, invSub.apply(*addeff)))
						notSameEffects = true;
				if (notSameEffects) continue;
				foreach(deleff, rule->del)
					if (!in(act->del, invSub.apply(*deleff)))
						notSameEffects = true;
				if (notSameEffects) continue;

				foreach(precond, act->truePrecond) {
					Literal subPrecond = sub.apply(*precond);
					if (!in(rule->preconditions, subPrecond))
						dist++;
				}
				foreach(precond, rule->preconditions) {
					Literal subPrecond = invSub.apply(*precond);
					if (!in(act->truePrecond, subPrecond))
						dist++;
				}

				if (minDist > dist)
					minDist = dist;
			}
		}

		total += minDist;
	}

	return total * 1.0f / (actions.size() * 1.0f);
}

LearningAgent::LearningAgent(bool verbose) : Agent(verbose) {
	iraleConfig = config->getSubconfig("irale");

	/*
		"irale": {
			"runs": 5,
			"steps": 1200,
			"reset_state_after_stagnation": false,
			"reset_state_after": 30,
			"test_domain_every": 30,
			"test_only_when_knowledge_modified": true,
			"always_generalize_constants": false,
			"use_bayesian_explorer": true
		},
	*/
}

void LearningAgent::init(shared_ptr<Domain> inDomain, vector<Term> inInstances, Goal inGoal, shared_ptr<vector<Trace>> inTrace) {
	Agent::init(inDomain, inInstances, inGoal, inTrace);
	setupInternalPlanner();

	startTime = chrono::high_resolution_clock::now();

	runs = iraleConfig->getInt("runs");
	steps = iraleConfig->getInt("steps");

	if (!stats) {
		stats = new float** [runs];
		for (int r = 0; r < runs; r++) {
			stats[r] = new float* [steps];
			for (int s = 0; s < steps; s++) {
				stats[r][s] = new float[7];
				for (int c = 0; c < 7; c++)
					stats[r][s][c] = 0.0f;
			}
		}
	}
	else
		for (int r = 0; r < runs; r++)
			for (int s = 0; s < steps; s++)
				for (int c = 0; c < 7; c++)
					stats[r][s][c] = 0.0f;

	rules.clear();
	counterExamples.clear();
	failedActionsCounterExamples.clear();
	failedBeforeFirstSuccess.clear();
	learning = true;
	step = 0;
	revisedSinceLastEval = true;
	lastRevisionStep = 0;
}

Literal LearningAgent::getNextAction(State state) {
	step++;

	if (step % 100 == 0) {
		cout << endl << "-----------------------------------------------------------------" << endl;
		cout << "Current knowledge. Rules: " << rules.size()
			<< " - Examples: " << counterExamples.size()
			<< " - Failed Actions Examples: " << failedActionsCounterExamples.size()
			<< endl;

		cout << endl;
		foreach(rit, rules)
			cout << **rit << endl << endl;
		cout << endl;
	}

	chrono::duration<double, std::milli> deltaTime = chrono::high_resolution_clock::now() - startTime;
	float progress = (run * steps + step) * 1.0f / (runs * steps * 1.0f);
	int percent = (int)(progress * 100.0f);
	int msEta = (int)(deltaTime.count() * 1.0f / progress * (1.0f - (double)progress));

	float averageUncertainty = 0.0f;
	float uncertainty, nec, nki;
	foreach(rit, rules) {
		shared_ptr<ActionRule> rule = *rit;
		uncertainty = 0.0f;
		nki = 0.0f;
		foreach(precond, rule->preconditions) {
			nec = rule->precondsNecessities[*precond];

			uncertainty += nec < 0.5f ?
						   nec / 0.5f :
						   1 - (nec - 0.5f) / (1 - 0.5f);
			nki++;
		}
		foreach(precond, rule->removedPreconditions) {
			nec = rule->precondsNecessities[*precond];

			uncertainty += nec < 0.5f ?
				nec / 0.5f :
				1 - (nec - 0.5f) / (1 - 0.5f);
			nki++;
		}
		foreach(pair, rule->constsNecessities) {
			nec = pair->second;

			uncertainty += nec < 0.5f ?
				nec / 0.5f :
				1 - (nec - 0.5f) / (1 - 0.5f);
			nki++;
		}
		uncertainty /= nki;
		averageUncertainty += uncertainty;
	}
	if (rules.size() > 0)
		averageUncertainty /= rules.size() * 1.0f;

	cout << "\rRun: " << run << " - Step: " << trace->size() % steps << " - Counter-examples: " << counterExamples.size() + failedActionsCounterExamples.size() << " - Progress: " << percent << "% - ETA: " << msEta / 1000 << "s - Avg Uncertainty: " << averageUncertainty << "                      ";
	if (run < runs) {
		if (trace->size() % steps != 0) {
			stats[run][trace->size() % steps][0] = (float)(counterExamples.size() + failedActionsCounterExamples.size());

			float specifSum = 0.0f;
			foreach(r, rules)
				specifSum += (*r)->specificity() * 1.0f;

			stats[run][trace->size() % steps][1] = specifSum / rules.size();

			if (learner) {
				stats[run][trace->size() % steps][2] = learner->statsRevProb;
				stats[run][trace->size() % steps][3] = learner->statsRevPos ? 1.0f : 0.0f;
			}
			else {
				stats[run][trace->size() % steps][2] = -1.0f;
				stats[run][trace->size() % steps][3] = 0.0f;
			}

			if (trace->size() % steps % iraleConfig->getInt("test_domain_every") == 0) {
				if (revisedSinceLastEval || !iraleConfig->getBool("test_only_when_knowledge_modified")) {
					domainTester->testDomain(domainFromRules(domain, rules), prevVarDist, prevPlanDist);
				}
				stats[run][trace->size() % steps][4] = prevVarDist;
				stats[run][trace->size() % steps][6] = prevPlanDist;
				revisedSinceLastEval = false;
			}
			else {
				stats[run][trace->size() % steps][4] = -1.0f;
				stats[run][trace->size() % steps][6] = -1.0f;
			}

			stats[run][trace->size() % steps][5] = computeVarDistBetweenDomains(domain, rules);
		}

		if (trace->size() % steps == 0) {
			revisedSinceLastEval = true;
			lastRevisionStep = 0;

			if (trace->size() > 0)
				run++;

			rules.clear();
			counterExamples.clear();
			failedActionsCounterExamples.clear();
			failedBeforeFirstSuccess.clear();
			step = 0;

			setupInternalPlanner();

			if ((int)(trace->size()) - steps > 0)
				trace->erase(trace->begin(), trace->begin() + steps);
			assert(trace->size() % steps == 0 && trace->size() >= 0);

			engine->setRandomState();
			state = engine->currentState;
		}
	}
	else if (run == runs) {
		auto t = time(nullptr);
		tm tm;
		localtime_s(&tm, &t);

		int fileIndex = 1;
		bool foundFreeFilename = false;
		while (!foundFreeFilename) {
			ostringstream oss;
			oss << "Stats/";
			oss << config->getString("outputfile");
			if (fileIndex > 1)
				oss << "_" << fileIndex;
			oss << ".csv";

			FILE* file;
			if (fopen_s(&file, oss.str().c_str(), "r") == 0) fclose(file);
			else {
				foundFreeFilename = true;
				statsFile.open(oss.str());
			}

			fileIndex++; 
		}

		for (int i = 0; i < runs; i++)
			foreach(col, columns)
				statsFile << *col << "_" << i << ",";
		statsFile << endl;

		for (int s = 0; s < steps; s++) {
			for (int r = 0; r < runs; r++)
				foreachindex(col, columns)
					statsFile << stats[r][s][col] << ",";
			statsFile << endl;
		}

		statsFile.close();
		while (true) {
			cout << "\rEND                                                                                ";
		}
	}

	// Update the knowledge based on the last trace
	bool knowledgeModified = false;
	if (trace->size() > 0) {
		Trace tr = trace->back();

		if (learner)
			learner->corroborateRules(tr);

		Predicate pred = tr.instAct.pred;
		string predName = pred.name;
		if (predName != "reset" && predName != "delete" && predName != "remove-fact") {
			
			// If action failed and was unknown yet, remember failure for later initialization
			if (!tr.authorized) {
				bool foundRule = false;
				foreach(rit, rules)
					if ((*rit)->actionLiteral.pred == pred) {
						foundRule = true;
						break;
					}
				if (!foundRule) {
					if (in(failedBeforeFirstSuccess, pred)) failedBeforeFirstSuccess[pred].push_back(tr);
					else failedBeforeFirstSuccess[pred] = { tr };
				}
			}

			knowledgeModified = updateKnowledge(trace->back());
		}
	}

	if (knowledgeModified) {
		revisedSinceLastEval = true;
		lastRevisionStep = step;
		updateInternalPlanner();
		learner->informRevision(true);
	}
	else {
		learner->informRevision(false);
	}

	if (step < headstartActions.size() + 1 && headstartActions.size() > 0) {
		cout << "Headstart: " << headstartActions[(int)step - 1] << endl;
		return headstartActions[(int)step - 1];
	}

	if (learning) {
		if (step - lastRevisionStep >= iraleConfig->getInt("reset_state_after") && iraleConfig->getBool("reset_state_after_stagnation")) {
			lastRevisionStep = step;

			learner->plan.clear();

			return domain->getActionPredByName("reset")();
		}

		Literal act = learner->getNextAction(state);

		return act;
	}
	else {
		Literal act = planner->getNextAction(state);
		return act;
	}
	
	return Literal();
}

void LearningAgent::updateProblem(vector<Term> inInstances, Goal inGoal, vector<Literal> inHeadstart) {
	Agent::updateProblem(inInstances, inGoal, inHeadstart);
	setupInternalPlanner();
	step = 0;
}

bool LearningAgent::updateKnowledge(Trace trace) {
	assert(trace.instAct.grounded());

	foreach(param, trace.instAct.parameters)
		if (in(trace.state.facts, Literal(domain->getActionPredByName("delete"), { *param })))
			return false;

	shared_ptr<ActionRule> example = make_shared<ActionRule>(trace, startPu, true);
	bool modified = false;

	if (verbose) {
		int totalPreconds = 0, totalConsts = 0;
		foreach(rit, rules) {
			totalPreconds += (int)(*rit)->preconditions.size();
			set<Term> consts;
			foreach(param, (*rit)->parameters)
				if (!param->isVariable)
					consts.insert(*param);
			totalConsts += (int)consts.size();
		}

		cout << endl << "-----------------------------------------------------------------" << endl;
		cout << "Updating knowledge. Rules: " << rules.size()
			 << " - Examples: " << counterExamples.size()
			 << " - Failed: " << failedActionsCounterExamples.size()
			 << " - Preconds: " << totalPreconds
			 << " - Consts: " << totalConsts
			 << endl;

		cout << endl;
		foreach(rit, rules)
			cout << **rit << endl << endl;
		cout << endl;
	}

	// Checking coverage and consistency
	vector<shared_ptr<ActionRule>> prematching;
	vector<shared_ptr<ActionRule>> contradiction;
	foreach(it, rules) {
		set<Substitution> prematchSubs = (*it)->prematchingSubs(example);

		if (prematchSubs.size() > 0) {
			prematching.push_back(*it);

			if (trace.authorized)
				foreach(subit, prematchSubs)
					if (!(*it)->postmatches(example, *subit))
						contradiction.push_back(*it);
		}
	}
	vector<shared_ptr<ActionRule>> coverage;
	if (trace.authorized)
		foreach(it, prematching)
			if (!in(contradiction, *it))
				coverage.push_back(*it);

	// prematching = rules that prematch example
	// contradiction = rules that prematch example with a substitution sigmaTheta without postmatching it for the same substitution
	// coverage = rules that prematch example with a substitution sigmaTheta and postmatches it for the same substitution
	
	set<shared_ptr<ActionRule>> uncoveredExamples;
	if (prematching.size() == 0 && trace.authorized) {
		log << "Nothing covered example." << endl;
		modified = true;

		counterExamples.insert(example);
		uncoveredExamples.insert(example);
	}

	if (prematching.size() > 0 && !trace.authorized) {
		log << "Rules covered an example in which action failed. SPECIALIZING" << endl;
		modified = true;

		failedActionsCounterExamples.insert(example);
		
		foreach(rit, prematching) {
			set<shared_ptr<ActionRule>> newUncovered = specialize(*rit, example);
			foreach(uncovit, newUncovered) {
				foreach(rit, rules)
					(*rit)->removeParentRecursive(*uncovit);
				uncoveredExamples.insert(*uncovit);
			}
		}

		log << "Specialization over. Uncovered examples: " << uncoveredExamples.size() << endl;
	}
	else if (contradiction.size() > 0) {
		// At least one rule contradicts the example, SPECIALIZE

		log << "At least one rule contradicted example. SPECIALIZING" << endl;
		modified = true;

		counterExamples.insert(example);

		foreach(rit, contradiction)
			uncoveredExamples = uncoveredExamples + specialize(*rit, example);

		log << "Specialization over. Uncovered examples: " << uncoveredExamples.size() << endl;
	}

	foreach(exampleit, uncoveredExamples) {
		// No rules prematches current example, GENERALIZE
		
		log << "Generalizing example..." << endl;

		generalize(*exampleit);
	}

	log << "End of knowledge update." << endl;
	
	return modified;
}

set<shared_ptr<ActionRule>> LearningAgent::specialize(shared_ptr<ActionRule> rule, shared_ptr<ActionRule> example) {
	rules.erase(rule);

	set<shared_ptr<ActionRule>> uncovered;

	foreach(parentit, rule->parents) {
		shared_ptr<ActionRule> parent = *parentit;
		if (parent->parents.size() == 0) {
			uncovered.insert(parent);
			rules.erase(parent);
		}
		else if (parent->contradicts(example))
			uncovered = uncovered + specialize(parent, example);
	}

	return uncovered;
}

void LearningAgent::generalize(shared_ptr<ActionRule> example) {
	// Trying to cover example with another rule (looking for the least-general (=most-specific) ones)
	log << "GENERALIZING - STEP 1 - Testing coverage" << endl;
	int leastGeneralityLevel = -1;
	set<shared_ptr<ActionRule>> leastGeneralRules;
	foreach(rit, rules) {
		shared_ptr<ActionRule> leastGeneralRule = (*rit)->getLeastGeneralRuleCovering(example);

		if (leastGeneralRule) {
			int genLevel = leastGeneralRule->generalityLevel();

			if (genLevel < leastGeneralityLevel || leastGeneralityLevel == -1) {
				leastGeneralityLevel = genLevel;
				leastGeneralRules = { leastGeneralRule };
			}
			else if (genLevel == leastGeneralityLevel) {
				leastGeneralRules.insert(leastGeneralRule);
			}
		}
	}

	// Adding example to the parents of each least general rule identified
	// TODO : find why some rules are added as parents to themselves, fix it and then remove assertion
	foreach(rit, leastGeneralRules) {
		if (*rit == example) {
			cout << "ERROR: SAME POINTER" << endl;
			continue;
		}
		(*rit)->insertParent(example);
		assert(!checkSelfParenting(*rit, *rit));
	}

	bool recovered = leastGeneralRules.size() > 0;

	// Trying to generalize current rules to cover example
	if (!recovered) {
		log << "GENERALIZING - STEP 2 - Computing generalizations" << endl;

		set<shared_ptr<ActionRule>> currentRules = set<shared_ptr<ActionRule>>(rules);
		foreach(rit, currentRules) {
			Substitution subr, subx;
			shared_ptr<ActionRule> rule = *rit;

			set<Term> genVars;
			bool pg = rule->postGeneralizes(example, subr, subx, genVars);

			log << "Rule " << (pg ? "post-generalizes" : "doesn't post-generalize") << " example." << endl;

			if (pg) {
				int trials = iraleConfig->getInt("generalization_trials");
				shared_ptr<ActionRule> lggRule = nullptr;

				while (trials-- > 0) {
					set<Term> genVarsTrial = genVars;
					Substitution subrTrial = subr;
					Substitution subxTrial = subx;

					log << "Substitutions: " << subrTrial << " - " << subxTrial << endl;
					log << "Post-generalized preconds: " << join(subrTrial.inverse().apply(rule->preconditions)) << endl;
					log << "Post-generalized example: " << join(subxTrial.inverse().apply(example->preconditions)) << endl;

					set<Literal> genPreconds = rule->anyGeneralization(example, subrTrial, subxTrial, genVarsTrial);

					log << "Found generalization: " << join(genPreconds) << endl;
					log << "New substitutions: " << subrTrial << " - " << subxTrial << endl;

					auto mappingR = subrTrial.getMapping();
					foreach (pair, mappingR) {
						if (subxTrial.get(pair->first).obj == pair->second) {
							genPreconds = Substitution({ pair->first }, { pair->second }).apply(genPreconds);
							subrTrial.remove(pair->first);
							subxTrial.remove(pair->first);
						}
						if (pair->first == pair->second)
							subrTrial.remove(pair->first);
					}

					set<Literal> removedPreconds;
					map<Literal, vector<float>> precondsNecessitiesList;
					map<Term, vector<float>> constsNecessitiesList;

					Substitution invSubR = subrTrial.inverse();
					Substitution invSubX = subxTrial.inverse();
					foreach(pair, rule->precondsNecessities) {
						Literal genVersion = invSubR.apply(pair->first);

						if (!in(genPreconds, genVersion)) {
							removedPreconds.insert(genVersion);
							//removedPreconds.insert(pair->first);
							//genVersion = pair->first;
						}

						if (in(precondsNecessitiesList, genVersion)) precondsNecessitiesList[genVersion].push_back(pair->second);
						else precondsNecessitiesList[genVersion] = { pair->second };

					}
					foreach(pair, example->precondsNecessities) {
						Literal genVersion = invSubX.apply(pair->first);

						if (!in(genPreconds, genVersion)) {
							removedPreconds.insert(genVersion);
							//removedPreconds.insert(pair->first);
							//genVersion = pair->first;
						}

						if (in(precondsNecessitiesList, genVersion)) precondsNecessitiesList[genVersion].push_back(pair->second);
						else precondsNecessitiesList[genVersion] = { pair->second };
					}
					foreach(pair, rule->constsNecessities)
						if (invSubR.apply(pair->first) == pair->first) {
							if (in(constsNecessitiesList, pair->first)) constsNecessitiesList[pair->first].push_back(pair->second);
							else constsNecessitiesList[pair->first] = { pair->second };
						}
					foreach(pair, example->constsNecessities)
						if (invSubX.apply(pair->first) == pair->first) {
							if (in(constsNecessitiesList, pair->first)) constsNecessitiesList[pair->first].push_back(pair->second);
							else constsNecessitiesList[pair->first] = { pair->second };
						}

					/*foreach(prec, removedPreconds)
						assertMsg(prec->grounded(), "Removed precondition is not grounded.");*/

					map<Literal, float> precondsNecessities;
					map<Term, float> constsNecessities;
					foreach(pair, precondsNecessitiesList) {
						float mean = 0.0f;
						foreach(val, pair->second)
							mean += *val;
						if (mean <= 0.01f && !in(genPreconds, pair->first)) {
							removedPreconds.erase(pair->first);
							continue;
						}
						
						precondsNecessities[pair->first] = mean / (float)pair->second.size();

					}
					foreach(pair, constsNecessitiesList) {
						float mean = 0.0f;
						foreach(val, pair->second)
							mean += *val;
						constsNecessities[pair->first] = mean / (float)pair->second.size();
					}

					shared_ptr<ActionRule> genRule = make_shared<ActionRule>(
						genPreconds, invSubR.apply(rule->actionLiteral), invSubR.apply(rule->add), invSubR.apply(rule->del),
						set<shared_ptr<ActionRule>>{ rule, example }, startPu, true);

					genRule->removedPreconditions = removedPreconds;

					foreach(pair, precondsNecessities)
						if (in(genRule->preconditions, pair->first) || in(genRule->removedPreconditions, pair->first)) {
							//assertMsg(isProb(pair->second), "Precond necessity not a probability" + joinmap(precondsNecessities));
							genRule->precondsNecessities[pair->first] = pair->second;
						}
					foreach(pair, constsNecessities)
						if (in(genRule->constsNecessities, pair->first)) {
							//assertMsg(isProb(pair->second), "Const necessity not a probability " + joinmap(constsNecessities));
							genRule->constsNecessities[pair->first] = pair->second;
						}

					log << "Gen rule:" << endl << *genRule << endl;

					if (genRule->wellFormed()) {
						bool doesntContradict = true;
						foreach(cx, counterExamples)
							if (genRule->contradicts(*cx)) {
								log << "Contradicts counter-example:" << endl << **cx << endl;
								doesntContradict = false;
								break;
							}
						if (doesntContradict) {
							foreach(fcx, failedActionsCounterExamples) {
								if (genRule->prematches(*fcx)) {
									log << "Prematches with a failed action counter-example:" << endl << **fcx << endl;
									doesntContradict = false;
									break;
								}
							}
						}

						if (doesntContradict) {
							bool better = lggRule == nullptr ||
								(iraleConfig->getBool("least_general") ?
								genRule->preconditions.size() > lggRule->preconditions.size() :
								genRule->preconditions.size() < lggRule->preconditions.size());

							if (better) {
								log << "Better LGG found." << endl;
								lggRule = genRule;
							}
						}
					}
					else log << "Not well formed." << endl;
				}

				if (lggRule != nullptr) {
					rules.insert(lggRule);
					rules.erase(rule);
					recovered = true;
					log << "Rule added to active rules." << endl;
				}
			}
		}
	}

	// Adding the example as such in the rules set
	if (!recovered) {
		log << "GENERALIZING - STEP 3 - Adding as such" << endl;

		log << example->toString() << endl;

		if (iraleConfig->getBool("always_generalize_constants")) {
			shared_ptr<ActionRule> generalizedRule = example->makeUseOfVariables();
			rules.insert(generalizedRule);
		}
		else
			rules.insert(example);

		updateInternalPlanner();

		if (in(failedBeforeFirstSuccess, example->actionLiteral.pred)) {
			foreach(trace, failedBeforeFirstSuccess[example->actionLiteral.pred])
				learner->corroborateRules(*trace);

			failedBeforeFirstSuccess.erase(example->actionLiteral.pred);
		}
	}
}

void LearningAgent::setupInternalPlanner() {
	internalDomain = domainFromRules(domain, rules);

	planner = make_shared<AStarAgent>(verbose);
	planner->init(internalDomain, instances, goal, trace);

	if (iraleConfig->getBool("use_bayesian_explorer"))
		learner = make_shared<BayesianExplorer>(verbose);
	else
		learner = make_shared<IRALeExplorer>(verbose);
	
	learner->init(internalDomain, instances, goal, trace);
	startPu = learner->startPu;

	vector<shared_ptr<ActionRule>> vecRules;
	foreach(r, rules)
		vecRules.push_back(*r);
	learner->setRules(vecRules);
	learner->setActionLiterals(domain->getActionLiterals());
}

void LearningAgent::updateInternalPlanner() {
	internalDomain = domainFromRules(domain, rules);
	
	planner->init(internalDomain, instances, goal, trace);
	learner->init(internalDomain, instances, goal, trace);
	vector<shared_ptr<ActionRule>> vecRules;
	foreach(r, rules)
		vecRules.push_back(*r);
	learner->setRules(vecRules);
}

void LearningAgent::handleEvent(SDL_Event event) {
	if (event.type == SDL_KEYDOWN)
		if (event.key.keysym.sym == SDLK_l)
			learning = !learning;
}
