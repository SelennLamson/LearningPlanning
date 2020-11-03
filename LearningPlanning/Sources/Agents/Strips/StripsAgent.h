/**
 * This is a C++ direct translation of Python STRIPS solver by Tansey (though leveraging classes already defined).
 * Python implementation can be found at: https://github.com/tansey/strips
 *
 * @author Thomas Lamson
 * Contact: thomas.lamson@cea.fr
 */

#pragma once

#include <vector>

#include "Agents/Agent.h"
#include "Logic/Domain.h"
#include "Utils.h"

using namespace std;

class StripsAgent : public Agent {
public:
	StripsAgent(bool inVerbose) : Agent(inVerbose) {}

	void init(shared_ptr<Domain> inDomain, vector<Term> inInstances, Goal inGoal, shared_ptr<vector<Trace>> inTrace) override;

	Literal getNextAction(State state) override;
	void updateProblem(vector<Term> inInstances, Goal inGoal, vector<Literal> inHeadstart) override;

	bool receivesEvents = false;

protected:
	void prepareActionSubstitutions();

	void findPlan(State state);
	bool findPlanRecursive(State &state, vector<Condition> goals, vector<GroundedAction> &currentPlan, map<GroundedAction, set<State>> forbiddenActions, size_t depth);
	vector<GroundedAction> getSortedPossibleActions(Condition goal, State state);
	bool canReachPreconds(vector<Condition> conds, State state, string pad);
	bool contradicts(vector<Condition> newConds, vector<Condition> goals, string pad);
	
	bool planReady = false;
	vector<Literal> plan;
	vector<GroundedAction> allGroundedActions;
};
