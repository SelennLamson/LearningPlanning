/**
 * @author Thomas Lamson
 * Contact: thomas.lamson@cea.fr
 */

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Logic/Domain.h"
#include "Logic/DomainTester.h"
#include "SDL.h"

using namespace std;

class LogicEngine;

class Agent {
public:
	Agent(bool inVerbose) : verbose(inVerbose) { }

	virtual void init(shared_ptr<Domain> inDomain, vector<Term> inInstances, Goal inGoal, shared_ptr<vector<Trace>> inTrace);

	void setDomainTester(shared_ptr<DomainTester> inDomainTester) {
		domainTester = inDomainTester;
	}

	void setEngine(LogicEngine* inEngine);

	virtual void updateProblem(vector<Term> inInstances, Goal inGoal, vector<Literal> inHeadstart);

	virtual Literal getNextAction(State state) {
		return Literal();
	}

	bool receivesEvents = false;
	virtual void handleEvent(SDL_Event event) {}

protected:
	vector<Literal> getAvailableActions(State state);

	shared_ptr<Domain> domain;
	Goal goal;
	shared_ptr<vector<Trace>> trace;
	vector<Term> instances;
	vector<Literal> headstartActions;

	shared_ptr<DomainTester> domainTester;
	LogicEngine* engine;

	bool verbose = false;
};
