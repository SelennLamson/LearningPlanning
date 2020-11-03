/**
 * @author Thomas Lamson
 * Contact: thomas.lamson@cea.fr
 *
 * This Bayesian Explorer Agent uses hill climbing over a Revision Probability to select the best
 * plan of actions in order to learn the rules of the domain faster. Meta-actions are also
 * available during learning, like removing an entity from the problem, or the reset action.
 */

#pragma once

#include "Agents/Agent.h"
#include "Agents/LearningAgent/ActionRule.h"
#include <vector>
#include <map>
#include <set>

class ExplorerAgentBase : public Agent {
public:
	ExplorerAgentBase(bool inVerbose) : Agent(inVerbose) {}

	void virtual setRules(vector<shared_ptr<ActionRule>> newRules) {}
	void virtual setActionLiterals(set<Literal> baseActionLiterals) {}

	void virtual corroborateRules(Trace trace) {}
	void virtual informRevision(bool knowledgeRevised) {}

	bool receivesEvents = false;

	vector<Literal> plan;
	float statsRevProb = -1.0f;
	bool statsRevPos = false;
	//int estimatedPreconditionsPerRule;
	float startPu = 0.5f;
};
