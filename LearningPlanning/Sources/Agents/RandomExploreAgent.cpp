/**
 * @author Thomas Lamson
 * Contact: thomas.lamson@cea.fr
 */

#include "Agents/RandomExploreAgent.h"

#define RANDOM_DEPTH 15
#define RANDOM_PLANS 30
#define RANDOM_ACTION_TRIALS 500
#define SMART_CHOICE_PROB 0.2f
#define EXPLORE_RANDOM_DEPTH 10
#define EXPLORE_RANDOM_PLANS 10
#define BAD_STEPS_BEFORE_REEXPLORE 3

void RandomExploreAgent::init(shared_ptr<Domain> inDomain, vector<Term> inInstances, Goal inGoal, shared_ptr<vector<Trace>> inTrace) {
	Agent::init(inDomain, inInstances, inGoal, inTrace);
	prepareActionSubstitutions();
}

void RandomExploreAgent::prepareActionSubstitutions() {
	vector<InstantiatedAction> allInstActs;

	allActions.clear();
	factScores.clear();
	vector<Action> actions = domain->getActions();

	foreach(it, actions) {
		Action action = *it;

		vector<Substitution> subs = Substitution().expandUncovered(action.actionLiteral.parameters, instances + domain->getConstants(), true);

		foreach(subit, subs) {
			allInstActs.push_back(InstantiatedAction(action, *subit));
			allActions.push_back(subit->apply(action.actionLiteral));
		}
	}

	foreach(act, allInstActs)
		foreach(fit, act->action.truePrecond) {
		Literal fact = act->substitution.apply(*fit);
		if (in(factScores, fact)) factScores[fact]++;
		else factScores[fact] = 1;
	}

	actionsDict.clear();
	smartActions.clear();
	foreach(act, allInstActs) {
		Predicate pred = act->action.actionLiteral.pred;
		if (in(actionsDict, pred)) actionsDict[pred]->push_back(act->substitution.apply(act->action.actionLiteral));
		else actionsDict[pred] = new vector<Literal>({ act->substitution.apply(act->action.actionLiteral) });
		actionPredicates.insert(pred);

		int helps = 0;
		foreach(eff, act->action.add)
			if (in(goal.trueFacts, act->substitution.apply(*eff)))
				helps++;
			else if (in(goal.falseFacts, act->substitution.apply(*eff)))
				helps--;
		foreach(eff, act->action.del)
			if (in(goal.trueFacts, act->substitution.apply(*eff)))
				helps--;
			else if (in(goal.falseFacts, act->substitution.apply(*eff)))
				helps++;

		while (helps > 0) {
			smartActions.push_back(act->substitution.apply(act->action.actionLiteral));
			helps--;
		}
	}
}

Literal RandomExploreAgent::getNextAction(State state) {
	if (heuristic(state) == 0.0f) {
		cout << "Goal reached." << endl;
		return Literal();
	}

	if (randomActionsToPerform > 0) {
		randomActionsToPerform--;
		vector<Literal> available = getAvailableActions(state);
		return *select_randomly(available.begin(), available.end());
	}

	if (plan.size() == 0) {
		// We are stuck
		float newHeuristic = heuristic(state);
		if (!dontCheckPrev && newHeuristic >= previousHeuristic) {
			stayedSame++;
			if (stayedSame >= BAD_STEPS_BEFORE_REEXPLORE || explorePhase) {
				stayedSame = 0;
				explorePhase = !explorePhase;
				dontCheckPrev = true;
				return getNextAction(state);
			}
		}

		if (dontCheckPrev || newHeuristic < previousHeuristic) {
			stayedSame = 0;
			previousHeuristic = newHeuristic;
		}

		dontCheckPrev = false;

		generateRandomPlan(state);
		if (plan.size() > 0)
			simplifyPlan(state);
	}

	if (plan.size() > 0) {
		cout << plan.size() << endl;
		Literal nextAction = plan.back();
		plan.pop_back();
		cout << nextAction.toString() << endl;
		return nextAction;
	}
	return Literal();
}

void RandomExploreAgent::updateProblem(vector<Term> inInstances, Goal inGoal, vector<Literal> inHeadstart) {
	Agent::updateProblem(inInstances, inGoal, inHeadstart);
	plan.clear();

	stayedSame = 0;
	previousHeuristic = 1000.0f;
	exploreSteps = 5;
	explorePhase = true;
	dontCheckPrev = false;

	prepareActionSubstitutions();
}

float RandomExploreAgent::heuristic(State state) {
	unsigned int count = 0;

	foreach(fact, state.facts)
		if (in(factScores, *fact))
			count += factScores[*fact];
	
	float h = -(count * 1.0f) / (state.facts.size() * 1.0f) - 1.0f;

	if (!explorePhase) {
		int unreachedGoals = 0;

		foreach(it, goal.trueFacts)
			if (!state.contains(*it))
				unreachedGoals++;
		foreach(it, goal.falseFacts)
			if (state.contains(*it))
				unreachedGoals++;

		return max(0.0f, unreachedGoals * 1.0f + max(-0.99f, h * 0.01f));
	}

	return h;
}

void RandomExploreAgent::generateRandomPlan(State state) {
	searchState = state;
	plan.clear();
	float bestPlanHeuristic = heuristic(state);

	unsigned int plans = explorePhase ? EXPLORE_RANDOM_PLANS : RANDOM_PLANS;
	unsigned int depth = explorePhase ? EXPLORE_RANDOM_DEPTH : RANDOM_DEPTH;

	for (unsigned int p = 0; p < plans; p++) {
		vector<Literal> currentPlan;
		State currentState = state;

		for (unsigned int a = 0; a < depth; a++) {
			cout << "\r" << "Best heuristic: " << bestPlanHeuristic << " - Steps: " << plan.size() << " - " << p * depth + a << "       ";

			Opt<State> couldPerform = Opt<State>();
			unsigned int trials = RANDOM_ACTION_TRIALS;
			vector<Literal>* chooseFrom;
			Literal action;

			chooseFrom = actionsDict[*select_randomly(actionPredicates.begin(), actionPredicates.end())];

			while (!couldPerform.there && trials > 0) {
				trials--;
				
				if (trials % 50 == 0) chooseFrom = actionsDict[*select_randomly(actionPredicates.begin(), actionPredicates.end())];

				if (!explorePhase && static_cast <float> (rand()) / static_cast <float> (RAND_MAX) < SMART_CHOICE_PROB)
					action = *select_randomly(smartActions.begin(), smartActions.end());
				else
					action = *select_randomly(chooseFrom->begin(), chooseFrom->end());
				
				couldPerform = domain->tryAction(currentState, instances, action);
			}

			if (!couldPerform.there) break;

			currentPlan.insert(currentPlan.begin(), action);

			currentState = couldPerform.obj;
			float h = heuristic(currentState);

			if (h == bestPlanHeuristic && currentPlan.size() < plan.size()) {
				plan = currentPlan;
			}
			else if (h < bestPlanHeuristic) {
				plan = currentPlan;
				bestPlanHeuristic = h;
			}

			if (h == 0) break;
		}

		if (bestPlanHeuristic == 0) break;
	}
	cout << endl;
}

void RandomExploreAgent::simplifyPlan(State state) {
	vector<State> states = { state };
	vector<Literal> simplifiedPlan;

	size_t stepsBefore = plan.size();

	foreach(action, plan) {
		State newState = domain->tryAction(states.back(), instances, *action).obj;

		bool found = false;
		size_t index = 0;
		foreachindex(si, states) {
			if (states[si] == newState) {
				found = true;
				index = si;
				break;
			}
		}

		if (found) {
			vector<State> tempStates = states;
			states.clear();

			vector<Literal> tempPlan = simplifiedPlan;
			simplifiedPlan.clear();

			for (size_t si = 0; si < index + 1; si++) {
				states.push_back(tempStates[si]);
				if (si < index)
					simplifiedPlan.push_back(tempPlan[si]);
			}
		}
		else {
			states.push_back(newState);
			simplifiedPlan.push_back(*action);
		}
	}

	plan = simplifiedPlan;

	if (plan.size() != stepsBefore)
		cout << "Simplified plan: from " << stepsBefore << " to " << plan.size() << " steps." << endl;
}
