/**
 * @author Thomas Lamson
 * Contact: thomas.lamson@cea.fr
 */

#include "Agents/DataGeneratorAgent.h"

#include <ctime>
#include <iomanip>
#include <sstream>
#include <iostream>
#include <fstream>
#include <chrono>

#include "Logic/LogicEngine.h"

#define SAMPLES_PER_ACTION 100
#define PROBLEMS 20
#define MAX_PLAN_SIZE 4

void DataGeneratorAgent::init(shared_ptr<Domain> inDomain, vector<Term> inInstances, Goal inGoal, shared_ptr<vector<Trace>> inTrace) {
	Agent::init(inDomain, inInstances, inGoal, inTrace);
	prepareActionSubstitutions();
}

void DataGeneratorAgent::prepareActionSubstitutions() {
	vector<InstantiatedAction> allInstActs;

	allActions.clear();
	samples.clear();
	problems.clear();
	vector<Action> actions = domain->getActions();

	vector<Term> allInsts = instances + domain->getConstants();

	foreach(it, actions) {
		Action action = *it;
		insertUnique(&actionPredicates, action.actionLiteral.pred);
		samples[action.actionLiteral.pred] = {};

		vector<Substitution> subs = Substitution().expandUncovered(action.actionLiteral.parameters, allInsts, true);

		foreach(subit, subs) {
			allInstActs.push_back(InstantiatedAction(action, *subit));
			allActions.push_back(subit->apply(action.actionLiteral));
		}
	}
}

Literal DataGeneratorAgent::getNextAction(State state) {
	if (trace->size() > 0) {
		Trace tr = trace->back();
		Predicate pred = tr.instAct.pred;

		if (in(samples, pred) && samples[pred].size() < SAMPLES_PER_ACTION && !in(samples[pred], tr))
			samples[pred].push_back(tr);
	}

	bool generate = false;
	float progress = 0.0f;
	foreach(pair, samples) {
		progress += pair->second.size() * 1.0f / (SAMPLES_PER_ACTION * 1.0f);
		generate |= pair->second.size() < SAMPLES_PER_ACTION;
	}
	progress /= samples.size() * 1.0f;
	cout << "\rProgress: " << (int)(progress * 100.0f) << "%       ";

	if (generate) {
		vector<Literal> available = getAvailableActions(state);

		if (static_cast<float>(rand()) / static_cast<float>(RAND_MAX) < 0.01) {
			engine->setRandomState();
			return Literal();
			//return Literal(domain->getActionPredByName("reset"), {});
		}

		return *select_randomly(available.begin(), available.end());
	}

	map<State, vector<pair<Literal, State>>> graph;
	foreach(pred, actionPredicates) {
		vector<Trace> traces = samples[*pred];
		foreach(tr, traces) 
			if (tr->authorized) {
				if (in(graph, tr->state)) graph[tr->state].push_back({ tr->instAct, tr->newState });
				else graph[tr->state] = { { tr->instAct, tr->newState } };
			}
	}

	for (int p = 0; p < PROBLEMS; p++) {
		State initState = select_randomly(graph.begin(), graph.end())->first;
		State currentState = initState;

		set<State> visited = { initState };
		for (int s = 0; s < MAX_PLAN_SIZE; s++) {
			if (!in(graph, currentState))
				break;
			vector<pair<Literal, State>> links = graph[currentState];
			
			int trials = 100;
			while (trials > 0) {
				trials--;
				pair<Literal, State> litState = *select_randomly(links.begin(), links.end());
				if (in(visited, litState.second))
					continue;
				if (!in(graph, litState.second) && s < MAX_PLAN_SIZE / 2)
					continue;

				currentState = litState.second;
				break;
			}

			visited.insert(currentState);
		}

		set<Literal> added, deleted;
		initState.difference(currentState, &added, &deleted);
		Goal g;
		g.trueFacts = toVec(added);
		g.falseFacts = toVec(deleted);

		problems.push_back({ initState, g });
	}

	// Register traces into file
	auto t = time(nullptr);
	tm tm;
	localtime_s(&tm, &t);

	ostringstream oss;
	oss << "Data/generated_traces_";
	oss << put_time(&tm, "%d-%m_%H-%M");
	oss << ".json";

	ofstream genFile;
	genFile.open(oss.str());
	genFile << "{\"samples\":[";

	foreachindex(i, actionPredicates) {
		Predicate pred = actionPredicates[i];
		vector<Trace> traces = samples[pred];

		genFile << "{\"pred\":\"" << pred.name << "\",\"traces\":[";

		foreachindex(j, traces) {
			Trace tr = traces[j];

			string stateStr = "[\"" + join("\",\"", tr.state.facts) + "\"]";
			string actionStr = "\"" + tr.instAct.toString() + "\"";
			string nextStr = "[\"" + join("\",\"", tr.newState.facts) + "\"]";

			genFile << "{\"state\":" + stateStr + ",\"action\":" + actionStr + ",\"next\":" + nextStr + "}";

			if (j < traces.size() - 1)
				genFile << ",";
		}

		genFile << "]}";
		if (i < actionPredicates.size() - 1)
			genFile << ",";
	}
	genFile << "],\"problems\":[";

	foreachindex(pId, problems) {
		State s = problems[pId].first;
		Goal g = problems[pId].second;

		string stateStr = "[\"" + join("\",\"", s.facts) + "\"]";
		string gPosStr = "[\"" + join("\",\"", g.trueFacts) + "\"]";
		string gNegStr = "[\"" + join("\",\"", g.falseFacts) + "\"]";

		genFile << "{\"init\":" + stateStr + ",\"goalpos\":" + gPosStr + ",\"goalneg\":" + gNegStr + "}";

		if (pId < problems.size() - 1)
			genFile << ",";
	}

	genFile << "]}";

	/*
	{
	  "samples":
		[
		  {
			"pred": "push",
			"traces": [
			  {
				"state": ["f1", "f2"],
				"action": "push(a, b)",
				"next": ["f1", "f3"]
			  },
			  {
				"state": ["f1", "f2"],
				"action": "push(a, b)",
				"next": ["f1", "f3"]
			  }
			]
		  },
		  {
			"pred": "paint",
			"traces": [
			  {
				"state": ["f1", "f2"],
				"action": "paint(a)",
				"next": ["f1", "f3"]
			  },
			  {
				"state": ["f1", "f2"],
				"action": "paint(a)",
				"next": ["f1", "f3"]
			  }
			]
		  }
		],
	  "problems":
		[
		  {
			"init": ["f1", "f2"],
			"goalpos": ["f3", "f4"],
			"goalneg": ["f5", "f6"]
		  },
		  {
			"init": ["f1", "f2"],
			"goalpos": ["f3", "f4"],
			"goalneg": ["f5", "f6"]
		  }
		]
	}
	*/

	genFile.close();
	while (true) cout << "\rEND                                          ";
	return Literal();
}

void DataGeneratorAgent::updateProblem(vector<Term> inInstances, Goal inGoal, vector<Literal> inHeadstart) {
	Agent::updateProblem(inInstances, inGoal, inHeadstart);
	prepareActionSubstitutions();
}
