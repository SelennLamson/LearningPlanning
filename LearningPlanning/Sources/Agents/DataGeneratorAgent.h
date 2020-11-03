/**
 * @author Thomas Lamson
 * Contact: thomas.lamson@cea.fr
 */

#pragma once

#include "Agents/Agent.h"
#include <vector>
#include <map>
#include <set>

class DataGeneratorAgent : public Agent {
public:
	DataGeneratorAgent(bool inVerbose) : Agent(inVerbose) {}

	void init(shared_ptr<Domain> inDomain, vector<Term> inInstances, Goal inGoal, shared_ptr<vector<Trace>> inTrace) override;

	Literal getNextAction(State state) override;
	void updateProblem(vector<Term> inInstances, Goal inGoal, vector<Literal> inHeadstart) override;

	bool receivesEvents = false;

private:
	void prepareActionSubstitutions();

	vector<Literal> allActions;
	vector<Predicate> actionPredicates;

	map<Predicate, vector<Trace>> samples;
	vector<pair<State, Goal>> problems;
};
