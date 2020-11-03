/**
 * This is a C++ translation of Python Partial-Order-Solver from the book Artificial Intelligence: A Modern Approach,
 * and the GitHub project: https://github.com/aimacode/aima-python
 *
 * Find other languages in which the many algorithms of this book have been implemented at: https://github.com/aimacode
 *
 * @author Thomas Lamson
 * Contact: thomas.lamson@cea.fr
 */

#pragma once

#include <vector>
#include <map>
#include <set>

#include "Agents/Agent.h"
#include "Logic/Domain.h"
#include "Utils.h"

using namespace std;

typedef pair<GroundedAction, GroundedAction> OrderingConstraint;
typedef pair<Condition, GroundedAction> AgendaElem;

struct CausalLink {
	GroundedAction act1, act2;
	Condition goal;

	CausalLink(GroundedAction inAct1, Condition inGoal, GroundedAction inAct2) : act1(inAct1), goal(inGoal), act2(inAct2) { }

	bool operator==(CausalLink const& other) const {
		return act1 == other.act1 && act2 == other.act2 && goal == other.goal;
	}

	string toString() {
		return "(" + act1.toString() + ", " + goal.toString() + ", " + act2.toString() + ")";
	}
};

class PopAgent : public Agent {
public:
	PopAgent(bool inVerbose) : Agent(inVerbose) {}

	void init(shared_ptr<Domain> inDomain, vector<Term> inInstances, Goal inGoal, shared_ptr<vector<Trace>> inTrace) override;

	Literal getNextAction(State state) override;
	void updateProblem(vector<Term> inInstances, Goal inGoal, vector<Literal> inHeadstart) override;

	bool receivesEvents = false;

protected:
	void prepareActionSubstitutions();

	bool findOpenPreconditions(Condition& subgoal, GroundedAction& action, vector<GroundedAction>& actionsForPrecond);
	bool checkCyclic(set<OrderingConstraint> graph);
	bool isAThreat(Condition precondition, Condition effect);
	void addConstraint(OrderingConstraint constraint);
	void addConstraint(GroundedAction before, GroundedAction after) {
		addConstraint(OrderingConstraint(before, after));
	}
	void protect(CausalLink causalLink, GroundedAction action);

	bool findPlan(State state);

	set<GroundedAction> allGroundedActions;

	bool planReady = false;
	vector<Literal> plan;
	
	vector<CausalLink> causalLinks;
	Predicate startPred, finishPred;
	GroundedAction start, finish;
	set<GroundedAction> actions;
	set<OrderingConstraint> constraints;
	set<AgendaElem> agenda;
};
