/**
 * @author Thomas Lamson
 * Contact: thomas.lamson@cea.fr
 */

#pragma once

#include "Agents/Agent.h"

class FFAgent : public Agent {
public:
	FFAgent(bool inVerbose) : Agent(inVerbose) {}

	void init(shared_ptr<Domain> inDomain, vector<Term> inInstances, Goal inGoal, shared_ptr<vector<Trace>> inTrace) override;

	Literal getNextAction(State state) override;
	void updateProblem(vector<Term> inInstances, Goal inGoal, vector<Literal> inHeadstart) override;

	bool receivesEvents = false;

private:
	bool solveProblem(State state, int stateHeuristic, set<Literal> helpfulFacts, vector<Literal>& currentPlan);
	int solveRelaxedProblem(State state, set<Literal>& helpfulFacts);

	bool planReady = false;
	vector<Literal> plan;
};
