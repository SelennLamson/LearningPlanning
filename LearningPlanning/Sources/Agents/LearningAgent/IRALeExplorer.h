/**
 * @author Thomas Lamson
 * Contact: thomas.lamson@cea.fr
 *
 * This IRALe Active Explorer implements Rodrigues (2012) paper which uses LGG to predict
 * whether an action is likely to lead to a generalization of the current model.
 * It combines this active exploration with a proportion of random actions.
 */

#pragma once

#include <vector>
#include <map>
#include <set>

#include "Agents/LearningAgent/ExplorerAgentBase.h"
#include "Agents/LearningAgent/ActionRule.h"

#include "ConfigReader.h"

class IRALeExplorer : public ExplorerAgentBase {

public:
	IRALeExplorer(bool inVerbose);

	void init(shared_ptr<Domain> inDomain, vector<Term> inInstances, Goal inGoal, shared_ptr<vector<Trace>> inTrace) override;
	void setRules(vector<shared_ptr<ActionRule>> newRules) override;
	void setActionLiterals(set<Literal> baseActionLiterals) override;

	Literal getNextAction(State state) override;
	void updateProblem(vector<Term> inInstances, Goal inGoal, vector<Literal> inHeadstart) override;

	bool receivesEvents = false;

private:
	void prepareActionSubstitutions();

	ConfigReader* explorerConfig;
	float epsilon;

	vector<shared_ptr<ActionRule>> rules;
	set<Literal> actionLiterals;
	set<Predicate> actionPredicates;
	vector<Literal> allActions;

	State prevState;
	vector<pair<Literal, size_t>> interestingActions;

	int iteration = 0;
};
