/**
 * This is a C++ direct translation of Python STRIPS solver by Tansey (though leveraging classes already defined).
 * Python implementation can be found at: https://github.com/tansey/strips
 *
 * @author Thomas Lamson
 * Contact: thomas.lamson@cea.fr
 */

#include "Agents/Strips/StripsAgent.h"

#define MAXDEPTH 20

using namespace std;

bool weakMatch(Condition c1, Condition c2) {
	return c1.lit == c2.lit;
}

bool weakContains(vector<Condition> items, Condition target) {
	foreach(it, items)
		if (weakMatch(*it, target))
			return true;
	return false;
}

Opt<Condition> weakFind(vector<Condition> items, Condition target) {
	foreach(it, items)
		if (weakMatch(*it, target))
			return Opt<Condition>(*it);
	return Opt<Condition>();
}

Opt<Condition> strongFind(vector<Condition> items, Condition target) {
	foreach(it, items)
		if (*it == target)
			return Opt<Condition>(*it);
	return Opt<Condition>();
}

void StripsAgent::init(shared_ptr<Domain> inDomain, vector<Term> inInstances, Goal inGoal, shared_ptr<vector<Trace>> inTrace) {
	Agent::init(inDomain, inInstances, inGoal, inTrace);
	planReady = false;
	prepareActionSubstitutions();
}

void StripsAgent::updateProblem(vector<Term> inInstances, Goal inGoal, vector<Literal> inHeadstart) {
	allGroundedActions.clear();
	Agent::updateProblem(inInstances, inGoal, inHeadstart);
	planReady = false;
	plan.clear();
	prepareActionSubstitutions();
}

Literal StripsAgent::getNextAction(State state) {
	if (!planReady)
		findPlan(state);

	if (planReady && plan.size() > 0) {
		Literal nextAction = plan.back();
		plan.pop_back();

		if(verbose) cout << plan.size() << " steps remaining." << endl;
		return nextAction;
	}

	return Literal();
}

void StripsAgent::prepareActionSubstitutions() {
	vector<Action> actions = domain->getActions();

	vector<Term> atomizedInstances;
	vector<Term> allInsts = instances + domain->getConstants();
	foreach(it, allInsts)
		atomizedInstances.push_back(*it);

	foreach(it, actions) {
		Action action = *it;

		vector<Substitution> subs = Substitution().expandUncovered(action.actionLiteral.parameters, atomizedInstances, true);

		foreach(subit, subs)
			allGroundedActions.push_back(GroundedAction(action, *subit));
	}
}

void StripsAgent::findPlan(State state) {
	if (verbose) cout << endl << "----------------------------------------------------------------------" << endl << "Starting STRIPS planification..." << endl;

	vector<Condition> goals;
	foreach(fit, goal.trueFacts)
		insertUnique(&goals, Condition(*fit, true));
	foreach(fit, goal.falseFacts)
		insertUnique(&goals, Condition(*fit, false));

	if (verbose) cout << "Goals: " << join(", ", goals) << endl;

	vector<GroundedAction> planActions;
	bool success = findPlanRecursive(state, goals, planActions, {}, 0);

	if (success) {

		if (verbose) {
			cout << endl;
			cout << "----------------------------- SUCCESS --------------------------------" << endl;
			cout << "Plan:  " << join(" -> ", planActions) << endl;
			cout << "----------------------------------------------------------------------" << endl;
			cout << endl;
		}

		vector<Action> actions = domain->getActions();
		foreach(git, planActions)
			plan.insert(plan.begin(), git->actionLiteral);
		planReady = true;
	}
	else {
		if (verbose) {
			cout << endl;
			cout << "------------------------- FAILED TO PLAN -----------------------------" << endl;
			cout << endl;
		}
	}
}

int initialStateDistance(State state, vector<Condition> conds) {
	int i = 0;
	foreach(cit, conds)
		if (!cit->reached(state))
			i++;
	return i;
}

bool StripsAgent::findPlanRecursive(State &state, vector<Condition> goals, vector<GroundedAction> &currentPlan, map<GroundedAction, set<State>> forbiddenActions, size_t depth) {
	string pad = padString(depth);

	if (goals.empty()) return true;
	if (depth > MAXDEPTH) return false;

	size_t i = 0;
	while (i < goals.size()) {
		Condition curGoal = goals[i];

		if (verbose) {
			cout << endl;
			cout << pad << "----------------------------- Depth: " << depth << " --------------------------------" << endl;
			cout << pad << "Current Plan:  " << join(" -> ", currentPlan) << endl;
			cout << pad << "Subgoal:       " << curGoal << endl;
			cout << pad << "Other Goals:   " << join(", ", goals, i + 1) << endl;
			cout << pad << "State:         " << state << endl;
			cout << endl;
		}

		if (curGoal.reached(state)) {
			i++;
			if (verbose) cout << pad << ">> Subgoal satisfied." << endl;
			continue;
		}

		vector<GroundedAction> possibleActions = getSortedPossibleActions(curGoal, state);

		if (verbose) {
			cout << pad << "List of possible actions that satisfy " << curGoal << ":" << endl;

			foreach(actit, possibleActions) {
				cout << pad << "> " << actit->toString() << " - Score: " << initialStateDistance(state, actit->preConditions) << endl;
			}

			cout << endl;
		}

		bool found = false;
		foreach(actit, possibleActions) {
			GroundedAction action = *actit;

			auto f = forbiddenActions.find(action);
			if (f != forbiddenActions.end()) {
				if (f->second.find(state) != f->second.end()) continue;
				forbiddenActions[action].insert(state);
			}
			else {
				forbiddenActions[action] = { state };
			}

			if (verbose) cout << pad << "-> Trying action: " << action << endl;
			
			// Only continue if action's preConditions can be reached and if action's postConditions don't contradict our goals
			if (!canReachPreconds(action.preConditions, state, pad)) continue;

			if (verbose) cout << pad << "   Preconditions can be reached." << endl;

			//if (contradicts(action.postConditions, goals, pad)) continue;

			//if (verbose) cout << pad << "   Doesn't contradict goals." << endl;

			// Recursively try to find a plan to reach action's preConditions
			State newState = State(state);
			vector<Condition> subGoals = action.preConditions;
			//currentPlan.push_back(action);

			vector<GroundedAction> subPlan;
			bool success = findPlanRecursive(newState, subGoals, subPlan, forbiddenActions, depth + 1);

			// Unable to find such a plan, we skip to the next action
			if (!success) {
				if (verbose) cout << pad << ">> No solution found with this action." << endl;
				//currentPlan.pop_back();
				continue;
			}

			if (verbose) cout << pad << ">> Possible plan found: " << join(" -> ", subPlan) << endl;

			// If we were able to find such a plan, we apply it in a temporary state and continue searching
			foreach(pit, action.postConditions)
				if (pit->truth) newState.addFact(pit->lit);
				else newState.removeFact(pit->lit);

			/*
			We need to check if the state deleted any of the previous goals. Three options on how to handle this:
				1. Give up
				2. Protect it from happening by backtracking all the way (requires fine-grained tracking of states)
				3. Re-introduce any goal which was deleted
			We choose 3. here, because it eventually solves the problem, given that no goal becomes impossible (irreversible actions).
			*/

			vector<Condition> clobbered;
			if (verbose && clobbered.size() > 0) cout << pad << "   >> Clobbered goals: " << join(", ", clobbered) << endl;
			foreachindex(gIndex, goals) {
				if (gIndex == i) break;
				if (goals[gIndex] == curGoal) continue;
				if (goals[gIndex].reached(newState)) continue;
				clobbered.push_back(goals[gIndex]);
			}
			foreach(git, clobbered) {
				removeFirst(&goals, *git);
				goals.push_back(*git);
			}
			i -= clobbered.size();

			foreach(it, subPlan)
				currentPlan.push_back(*it);
			currentPlan.push_back(action);

			// We now accept the temporary state
			state = newState;
			if (verbose) cout << pad << ">> Updating." << endl;
			if (verbose) cout << pad << "   New state: " << state << endl;
			if (verbose) cout << pad << "   New plan:  " << join(" <- ", currentPlan) << endl;

			i++;
			found = true;
			break;
		}

		if (!found) {
			if (verbose) cout << pad << ">> No action found to satisfy this subgoal. Backtracking." << endl;
			return false;
		}
	}

	if (verbose) cout << pad << ">> Solution found to satisfy subgoals." << endl;
	return true;
}

vector<GroundedAction> StripsAgent::getSortedPossibleActions(Condition goal, State state) {
	vector<GroundedAction> possible;
	foreach(actit, allGroundedActions) {
		foreach(pit, actit->postConditions) {
			if (*pit == goal) {
				possible.push_back(*actit);
				break;
			}
		}
	}

	auto compare = [&, state](GroundedAction lhs, GroundedAction rhs) {
		return initialStateDistance(state, lhs.preConditions) < initialStateDistance(state, rhs.preConditions);
	};

	sort(possible.begin(), possible.end(), compare);
	return possible;
}

bool StripsAgent::canReachPreconds(vector<Condition> conds, State state, string pad) {
	foreach(cit, conds) {
		if (cit->reached(state)) continue;
		bool found = false;
		foreach(actit, allGroundedActions) {
			foreach(pit, actit->postConditions) {
				if (*pit == *cit) {
					found = true;
					break;
				}
			}
			if (found) break;
		}
		if (found) continue;

		if (verbose) cout << pad << "   Couldn't reach precondition: " << *cit << endl;
		return false;
	}
	return true;
}

bool StripsAgent::contradicts(vector<Condition> newConds, vector<Condition> goals, string pad) {
	foreach(cit, newConds) {
		Opt<Condition> f = weakFind(goals, *cit);
		if (f.there && f.obj.truth != cit->truth) return true;
	}
	return false;
}
