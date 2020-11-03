/**
 * @author Thomas Lamson
 * Contact: thomas.lamson@cea.fr
 */

#pragma once

#include "Agents/Agent.h"
#include "SDL.h"

class ManualAgent : public Agent {
public:
	ManualAgent(bool inVerbose) : Agent(inVerbose) {}

	void init(shared_ptr<Domain> inDomain, vector<Term> inInstances, Goal inGoal, shared_ptr<vector<Trace>> inTrace) override;

	Literal getNextAction(State state) override;
};
