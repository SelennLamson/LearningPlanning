/**
 * @author Thomas Lamson
 * Contact: thomas.lamson@cea.fr
 *
 * This agent has access to the domain definition as any other agent, but it will not rely on it and will rather
 * try to learn it from traces and random execution of actions.
 *
 * Based on paper: "Incremental Learning of Relational Action Rules", Christophe Rodrigues, Pierre Gérard,
 *                 Céline Rouveirol, Henry Soldano, https://doi.org/10.1109/ICMLA.2010.73
 *
 * Openly available here: https://lipn.univ-paris13.fr/~rodrigues/icmla2010PP.pdf
 */

#pragma once

#include <memory>
#include <iostream>
#include <fstream>
#include <chrono>

#include "Agents/AStarAgent.h"
#include "Agents/Agent.h"
#include "Agents/LearningAgent/ActionRule.h"
#include "Agents/LearningAgent/ExplorerAgentBase.h"

#include "Agents/LearningAgent/IRALeExplorer.h"
#include "Agents/LearningAgent/BayesianExplorer.h"

#include "ConfigReader.h"

class ConvertedDomain;
class AStarAgent;

class LearningAgent : public Agent {
public:
	LearningAgent(bool inVerbose);

	void init(shared_ptr<Domain> inDomain, vector<Term> inInstances, Goal inGoal, shared_ptr<vector<Trace>> inTrace) override;

	Literal getNextAction(State state) override;
	void updateProblem(vector<Term> inInstances, Goal inGoal, vector<Literal> inHeadstart) override;
	void setupInternalPlanner();
	void updateInternalPlanner();
	void handleEvent(SDL_Event event) override;

	bool receivesEvents = true;

	// In paper: ALGORITHM 1: REVISION
	bool updateKnowledge(Trace trace);

	set<shared_ptr<ActionRule>> rules;
	set<shared_ptr<ActionRule>> counterExamples;

private:
	// In paper: ALGORITHM 2: SPECIALIZE
	set<shared_ptr<ActionRule>> specialize(shared_ptr<ActionRule> rule, shared_ptr<ActionRule> example);

	// In paper: ALGORITHM 3: GENERALIZE
	void generalize(shared_ptr<ActionRule> example);

	set<shared_ptr<ActionRule>> failedActionsCounterExamples;
	map<Predicate, vector<Trace>> failedBeforeFirstSuccess;

	shared_ptr<Agent> planner;
	shared_ptr<ExplorerAgentBase> learner;
	shared_ptr<Domain> internalDomain;

	ConfigReader* iraleConfig;
	int runs, steps;
	//int estimatedPreconditions = 10;
	float startPu = 0.5f;

	bool learning = true;
	size_t step = 0;

	bool revisedSinceLastEval = true;
	size_t lastRevisionStep = 0;
	float prevVarDist = 1.0f, prevPlanDist = 1.0f;

	ofstream statsFile;
	float*** stats = nullptr;
	vector<string> columns = {
		"CounterExamples",
		"Specificity",
		"PRev",
		"Pos",
		"VarDist",
		"RuleDist",
		"PlanDist"
	};

	int run = 0;
	chrono::high_resolution_clock::time_point startTime;
};
