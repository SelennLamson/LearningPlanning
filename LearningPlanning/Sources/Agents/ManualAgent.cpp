/**
 * @author Thomas Lamson
 * Contact: thomas.lamson@cea.fr
 */

#include "Agents/ManualAgent.h"
#include <iostream>
#include <sstream>

void ManualAgent::init(shared_ptr<Domain> inDomain, vector<Term> inInstances, Goal inGoal, shared_ptr<vector<Trace>> inTrace) {
	Agent::init(inDomain, inInstances, inGoal, inTrace);
}

float heuristic(Goal goal, State state) {
	float count = 0;

	foreach(it, goal.trueFacts)
		if (!state.contains(*it))
			count++;
	foreach(it, goal.falseFacts)
		if (state.contains(*it))
			count++;

	return count;
}

Literal ManualAgent::getNextAction(State state) {
	string command, tmp;
	getline(cin, command);

	Predicate action;
	vector<Term> params;
	vector<Term> allInsts = instances + domain->getConstants();
	bool first = true;
	char c = ' ';
	tmp = "";

	for (unsigned int i = 0; i < command.length() + 1; i++) {
		if (i < command.length())
			c = command[i];

		if ((i == command.length() || c == ' ' || c == '(' || c == ')' || c == ',') && action.name != "remove-fact") {
			if (tmp != "") {
				if (first) {
					first = false;
					action = domain->getActionPredByName(tmp);
					std::cout << action.name << endl;
					if (action == Predicate()) {
						std::cout << "Didn't recognized action \"" << tmp << "\"." << endl;
					}
				}
				else {
					Term inst;
					bool found = false;
					foreach(it, allInsts) {
						if (it->name == tmp) {
							inst = *it;
							found = true;
							break;
						}
					}
					if (found)
						params.push_back(inst);
					else {
						std::cout << "Didn't recognized object \"" << tmp << "\"." << endl;
						return Literal();
					}
				}

				tmp = "";
			}
		}
		else {
			tmp += c;
		}
	}

	if (action.name == "remove-fact") {
		if (tmp == "") {
			return Literal(action, {});
		}

		Term p = Instance(tmp, 0);
		Literal actionLiteral = Literal(action, { p });
		std::cout << "Removing fact: " << tmp << endl;
		return actionLiteral;
	}
 
	if (params.size() != action.arity) {
		std::cout << "Action " << action.name << " requires " << action.arity << " arguments." << endl;
		return Literal();
	}
	Literal actionLiteral = Literal(action, params);

	std::cout << "Action submitted: " << actionLiteral << ". Heuristic = " << heuristic(goal, domain->tryAction(state, instances, actionLiteral).obj) << endl;
	return actionLiteral;
}
