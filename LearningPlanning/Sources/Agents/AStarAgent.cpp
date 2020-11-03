/**
 * @author Thomas Lamson
 * Contact: thomas.lamson@cea.fr
 */

#include <assert.h>
#include <math.h>
#include <deque>
#include <chrono>

#include "Agents/AStarAgent.h"

using namespace std;

struct Node {
	shared_ptr<Node> prevNode;
	State state;
	Literal action;
	float cost;
	float heuristic;
	int depth;

	Node(shared_ptr<Node> inPrevNode, State inState, Literal inAction, float inCost, float inHeuristic, int inDepth) :
		prevNode(inPrevNode), state(inState), action(inAction), cost(inCost), heuristic(inHeuristic), depth(inDepth) { }
	Node(float inCost) : prevNode(nullptr), state(State()), action(Literal()), cost(inCost), heuristic(0.0f), depth(0) { }
};

void AStarAgent::init(shared_ptr<Domain> inDomain, vector<Term> inInstances, Goal inGoal, shared_ptr<vector<Trace>> inTrace) {
	Agent::init(inDomain, inInstances, inGoal, inTrace);
	planReady = false;
}

void AStarAgent::updateProblem(vector<Term> inInstances, Goal inGoal, vector<Literal> inHeadstart) {
	Agent::updateProblem(inInstances, inGoal, inHeadstart);

	planReady = false;
	plan.clear();
}

void AStarAgent::setMaxDepth(int newLimit) {
	maxDepth = newLimit;
}

void AStarAgent::setTimeLimit(float seconds) {
	timeLimit = seconds;
}

Literal AStarAgent::getNextAction(State state) {
	chrono::high_resolution_clock::time_point startTime = chrono::high_resolution_clock::now();

	if (planReady && plan.size() > 0) {
		Literal nextAction = plan.back();
		plan.pop_back();

		if (verbose) cout << plan.size() << " steps remaining." << endl;
		return nextAction;
	}
	else if (planReady) {
		return Literal();
	}

	if (verbose) cout << "Planning to achieve goal..." << endl;

	shared_ptr<Node> startNode = make_shared<Node>(nullptr, state, Literal(), 0.0f, heuristic(state), 0);
	
	auto compare = [](shared_ptr<Node> lhs, shared_ptr<Node> rhs) {
		return lhs->heuristic + lhs->cost < rhs->heuristic + rhs->cost;
	};

	set<State> closedList;
	deque<shared_ptr<Node>> openList;

	openList.push_back(startNode);

	while (!openList.empty()) {
		chrono::high_resolution_clock::time_point currTime = chrono::high_resolution_clock::now();
		chrono::duration<double, std::milli> elapsedTime = currTime - startTime;
		if (timeLimit > 0.0f && elapsedTime.count() * 1.0f / 1000.0f > timeLimit) {
			if (verbose) cout << "Planning time limit exceeded" << endl;
			return Literal();
		}

		if (verbose) cout << "\rOpen list: " << openList.size() << "                ";

		shared_ptr<Node> current = openList.front();
		openList.pop_front();

		if (current->heuristic == 0.0f) {
			while (current->prevNode != nullptr) {
				plan.push_back(current->action);
				current = current->prevNode;
			}
			planReady = true;

			if (verbose) cout << "Plan found: " << plan.size() << " steps." << endl;

			return getNextAction(state);
		}
		else {
			vector<Literal> available = getAvailableActions(current->state);
			
			if (maxDepth > 0 && current->depth >= maxDepth)
				available.clear();

			float newCost = current->cost + 1;

			shared_ptr<Node> fakeNode = make_shared<Node>(newCost);
			__int64 lowerBound = distance(openList.begin(), lower_bound(openList.begin(), openList.end(), fakeNode, compare));

			foreach(actit, available) {
				State newState = domain->tryAction(current->state, instances, *actit).obj;
				float newHeuristic = heuristic(newState);
				
				if (closedList.find(newState) == closedList.end()) {
					bool found = false;
					foreach(openit, openList) {
						if ((*openit)->state == newState && (*openit)->cost < newCost) {
							found = true;
							break;
						}
					}
					if (!found) {
						shared_ptr<Node> newNode = make_shared<Node>(current, newState, *actit, newCost, newHeuristic, current->depth + 1);
						openList.insert(lower_bound(openList.begin() + lowerBound, openList.end(), newNode, compare), newNode);
					}
				}
			}

			closedList.insert(current->state);
		}
	}

	return Literal();
}

float AStarAgent::heuristic(State state) {
	float count = 0;

	foreach(it, goal.trueFacts)
		if (!state.contains(*it))
			count++;
	foreach(it, goal.falseFacts)
		if (state.contains(*it))
			count++;

	return count * 1.0f;
}
