/**
 * @author Thomas Lamson
 * Contact: thomas.lamson@cea.fr
 */

#include "Agents/PartialOrderPlanner/PopAgentMultivariate.h"

using namespace std;

PartialPlan::PartialPlan(State startState, Goal finishGoal, Literal inStartLiteral, Literal inFinishLiteral) :
	startLiteral(inStartLiteral), finishLiteral(inFinishLiteral) {
	vector<Condition> startConditions, finishConditions;

	foreach(fact, startState.facts)
		startConditions.push_back(Condition(*fact, true));
	
	foreach(fact, finishGoal.trueFacts)
		finishConditions.push_back(Condition(*fact, true));
	foreach(fact, finishGoal.falseFacts)
		finishConditions.push_back(Condition(*fact, false));

	startAction = GroundedAction(startLiteral, {}, startConditions);
	finishAction = GroundedAction(finishLiteral, finishConditions, {});

	Substitution safeVariablesStart = Substitution(false);
	set<Term> startVars = startAction.getVariables();
	foreach(var, startVars)
		safeVariablesStart.set(*var, Variable(var->name + "_0"));
	startAction = safeVariablesStart.apply(startAction);

	Substitution safeVariablesFinish = Substitution(false);
	set<Term> finishVars = finishAction.getVariables();
	foreach(var, finishVars)
		safeVariablesFinish.set(*var, Variable(var->name + "_1"));
	finishAction = safeVariablesFinish.apply(finishAction);

	actions.push_back(startAction); // Index 0
	actions.push_back(finishAction); // Index 1
	addConstraint(0, 1);

	foreach(precond, finishConditions)
		openPreconditions.insert({ *precond, 1 });
}

PartialPlan::PartialPlan(PartialPlan const& other) {
	startLiteral = other.startLiteral;
	finishLiteral = other.finishLiteral;

	startAction = other.startAction;
	finishAction = other.finishAction;
	actions = other.actions;
	
	causalLinks = other.causalLinks;
	constraints = other.constraints;
	openPreconditions = other.openPreconditions;

	history = other.history;
}

void PartialPlan::computeNextChoices(vector<GroundedAction> availableActions,
									 vector<shared_ptr<PartialPlan>>& /*r*/ newPlans) {
	set<pair<size_t, Substitution>> bestBindings;
	OpenPrecondition boundPrecond;

	set<pair<GroundedAction, Substitution>> bestAS;
	OpenPrecondition ASPrecond;

	foreach(openPrecond, openPreconditions) {

		// 1. Looking for direct bindings from current precondition to an existing action
		set<pair<size_t, Substitution>> bindings = getDirectBindings(*openPrecond);

		// 2. Looking for new actions and substitutions to fulfill current precondition
		set<pair<GroundedAction, Substitution>> AS;

		Condition precond = openPrecond->first;
		foreach(actit, availableActions) {

			GroundedAction action = *actit;
			set<Term> vars = action.getVariables();
			Substitution safeVariables = Substitution(false);
			foreach(var, vars)
				safeVariables.set(*var, Variable(var->name + "_" + to_string(actions.size())));
			action = safeVariables.apply(action);

			foreach(eff, action.postConditions) {
				if (eff->truth != precond.truth) continue;

				// Try to unify effect with the precondition being solved
				Substitution sub = Substitution(false);
				bool unifies = sub.unify(eff->lit, precond.lit);
				if (!unifies) continue;

				//if (in(actions, sub.apply(action))) continue;

				AS.insert({ action, sub });
			}
		}

		// 3. We bind the precondition with the least direct bindings (> 0)
		if (bindings.size() > 0)
			if (bestBindings.size() == 0 || bestBindings.size() > bindings.size()) {
				boundPrecond = *openPrecond;
				bestBindings = bindings;
			}

		// 4. We add an action to the precondition with the least choices (> 0)
		if (AS.size() > 0)
			if (bestAS.size() == 0 || bestAS.size() > AS.size()) {
				ASPrecond = *openPrecond;
				bestAS = AS;
			}
	}

	// 5. Creating new plans by performing direct bindings
	foreach(binding, bestBindings) {
		PartialPlan newPlan = PartialPlan(*this);
		if (newPlan.directlyBindPrecondition(boundPrecond, binding->first, binding->second))
			newPlans.push_back(make_shared<PartialPlan>(newPlan));
	}

	// 6. Creating new plans by performing actions with substitutions
	foreach(actSub, bestAS) {
		PartialPlan newPlan = PartialPlan(*this);
		if (newPlan.applyActionToPrecondition(ASPrecond, actSub->first, actSub->second))
			newPlans.push_back(make_shared<PartialPlan>(newPlan));
	}
}

void PartialPlan::updateHeuristic() {
	heuristic = 0.0f;

	foreach(precond, openPreconditions)
		if (hasDirectBinding(*precond))
			heuristic += 1.0f;
		else
			heuristic += 3.0f;

	//heuristic *= 0.5f;
}

bool PartialPlan::goalCheck() {
	return openPreconditions.size() == 0;
}

bool PartialPlan::directlyBindPrecondition(OpenPrecondition precond, size_t toAction, Substitution sub) {
	history.push_back("Bound precondition: " + precond.first.toString() + " required by [" + to_string(precond.second) + "] to [" + to_string(toAction) + "]");
	
	// 1. Remove precond from openPreconditions
	openPreconditions.erase(precond);

	// 2. Perform variable binding
	if (!bindVariables(sub)) return false;
	foreach(act, actions) {
		set<Term> params;
		foreach(pit, act->actionLiteral.parameters)
			params.insert(*pit);
		if (params.size() != act->actionLiteral.parameters.size()) return false;
	}

	// 3. Add Action->Precond's owner constraint
	addConstraint(toAction, precond.second);

	// 4. Add (Action->Precond->Precond's owner) causal link
	CausalLink newLink = CausalLink(toAction, precond.first, precond.second);
	causalLinks.insert(newLink);
	history.push_back("Added causal link: (" + to_string(toAction) + ", " + precond.first.lit.toString() + ", " + to_string(precond.second) + ")");

	// 5. Protect new link from every other action
	foreachindex(i, actions)
		if (!protectCausalLink(newLink, i))
			return false;

	return true;
}

bool PartialPlan::applyActionToPrecondition(OpenPrecondition precond, GroundedAction action, Substitution sub) {
	// 1. Remove precond from openPreconditions
	openPreconditions.erase(precond);

	// 2. Perform variable binding
	action = sub.apply(action);
	/*if (!bindVariables(sub)) return false;
	set<GroundedAction> uniqueActions;
	foreach(act, actions) {
		set<Term> params;
		foreach(pit, act->actionLiteral.parameters)
			params.insert(*pit);
		if (params.size() != act->actionLiteral.parameters.size()) return false;
		uniqueActions.insert(*act);
	}
	if (uniqueActions.size() != actions.size()) return false;*/

	// 3. Insert new action
	size_t actionId = actions.size();
	actions.push_back(action);
	history.push_back("Added action: [" + to_string(actionId) + "] - " + action.actionLiteral.toString());

	// 4. Add Start->Action constraint and Action->Finish
	addConstraint(0, actionId);
	addConstraint(actionId, 1);

	// 5. Protect all causal links from new action
	foreach(link, causalLinks)
		if (!protectCausalLink(*link, actionId))
			return false;

	// 6. Insert new preconditions to openPreconditions
	foreach(newPrecond, action.preConditions)
		openPreconditions.insert({ *newPrecond, actionId });

	// 7. Add Action->Precond's owner constraint
	addConstraint(actionId, precond.second);

	// 8. Add (Action->Precond->Precond's owner) causal link
	CausalLink newLink = CausalLink(actionId, precond.first, precond.second);
	causalLinks.insert(newLink);
	history.push_back("Added causal link: (" + to_string(actionId) + ", " + precond.first.lit.toString() + ", " + to_string(precond.second) + ")");

	// 9. Protect new link from every other action
	foreachindex(i, actions)
		if (!protectCausalLink(newLink, i))
			return false;

	return true;
}

bool PartialPlan::bindVariables(Substitution sub) {
	if (sub.getMapping().size() > 0) history.push_back("Bound variables: " + sub.toString());

	foreachindex(i, actions)
		actions[i] = sub.apply(actions[i]);

	set<CausalLink> newCausalLinks;
	foreach(link, causalLinks)
		newCausalLinks.insert(CausalLink(get<0>(*link), Condition(sub.apply(get<1>(*link).lit), get<1>(*link).truth), get<2>(*link)));
	causalLinks = newCausalLinks;

	set<OpenPrecondition> newOpenPreconditions;
	foreach(precond, openPreconditions)
		newOpenPreconditions.insert({ Condition(sub.apply(precond->first.lit), precond->first.truth), precond->second });
	openPreconditions = newOpenPreconditions;

	foreach(link, causalLinks)
		foreachindex(i, actions)
			if (!protectCausalLink(*link, i)) return false;
	return true;
}

void PartialPlan::addConstraint(size_t before, size_t after) {
	history.push_back("Added constraint: " + to_string(before) + " -> " + to_string(after));
	if (in(constraints, before)) constraints[before].insert(after);
	else constraints[before] = { after };
}

void PartialPlan::removeConstraint(size_t before, size_t after) {
	history.push_back("Removed constraint: " + to_string(before) + " -> " + to_string(after));
	if (in(constraints, before)) constraints[before].erase(after);
}

bool PartialPlan::hasConstraint(size_t before, size_t after) {
	if (!in(constraints, before)) return false;
	if (!in(constraints[before], after)) return false;
	return true;
}

set<size_t> PartialPlan::getConstraints(size_t before) {
	if (!in(constraints, before)) return {};
	return constraints[before];
}

bool PartialPlan::protectCausalLink(CausalLink link, size_t fromAction) {
	if (fromAction == get<0>(link) || fromAction == get<2>(link)) return true;

	GroundedAction action = actions[fromAction];
	foreach(eff, action.postConditions) {
		if (get<1>(link).truth == eff->truth) continue;
		if (get<1>(link).lit != eff->lit) continue;

		history.push_back("Threat detected: action [" + to_string(fromAction) + "] threatens (" + to_string(get<0>(link)) + ", " + get<1>(link).toString() + ", " + to_string(get<2>(link)) + ")");
		
		// Trying promotion
		addConstraint(fromAction, get<0>(link));
		if (checkCyclicity()) {
			removeConstraint(fromAction, get<0>(link));
			history.pop_back();
			history.pop_back();

			// Trying demotion
			addConstraint(get<2>(link), fromAction);
			if (checkCyclicity()) {
				removeConstraint(get<2>(link), fromAction);
				history.pop_back();
				history.pop_back();

				// Couldn't solve threat
				return false;
			}
		}
		break;
	}
	return true;
}

bool visit(OrderingGraph& graph, size_t vertex, set<size_t> path) {
	path.insert(vertex);
	foreach(i, graph[vertex]) {
		if (in(path, *i)) return true;
		if (visit(graph, *i, path)) return true;
	}
	return false;
}

bool PartialPlan::checkCyclicity() {
	foreach(i, constraints)
		if (visit(constraints, i->first, {}))
			return true;
	return false;
}

bool PartialPlan::hasDirectBinding(OpenPrecondition precond) {
	foreachindex(i, actions)
		if (!isAAfterB(i, precond.second))
			foreach(eff, actions[i].postConditions) {
				if (eff->truth != precond.first.truth) continue;

				// Try to unify effect with the precondition being solved
				Substitution sub = Substitution(false);
				bool unifies = sub.unify(eff->lit, precond.first.lit);
				if (unifies) return true;
			}
	return false;
}

set<pair<size_t, Substitution>> PartialPlan::getDirectBindings(OpenPrecondition precond) {
	set<pair<size_t, Substitution>> bindings;
	foreachindex(i, actions)
		if (!isAAfterB(i, precond.second)) {
			foreach(eff, actions[i].postConditions) {
				if (eff->truth != precond.first.truth) continue;

				// Try to unify effect with the precondition being solved
				Substitution sub = Substitution(false);
				bool unifies = sub.unify(eff->lit, precond.first.lit);
				if (!unifies) continue;

				bindings.insert({ i, sub });
			}
		}
	return bindings;
}

bool PartialPlan::isAAfterB(size_t actionA, size_t actionB) {
	vector<size_t> search = { actionB };
	size_t index = 0;
	while (index < search.size()) {
		set<size_t> nexts = getConstraints(search[index]);
		foreach(next, nexts) {
			if (*next == actionA) return true;
			insertUnique(&search, *next);
		}
		index++;
	}
	return false;
}

bool PartialPlan::extractPlan(vector<GroundedAction>& /*r*/ plan) {
	set<size_t> toAdd;
	foreachindex(i, actions)
		toAdd.insert(i);

	bool added, eligible;
	while (toAdd.size() > 0) {
		added = false;
		size_t add = 0;

		foreach(a, toAdd) {
			eligible = true;
			foreachindex(b, actions)
				if (*a != b && in(toAdd, b) && isAAfterB(*a, b)) {
					eligible = false;
					break;
				}
			if (eligible) {
				added = true;
				add = *a;
				break;
			}
		}

		if (!added) return false;
		
		plan.push_back(actions[add]);
		toAdd.erase(add);
	}
	return true;
}

void PopAgentMultivariate::init(shared_ptr<Domain> inDomain, vector<Term> inInstances, Goal inGoal, shared_ptr<vector<Trace>> inTrace) {
	Agent::init(inDomain, inInstances, inGoal, inTrace);

	prepareActionSubstitutions();

	Predicate startPredicate = Predicate("POP_Start", 0);
	Predicate finishPredicate = Predicate("POP_Finish", 0);

	startLiteral = Literal(startPredicate, {});
	finishLiteral = Literal(finishPredicate, {});
}

void PopAgentMultivariate::updateProblem(vector<Term> inInstances, Goal inGoal, vector<Literal> inHeadstart) {
	planReady = false;
	plan.clear();
	availableActions.clear();

	Agent::updateProblem(inInstances, inGoal, inHeadstart);
	prepareActionSubstitutions();
}

void PopAgentMultivariate::prepareActionSubstitutions() {
	vector<Action> actions = domain->getActions();

	foreach(it, actions)
		availableActions.push_back(GroundedAction(*it));
}

Literal PopAgentMultivariate::getNextAction(State state) {
	if (!planReady)
		findPlan(state);

	if (planReady && plan.size() > 0) {
		Literal nextAction = plan.back();
		plan.pop_back();

		if (verbose) cout << plan.size() << " steps remaining." << endl;
		return nextAction;
	}

	return Literal();
}

bool PopAgentMultivariate::findPlan(State state) {
	if (verbose) cout << "Planning to achieve goal..." << endl;

	// Creating the first plan, with two actions only: Start and Finish
	shared_ptr<PartialPlan> startNode = make_shared<PartialPlan>(state, goal, startLiteral, finishLiteral);
	startNode->updateHeuristic();

	vector<shared_ptr<PartialPlan>> openList;
	openList.push_back(startNode);

	size_t step = 0;

	while (!openList.empty()) {
		step++;
		if (verbose) cout << "Step: " << step << " - Open list: " << openList.size() << endl;

		size_t index = 0;
		float minHeuristic = 0.0f;
		bool first = true;
		foreachindex(i, openList) {
			if (first || openList[i]->heuristic < minHeuristic) {
				first = false;
				index = i;
				minHeuristic = openList[i]->heuristic;
			}
		}

		shared_ptr<PartialPlan> current = openList[index];
		openList.erase(openList.begin() + index);

		if (verbose) {
			cout << "PARTIAL PLAN: heuristic = " << current->heuristic << endl;
			cout << "Actions: " << join(", ", current->actions) << endl;
			cout << "Open preconditions: ";
			foreach(precond, current->openPreconditions)
				cout << precond->first.lit.toString() << ", ";
			cout << endl;
			//cout << "Causal links: " << join(", ", current->causalLinks) << endl;
			cout << endl;
		}


		vector<shared_ptr<PartialPlan>> newPlans;
		current->computeNextChoices(availableActions, newPlans);

		foreach(plan, newPlans) {
			shared_ptr<PartialPlan> node = *plan;
			node->updateHeuristic();
			node->cost = current->cost + 1.0f;
			node->heuristic += node->cost;
			node->parent = current;
			if (node->goalCheck()) {
				// Found a plan !
				extractPlan(node);
				return true;
			}

			openList.push_back(node);
		}
	}

	return false;
}

void PopAgentMultivariate::extractPlan(shared_ptr<PartialPlan> partialPlan) {
	vector<Action> domainActions = domain->getActions();
	vector<GroundedAction> actions;

	if (!partialPlan->extractPlan(actions)) return;
	
	foreach(git, actions)
		plan.insert(plan.begin(), git->actionLiteral);
	planReady = true;


	if (verbose) {
		vector<shared_ptr<PartialPlan>> allSteps = { partialPlan };
		while (allSteps.front()->parent != nullptr)
			allSteps.insert(allSteps.begin(), allSteps.front()->parent);

		foreachindex(i, allSteps) {
			cout << "-----------------------------------------------" << endl;
			cout << "STEP " << i << ":" << endl;

			shared_ptr<PartialPlan> pp = allSteps[i];
			
			foreachindex(i, pp->actions) {
				cout << i << ": " << pp->actions[i].actionLiteral << endl;
			}
			foreach(it, pp->causalLinks) {
				Condition cond = get<1>(*it);
				cout << get<0>(*it) << " - " << cond << " - " << get<2>(*it) << endl;
			}
			foreach(it, pp->constraints) {
				cout << it->first << " before ";
				foreach(elem, it->second)
					cout << *elem << " ";
				cout << endl;
			}

			cout << endl << "History:" << endl;
			foreachindex(hindex, pp->history) {
				if (pp->parent == nullptr || hindex >= pp->parent->history.size())
					cout << "- " << pp->history[hindex] << endl;
			}

			cout << endl;
		}
	}


	if (verbose) cout << "Plan: " << join(", ", actions) << endl;
}
