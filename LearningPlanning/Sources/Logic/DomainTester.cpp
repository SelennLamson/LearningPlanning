/**
 * @author Thomas Lamson
 * Contact: thomas.lamson@cea.fr
 */

#include "Logic/DomainTester.h"
#include "Agents/AStarAgent.h"

#include <iostream>
#include <sstream>
#include <fstream>

#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"

#define MAX_PLAN_SIZE 5
#define PLAN_TIME_LIMIT 0.1f

using namespace rapidjson;

void DomainTester::init(shared_ptr<Domain> inDomain, shared_ptr<Problem> inProblem, string datasetPath, int inTestProblems) {
	testProblems = inTestProblems;
	initialized = true;

	groundTruthDomain = inDomain;
	trace = make_shared<vector<Trace>>();

	instances = toVec(inProblem->instances);

	samples.clear();

	std::string line, text;
	std::ifstream in(datasetPath);
	while (std::getline(in, line)) text += line + "\n";
	in.close();
	const char* data = text.c_str();

	Document d;
	d.Parse(data);

	auto arr = d["samples"].GetArray();
	for (auto it = arr.Begin(); it != arr.End(); it++) {
		auto entry = it->GetObject();

		Predicate pred = groundTruthDomain->getActionPredByName(entry["pred"].GetString());
		
		auto traces = entry["traces"].GetArray();
		vector<Trace> parsedTraces;
		for (auto trIt = traces.Begin(); trIt != traces.End(); trIt++) {
			auto trace = trIt->GetObject();

			State state;
			auto stateFacts = trace["state"].GetArray();
			for (auto fIt = stateFacts.Begin(); fIt != stateFacts.End(); fIt++)
				state.addFact(groundTruthDomain->parseLiteral(fIt->GetString(), instances, false));

			State nextState;
			auto nextStateFacts = trace["next"].GetArray();
			for (auto fIt = nextStateFacts.Begin(); fIt != nextStateFacts.End(); fIt++)
				nextState.addFact(groundTruthDomain->parseLiteral(fIt->GetString(), instances, false));

			Literal action = groundTruthDomain->parseLiteral(trace["action"].GetString(), instances, true);

			parsedTraces.push_back(Trace(state, action, state == nextState, nextState));
		}

		samples[pred] = parsedTraces;
	}

	vector<Problem> candidateProblems;

	auto prArr = d["problems"].GetArray();
	for (auto it = prArr.Begin(); it != prArr.End(); it++) {
		auto entry = it->GetObject();

		State state;
		auto initState = entry["init"].GetArray();
		for (auto fIt = initState.Begin(); fIt != initState.End(); fIt++)
			state.addFact(groundTruthDomain->parseLiteral(fIt->GetString(), instances, false));

		Goal goal;
		auto goalPositive = entry["goalpos"].GetArray();
		auto goalNegative = entry["goalneg"].GetArray();
		for (auto gIt = goalPositive.Begin(); gIt != goalPositive.End(); gIt++)
			goal.trueFacts.push_back(groundTruthDomain->parseLiteral(gIt->GetString(), instances, false));
		for (auto gIt = goalNegative.Begin(); gIt != goalNegative.End(); gIt++)
			goal.falseFacts.push_back(groundTruthDomain->parseLiteral(gIt->GetString(), instances, false));

		candidateProblems.push_back(Problem(groundTruthDomain, inProblem->instances, state, goal));
	}

	shared_ptr<AStarAgent> gtPlanner = make_shared<AStarAgent>(false);
	gtPlanner->init(groundTruthDomain, instances, inProblem->goal, trace);
	gtPlanner->setMaxDepth(MAX_PLAN_SIZE);
	gtPlanner->setTimeLimit(PLAN_TIME_LIMIT);

	foreach(pr, candidateProblems) {
		gtPlanner->updateProblem(instances, pr->goal, {});

		State s = pr->initialState;
		bool success = false;
		while (true) {
			Literal act = gtPlanner->getNextAction(s);

			if (act == Literal())
				break;
			
			Opt<State> opt = groundTruthDomain->tryAction(s, instances, act);

			if (!opt.there)
				break;

			s = opt.obj;
			if (pr->goal.reached(s)) {
				success = true;
				break;
			}
		}

		if (success)
			problems.push_back(*pr);
	}

	if (testProblems > problems.size())
		testProblems = (int)problems.size();

	std::cout << "Loaded " << samples.size() * samples.begin()->second.size() << " samples for evaluation." << endl;
	std::cout << "Loaded " << candidateProblems.size() << " problems for planning evaluation, ground-truth solved " << problems.size() << " of them." << endl;
}

void DomainTester::testDomain(shared_ptr<Domain> testedDomain, /*r*/ float& variationalDistance, /*r*/ float& planningDistance) {
	/*vector<Action> actions = testedDomain->getActions(false);
	foreach(act, actions) {
		cout << endl;
		cout << "Literal: " << act->actionLiteral << endl;
		cout << "Parameters: " << join(", ", act->parameters) << endl;
		cout << "Preconds: " << join(", ", act->truePrecond) << endl;
		cout << "Add: " << join(", ", act->add) << endl;
		cout << "Del: " << join(", ", act->del) << endl;
	}*/

	if (!initialized) {
		variationalDistance = planningDistance = -1.0f;
		return;
	}

	int successes = 0;
	int totalCount = 0;

	std::cout << endl;

	foreach(pair, samples)
		foreach(trace, pair->second) {
			if (testedDomain->tryAction(trace->state, instances, trace->instAct).obj == trace->newState)
				successes++;
			totalCount++;
		}

	variationalDistance = 1.0f - (successes * 1.0f) / (totalCount * 1.0f);

	shared_ptr<AStarAgent> testPlanner = make_shared<AStarAgent>(false);
	testPlanner->init(testedDomain, instances, problems[0].goal, trace);
	testPlanner->setMaxDepth(MAX_PLAN_SIZE);
	testPlanner->setTimeLimit(PLAN_TIME_LIMIT);

	float testPlannerScore = 0.0f;

	set<vector<Problem>::iterator> selectedProblemsIts;
	while (selectedProblemsIts.size() < testProblems) {
		vector<Problem>::iterator pr = select_randomly(problems.begin(), problems.end());
		selectedProblemsIts.insert(pr);
	}
	vector<Problem> selectedProblems;
	foreach(it, selectedProblemsIts)
		selectedProblems.push_back(**it);

	foreachindex(prId, selectedProblems) {
		std::cout << "\rEvaluating domain... " << prId + 1 << " / " << testProblems << "        ";

		Problem pr = problems[prId];
		testPlanner->updateProblem(instances, pr.goal, {});

		State s = pr.initialState;
		bool success = false;
		while (true) {
			Literal act = testPlanner->getNextAction(s);

			if (act == Literal())
				break;

			Opt<State> opt = groundTruthDomain->tryAction(s, instances, act);

			if (!opt.there)
				break;

			s = opt.obj;
			if (pr.goal.reached(s)) {
				success = true;
				break;
			}
		}

		if (success)
			testPlannerScore++;
	}

	planningDistance = 1 - (testPlannerScore * 1.0f / (testProblems * 1.0f));

	std::cout << "Var. Dist.: " << variationalDistance << " - Plan. Dist.: " << planningDistance << "                    " << endl;
}
