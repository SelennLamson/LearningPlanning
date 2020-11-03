/**
 * @author Thomas Lamson
 * Contact: thomas.lamson@cea.fr
 */

#pragma once

#include "Agents/Agent.h"

class AStarAgent : public Agent {
public:
	AStarAgent(bool inVerbose) : Agent(inVerbose) {}

	void init(shared_ptr<Domain> inDomain, vector<Term> inInstances, Goal inGoal, shared_ptr<vector<Trace>> inTrace) override;

	Literal getNextAction(State state) override;
	void updateProblem(vector<Term> inInstances, Goal inGoal, vector<Literal> inHeadstart) override;
	void setMaxDepth(int newLimit);
	void setTimeLimit(float seconds);

	bool receivesEvents = false;

private:
	float heuristic(State state);

	bool planReady = false;
	vector<Literal> plan;
	int maxDepth = -1;
	float timeLimit = -1.0f;
};
