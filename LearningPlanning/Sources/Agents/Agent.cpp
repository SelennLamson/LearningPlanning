/**
 * @author Thomas Lamson
 * Contact: thomas.lamson@cea.fr
 */

#include "Agents/Agent.h"

#include "Logic/LogicEngine.h"

#include <assert.h>

void Agent::init(shared_ptr<Domain> inDomain, vector<Term> inInstances, Goal inGoal, shared_ptr<vector<Trace>> inTrace) {
	domain = inDomain;
	instances = inInstances;
	goal = inGoal;
	trace = inTrace;
}

void Agent::updateProblem(vector<Term> inInstances, Goal inGoal, vector<Literal> inHeadstart) {
	instances = inInstances;
	goal = inGoal;
	headstartActions = inHeadstart;
}

void Agent::setEngine(LogicEngine* inEngine) {
	engine = inEngine;
}

vector<Literal> Agent::getAvailableActions(State state) {
	vector<Literal> availableActions;

	foreach(act, domain->actions) {
		vector<Substitution> subs = state.unifyAction(*act);
		foreach(sub, subs) {
			vector<Substitution> expandedSubs = sub->expandUncovered(act->parameters, instances + domain->getConstants(), true);
			foreach(expSub, expandedSubs)
				availableActions.push_back(expSub->apply(act->actionLiteral));
		}
	}

	return availableActions;
}
