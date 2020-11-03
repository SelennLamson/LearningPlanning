/**
 * @author Thomas Lamson
 * Contact: thomas.lamson@cea.fr
 *
 * This Bayesian Explorer Agent uses hill climbing over a Revision Probability to select the best
 * plan of actions in order to learn the rules of the domain faster. Meta-actions are also
 * available during learning, like removing an entity from the problem, or the reset action.
 */

#pragma once

#include <vector>
#include <map>
#include <set>

#include "Agents/LearningAgent/ExplorerAgentBase.h"
#include "Agents/LearningAgent/ActionRule.h"

#include "ConfigReader.h"

class UnknownRule {
public:
	UnknownRule() {}
	UnknownRule(float rawProb, shared_ptr<Domain> domain, size_t instSize, Literal inGroundedAction);

	float computeProb(State state, /*r*/ float& expectedGain);
	void corroborateFailure(State state);

	Literal groundedAction = Literal();
	float pAny = 0.0f;
	int nAll = 0;
	map<Literal, float> pNfs;
};

class BayesianExplorer : public ExplorerAgentBase {

public:
	BayesianExplorer(bool inVerbose);

	void init(shared_ptr<Domain> inDomain, vector<Term> inInstances, Goal inGoal, shared_ptr<vector<Trace>> inTrace) override;
	void setRules(vector<shared_ptr<ActionRule>> newRules) override;
	void setActionLiterals(set<Literal> baseActionLiterals) override;

	Literal getNextAction(State state) override;
	void updateProblem(vector<Term> inInstances, Goal inGoal, vector<Literal> inHeadstart) override;

	void corroborateRules(Trace trace) override;
	void informRevision(bool knowledgeRevised) override;

	bool receivesEvents = false;

private:
	float revisionProbability(State state, Literal action, bool makeTrace = false);
	float expectedInformationGain(State state, Literal action);
	float computePu(Experiment e, /*r*/ float& expectedGain);
	float computePu(Experiment e);

	void generateRandomPlan(State state);
	void prepareActionSubstitutions();
	set<Literal> getAvailableExperiments(set<Term> newDeleted, State state);
	set<Literal> getAvailableExperiments(set<Term> newDeleted, State state, set<Predicate> actionPreds);
	set<Term> getNotDeleted();
	int metaActionType();

	void makeMotivationTraceJsonObject(State state, Literal action, float revisionProb,
		map<ActionRule*, set<Substitution>> subsPerRule,
		map<ActionRule*, pair<bool, float>> resultsPerRule);
	void saveMotivationTraceFile();

	vector<Literal> allActions;

	vector<float> revisionProbs;

private:
	ConfigReader* bayesianConfig;
	bool random, useStagnation, usePassthrough;
	float gamma, explorationTimeLimit, passthroughThreshold, metaProbability, factRemovalDiscount, randomDiscount, focusSpecificRules;
	int estimatedRulesPerAction, randomPlans, randomExperiments, randomActionTrials, planDepth, stagnationThreshold;

	bool saveMotivationTrace;
	string motivationTraceFileName;
	vector<string> motivationTraceObjects;

	vector<float> positiveProbs, negativeProbs;
	float lastRevProb = -1.0f;
	float posMean = 1.0f;
	float negMean = 0.0f;
	int revsNoProb = 0;
	int revisions = 0;

	int stepsWithoutRevision = 0;

	vector<shared_ptr<ActionRule>> rules;
	set<Literal> actionLiterals;
	set<Predicate> actionPredicates;
	set<Term> deletedInstances;
	
	set<Experiment> allExperiments;
	map<shared_ptr<ActionRule>, set<Experiment>> experimentsPerRule;
	map<Predicate, set<Experiment>> experimentsPerAction;
	shared_ptr<ActionRule> targetRule;

	map<Literal, UnknownRule> unknownRules;

	int iteration = 0;
};
