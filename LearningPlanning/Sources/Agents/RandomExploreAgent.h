/**
 * @author Thomas Lamson
 * Contact: thomas.lamson@cea.fr
 */

#pragma once

#include "Agents/Agent.h"
#include <vector>
#include <map>
#include <set>

class RandomExploreAgent : public Agent {
public:
	RandomExploreAgent(bool inVerbose) : Agent(inVerbose) {}

	void init(shared_ptr<Domain> inDomain, vector<Term> inInstances, Goal inGoal, shared_ptr<vector<Trace>> inTrace) override;

	Literal getNextAction(State state) override;
	void updateProblem(vector<Term> inInstances, Goal inGoal, vector<Literal> inHeadstart) override;

	bool receivesEvents = false;

private:
	float heuristic(State state);
	void generateRandomPlan(State state);
	void simplifyPlan(State state);
	void prepareActionSubstitutions();

	State searchState;
	float previousHeuristic = 1000.0f;
	int stayedSame = 0;

	unsigned int randomActionsToPerform = 0;

	bool explorePhase = false;
	bool dontCheckPrev = false;
	int exploreSteps = 5;

	vector<Literal> allActions;
	map<Literal, unsigned int> factScores;
	vector<Literal> plan;
	vector<Literal> smartActions;
	map<Predicate, vector<Literal>*> actionsDict;
	set<Predicate> actionPredicates;
};
