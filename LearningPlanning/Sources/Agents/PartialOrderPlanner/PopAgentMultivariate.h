/**
 * @author Thomas Lamson
 * Contact: thomas.lamson@cea.fr
 */

#pragma once

#include <vector>
#include <set>
#include <map>
#include <memory>

#include "Agents/Agent.h"
#include "Logic/Domain.h"
#include "Utils.h"

using namespace std;

typedef tuple<size_t, Condition, size_t> CausalLink;
typedef pair<Condition, size_t> OpenPrecondition;
typedef map<size_t, set<size_t>> OrderingGraph;

struct PartialPlan {
	float heuristic = 0.0f;
	float cost = 0.0f;
	shared_ptr<PartialPlan> parent;
	vector<string> history;

	Literal startLiteral;
	Literal finishLiteral;
	GroundedAction startAction; // Index 0
	GroundedAction finishAction; // Index 1

	vector<GroundedAction> actions;
	set<CausalLink> causalLinks;
	OrderingGraph constraints;
	set<OpenPrecondition> openPreconditions;

	PartialPlan(State startState, Goal finishGoal, Literal inStartLiteral, Literal inFinishLiteral);
	PartialPlan(PartialPlan const& other);

	void computeNextChoices(vector<GroundedAction> availableActions,
							vector<shared_ptr<PartialPlan>>& /*r*/ newPlans);
	void updateHeuristic();
	bool goalCheck();
	bool extractPlan(vector<GroundedAction>& /*r*/ plan);

private:
	bool directlyBindPrecondition(OpenPrecondition precond, size_t toAction, Substitution sub);
	bool applyActionToPrecondition(OpenPrecondition precond, GroundedAction action, Substitution sub);
	bool bindVariables(Substitution sub);
	void addConstraint(size_t before, size_t after);
	void removeConstraint(size_t before, size_t after);
	bool hasConstraint(size_t before, size_t after);
	set<size_t> getConstraints(size_t before);
	bool protectCausalLink(CausalLink link, size_t fromAction);
	bool checkCyclicity();
	bool hasDirectBinding(OpenPrecondition precond);
	set<pair<size_t, Substitution>> getDirectBindings(OpenPrecondition precond);
	bool isAAfterB(size_t actionA, size_t actionB);
};

class PopAgentMultivariate : public Agent {
public:
	PopAgentMultivariate(bool inVerbose) : Agent(inVerbose) {}

	void init(shared_ptr<Domain> inDomain, vector<Term> inInstances, Goal inGoal, shared_ptr<vector<Trace>> inTrace) override;

	Literal getNextAction(State state) override;
	void updateProblem(vector<Term> inInstances, Goal inGoal, vector<Literal> inHeadstart) override;

	bool receivesEvents = false;

protected:
	void prepareActionSubstitutions();
	bool findPlan(State state);
	void extractPlan(shared_ptr<PartialPlan> partialPlan);

	bool planReady = false;
	vector<Literal> plan;
	vector<GroundedAction> availableActions;

	Literal startLiteral, finishLiteral;
};

/*
DONE
> At each point :
> 1. A precondition can be answered with a new action (variable binding if necessary)
> 2. A precondition can be answered with an existing action (variable binding if necessary)

DONE
> Therefore, a search choice consists in :
> 1. Choosing a precondition to fulfill
> 2. Choosing an existing or a new action to fulfill it
> --> Lots of choices !

DONE
> As every precondition must be answered, the order in which we answer them does not matter.
> Actually, keeping the order as a choice will duplicate the entries in the search space, so it shouldn't be considered.
> As an heuristic, preconditions that offer the least choices of action are to be fulfilled first, and it is not a choice.
> Therefore, backtracking only occurs on action choices.
> --> Less choices !

If a cyclicity is created in the causal constraints, it means the current plan is not viable.
We should backtrack some choices if this happens.

DONE
> Causal threats in multivariate setting:
> 1. A constant to constant threat must be handled right away (identity of symbols).
> 2. A variable to variable threat must be handled right away (identity of symbols).
> --> No need to do more than that, just check every causal link any time an action is chosen and a variable is bound.
> (book keeping might come handy, but it is only an optimisation)

DONE
> Heuristic over plans:
> - (-) Number of open preconditions --> good metric
> - (-) Number of threats
> - (-) Number of actions
> We could decrease the number of open preconditions by how many of them can be answered directly by existing actions (without binding variables).

A stack of active plans must be kept.
Given an active plan and an evolution choice, every possible choice leads to a new active plan in the stack, as in A* search.
Heuristics is computed for each new plan, and the stack is sorted from best to worst heuristic.

DONE
> A partial plan is defined by:
> - The set of actions taken (action objects with current variable bindings)
> - Causal links between effects and preconditions (fulfillment)
> - Ordering constraints of actions
> - Set of open preconditions (can be computed from the rest but it's good to keep it updated in memory)
*/
