/**
 * This is a C++ translation of Python Partial-Order-Solver from the book Artificial Intelligence: A Modern Approach,
 * and the GitHub project: https://github.com/aimacode/aima-python
 *
 * Find other languages in which the many algorithms of this book have been implemented at: https://github.com/aimacode
 *
 * @author Thomas Lamson
 * Contact: thomas.lamson@cea.fr
 */

#include "Agents/PartialOrderPlanner/PopAgent.h"

using namespace std;

#define MAX_STEPS 200

void PopAgent::init(shared_ptr<Domain> inDomain, vector<Term> inInstances, Goal inGoal, shared_ptr<vector<Trace>> inTrace) {
	Agent::init(inDomain, inInstances, inGoal, inTrace);

	vector<Condition> goalFacts;
	foreach(fact, goal.trueFacts)
		goalFacts.push_back(Condition(*fact, true));
	foreach(fact, goal.falseFacts)
		goalFacts.push_back(Condition(*fact, false));

	finishPred = Predicate("POP_Finish", 0);
	startPred = Predicate("POP_Start", 0);

	finish = GroundedAction(Literal(finishPred, {}), goalFacts, {});

	foreach(fact, goalFacts)
		agenda.insert({ *fact, finish });

	prepareActionSubstitutions();
}

Literal PopAgent::getNextAction(State state) {
	if (!planReady)
		findPlan(state);

	if (planReady && plan.size() > 0) {
		Literal nextAction = plan.back();
		plan.pop_back();

		if (verbose) cout << plan.size() << " steps remaining." << endl;
		return nextAction;
	}

	return Literal();
}

void PopAgent::updateProblem(vector<Term> inInstances, Goal inGoal, vector<Literal> inHeadstart) {
	causalLinks.clear();
	start = GroundedAction();
	actions.clear();
	constraints.clear();
	agenda.clear();

	vector<Condition> goalFacts;
	foreach(fact, goal.trueFacts)
		goalFacts.push_back(Condition(*fact, true));
	foreach(fact, goal.falseFacts)
		goalFacts.push_back(Condition(*fact, false));
	finish = GroundedAction(Literal(finishPred, {}), goalFacts, {});

	foreach(fact, goalFacts)
		agenda.insert({ *fact, finish });
	
	allGroundedActions.clear();
	Agent::updateProblem(inInstances, inGoal, inHeadstart);
	planReady = false;
	plan.clear();

	prepareActionSubstitutions();
}

void PopAgent::prepareActionSubstitutions() {
	vector<Action> actions = domain->getActions();

	foreach(it, actions) {
		Action action = *it;
		vector<Substitution> subs = Substitution().expandUncovered(action.actionLiteral.parameters, instances + domain->getConstants(), true);

		foreach(subit, subs)
			allGroundedActions.insert(GroundedAction(action, *subit));
	}
}

typedef pair<Condition, unsigned int> Ways;

bool PopAgent::findOpenPreconditions(Condition& subgoal, GroundedAction& action, vector<GroundedAction>& actionsForPrecond) {
	map<Condition, unsigned int> numberOfWays;
	map<Condition, vector<GroundedAction>> actionsForPrecondition;

	foreach(it, agenda) {
		Condition openPrecond = it->first;
		vector<GroundedAction> possibleActions = toVec(actions) + toVec(allGroundedActions);

		foreach(act, possibleActions) {
			foreach(eff, act->postConditions) {
				Literal al = act->actionLiteral;
				Condition el = *eff;

				if (openPrecond == *eff) {
					if (in(numberOfWays, openPrecond)) {
						numberOfWays[openPrecond]++;
						actionsForPrecondition[openPrecond].push_back(*act);
					}
					else {
						numberOfWays[openPrecond] = 1;
						actionsForPrecondition[openPrecond] = { *act };
					}
				}
			}
		}
	}

	vector<Ways> numberOfWaysVec;
	foreach(pair, numberOfWays) {
		if (pair->second == 0) return false;
		numberOfWaysVec.push_back({ pair->first, pair->second });
	}
	sort(numberOfWaysVec.begin(), numberOfWaysVec.end(),
		[](Ways const& a, Ways const& b) { return a.second < b.second; });

	subgoal = numberOfWaysVec[0].first;
	foreach(elem, agenda) {
		if (elem->first == subgoal) {
			action = elem->second;
			break;
		}
	}
	actionsForPrecond = actionsForPrecondition[subgoal];

	// Weird : a precondition is removed from a set of actions...
	// allGroundedActions.erase(subgoal);

	return true;
}

bool visit(map<GroundedAction, set<GroundedAction>>& graph, GroundedAction vertex, set<GroundedAction> path) {
	set<GroundedAction> newPath = path;
	newPath.insert(vertex);
	foreach(it, graph[vertex]) {
		if (in(newPath, *it)) return true;
		if (visit(graph, *it, newPath)) return true;
	}
	return false;
}

bool PopAgent::checkCyclic(set<OrderingConstraint> graph) {
	map<GroundedAction, set<GroundedAction>> mapGraph;
	foreach(it, graph)
		if (mapGraph.find(it->first) == mapGraph.end())
			mapGraph[it->first] = { it->second };
		else
			mapGraph[it->first].insert(it->second);

	foreach(it, mapGraph)
		if (visit(mapGraph, it->first, {}))
			return true;
	return false;
}

bool PopAgent::isAThreat(Condition precondition, Condition effect) {
	if (precondition.truth == effect.truth) return false;
	if (precondition.lit != effect.lit) return false;
	return true;
}

void PopAgent::addConstraint(OrderingConstraint constraint) {
	if (constraint.first == finish || constraint.second == start) return;

	set<OrderingConstraint> newConstraints = constraints;
	newConstraints.insert(constraint);
	if (checkCyclic(newConstraints)) return;

	constraints = newConstraints;
}

void PopAgent::protect(CausalLink causalLink, GroundedAction action) {
	bool threat = false;
	foreach(eff, action.postConditions) {
		if (isAThreat(causalLink.goal, *eff)) {
			threat = true;
			break;
		}
	}

	if (threat && action != causalLink.act1 && action != causalLink.act2) {
		// Try promotion
		set<OrderingConstraint> newConstraints = constraints;
		newConstraints.insert({ action, causalLink.act1 });
		if (!checkCyclic(newConstraints))
			addConstraint({ action, causalLink.act1 });
		else {
			// Try demotion
			newConstraints = constraints;
			newConstraints.insert({ causalLink.act2, action });
			if (!checkCyclic(newConstraints))
				addConstraint({ causalLink.act2, action });
			else if (verbose) cout << "Unable to resolve a threat caused by " << action << " onto " << causalLink.toString() << endl;
		}
	}
}

bool PopAgent::findPlan(State state) {
	int step = 0;

	vector<Condition> startFacts;
	foreach(fact, state.facts)
		startFacts.push_back(Condition(*fact, true));
	start = GroundedAction(Literal(startPred, {}), {}, startFacts);

	actions.insert(start);
	actions.insert(finish);

	while (agenda.size() > 0) {
		step += 1;

		if (step > MAX_STEPS) {
			if (verbose) cout << "Couldn't find a solution in " << MAX_STEPS << " steps." << endl;
			return false;
		}

		Condition subgoal;
		GroundedAction act1;
		vector<GroundedAction> possibleActions;
		if (!findOpenPreconditions(subgoal, act1, possibleActions)) return false;

		GroundedAction act0 = GroundedAction(possibleActions[0]);
		agenda.erase({ subgoal, act1 });
		actions.insert(act0);
		addConstraint(start, act0);

		foreach(it, causalLinks)
			protect(*it, act0);

		foreach(it, act0.preConditions)
			agenda.insert({ *it, act0 });

		addConstraint(act0, act1);

		CausalLink newLink = CausalLink(act0, subgoal, act1);
		if (!in(causalLinks, newLink))
			causalLinks.push_back(CausalLink(act0, subgoal, act1));

		foreach(it, actions)
			protect(newLink, *it);
	}

	// list(reversed(list(self.toposort(self.convert(self.constraints)))))

	vector<GroundedAction> orderedActions;

	// Constructing a graph of the orderings constraints (node -> set of nodes)
	map<GroundedAction, set<GroundedAction>> orderings;
	foreach(cit, constraints)
		if (in(orderings, cit->first))
			orderings[cit->first].insert(cit->second);
		else
			orderings[cit->first] = { cit->second };

	// Removing self-loops and retrieving extra dependencies
	set<GroundedAction> extraElementsInDependencies;
	foreach(pair, orderings) {
		pair->second.erase(pair->first);

		foreach(elt, pair->second)
			if (!in(orderings, *elt))
				extraElementsInDependencies.insert(*elt);
	}

	foreach(elt, extraElementsInDependencies)
		orderings[*elt] = {};

	bool foundZeroDep;
	do {
		foundZeroDep = false;
		GroundedAction performed;

		foreach(pair, orderings) {
			if (pair->second.size() == 0) {
				performed = pair->first;
				foundZeroDep = true;
				break;
			}
		}

		if (foundZeroDep) {
			orderedActions.insert(orderedActions.begin(), performed);

			orderings.erase(performed);
			foreach(pair, orderings)
				pair->second.erase(performed);
		}
	} while (foundZeroDep);
	if (orderings.size() != 0) return false;

	vector<Action> actions = domain->getActions();
	foreach(git, orderedActions)
		plan.push_back(git->actionLiteral);
	planReady = true;

	return true;
}
