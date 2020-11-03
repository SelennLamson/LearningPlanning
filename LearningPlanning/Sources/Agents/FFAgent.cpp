/**
 * @author Thomas Lamson
 * Contact: thomas.lamson@cea.fr
 */

#include <assert.h>
#include <math.h>

#include "Agents/FFAgent.h"

#define MAX_DEPTH 100

using namespace std;

void FFAgent::init(shared_ptr<Domain> inDomain, vector<Term> inInstances, Goal inGoal, shared_ptr<vector<Trace>> inTrace) {
	Agent::init(inDomain, inInstances, inGoal, inTrace);
	planReady = false;
}

void FFAgent::updateProblem(vector<Term> inInstances, Goal inGoal, vector<Literal> inHeadstart) {
	Agent::updateProblem(inInstances, inGoal, inHeadstart);

	planReady = false;
	plan.clear();
}

Literal FFAgent::getNextAction(State state) {
	if (planReady && plan.size() > 0) {
		Literal nextAction = plan.back();
		plan.pop_back();

		cout << plan.size() << " steps remaining." << endl;
		return nextAction;
	}
	else if (planReady) {
		planReady = false;
		//return InstantiatedAction();
	}

	set<Literal> helpfulFacts;
	int stateHeuristic = solveRelaxedProblem(state, helpfulFacts);
	planReady = solveProblem(state, stateHeuristic, helpfulFacts, plan);
	if (planReady) return getNextAction(state);

	cout << "COULDN'T PLAN..." << endl;
	return Literal();
}

bool FFAgent::solveProblem(State state, int stateHeuristic, set<Literal> helpfulFacts, vector<Literal> &currentPlan) {
	/*
	Enforced-Hill-Climbing (EHC) using the relaxed problem heuristic.
	*/
	cout << "--------------------------------------------------------------------" << endl;
	cout << "SOLVE FROM: " << state << endl;
	cout << "State heuristic : " << stateHeuristic << " - Helpful facts : " << join(", ", helpfulFacts) << endl;

	vector<pair<State, vector<Literal>>> stack = { {state, currentPlan} };
	bool firstStep = true;

	while (stack.back().second.size() < MAX_DEPTH) {
		cout << "Current depth: " << stack.back().second.size() << endl;

		vector<pair<State, vector<Literal>>> prevStack = stack;
		stack.clear();

		foreach(statePlan, prevStack) {
			vector<Literal> availableActions = getAvailableActions(statePlan->first);

			foreach(act, availableActions) {
				State newState = domain->tryAction(statePlan->first, instances, *act).obj;

				vector<Literal> newPlan = statePlan->second;
				newPlan.insert(newPlan.begin(), *act);

				if (firstStep) {
					bool isHelpful = false;
					foreach(fact, helpfulFacts)
						if (newState.contains(*fact)) {
							isHelpful = true;
							break;
						}
					if (!isHelpful) {
						stack.push_back({ newState, newPlan });
						continue;
					}
				}

				set<Literal> newHelpfulFacts;
				int newStateHeuristic = solveRelaxedProblem(newState, newHelpfulFacts);

				bool reachedGoal = true;
				foreach(fact, goal.trueFacts)
					if (!newState.contains(*fact)) {
						reachedGoal = false;
						break;
					}
				if (reachedGoal)
					foreach(fact, goal.falseFacts)
						if (newState.contains(*fact)) {
							reachedGoal = false;
							break;
						}
				if (reachedGoal) {
					currentPlan = newPlan;
					return true;
				}

				if (newStateHeuristic < stateHeuristic) {
					currentPlan = newPlan;
					return true;
					return solveProblem(newState, newStateHeuristic, newHelpfulFacts, currentPlan);
				}

				stack.push_back({ newState, newPlan });
			}
		}
	}
	
	return false;
}

int FFAgent::solveRelaxedProblem(State state, set<Literal> &helpfulFacts) {
	/* 
	GraphPlan forward search to estimate the number of actions required to solve the problem.
	Problem is relaxed by ignoring delete effects of actions.
	 */

	vector<State> states = { state };
	vector<vector<GroundedAction>> actions;
	bool goalReached = false;

	while (true) {
		vector<Literal> availableActions = getAvailableActions(state);
		vector<GroundedAction> grounded;
		
		foreach(act, availableActions) {
			grounded.push_back(GroundedAction(*act));
			state = domain->tryAction(state, instances, *act, true).obj;
		}
		actions.push_back(grounded);
		states.push_back(state);

		goalReached = true;
		foreach(fact, goal.trueFacts)
			if (!state.contains(*fact)) {
				goalReached = false;
				break;
			}
		foreach(fact, goal.falseFacts)
			if (state.contains(*fact)) {
				goalReached = false;
				break;
			}

		if (goalReached || state == states[states.size() - 2])
			break;
	}

	if (!goalReached)
		return 1000;

	int index = (int)(states.size() - 2);
	int result = 0;

	set<Condition> toProve, toProveNext;

	foreach(fact, goal.trueFacts)
		toProveNext.insert(Condition(*fact, true));

	while (index >= 0) {
		if (index == 0)
			foreach(fact, toProveNext)
			helpfulFacts.insert(fact->lit);

		toProve = toProveNext;

		foreach(fact, toProve)
			if (!states[index].contains(fact->lit))
				foreach(act, actions[index])
					if (in(act->postConditions, *fact)) {
						toProveNext.erase(*fact);
						foreach(precond, act->preConditions)
							if (precond->truth)
								toProveNext.insert(*precond);
						result++;
						break;
					}

		index--;
	}

	return result;
}
