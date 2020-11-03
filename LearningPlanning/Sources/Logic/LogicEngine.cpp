/**
 * @author Thomas Lamson
 * Contact: thomas.lamson@cea.fr
 */

#include "Logic/LogicEngine.h"
#include "Logic/JSON_Parsing.h"
#include <chrono>
#include <iostream>
#include <iomanip>

LogicEngine::LogicEngine() {
	currentState = State();
}

LogicEngine::~LogicEngine() {

}

void LogicEngine::loadDomain(string path) {
	JsonParser parser = JsonParser();
	domain = parser.parseDomain(path);
}

void LogicEngine::loadProblem(string path) {
	loadProblem(path, "");
}

void LogicEngine::loadProblem(string path, string headstartPath) {
	JsonParser parser = JsonParser(domain);
	problem = parser.parseProblem(path, headstartPath);

	currentState = problem->initialState;
	instances = toVec(problem->instances);

	if (agent) {
		agent->updateProblem(instances, problem->goal, problem->headstartActions);
	}

	if (renderer) {
		renderer->renderState(currentState, instances);
	}

	if (domain) {
		domain->setResetState(problem->initialState);
	}
}

void LogicEngine::init(shared_ptr<Agent> inAgent, shared_ptr<DomainTester> domainTester, shared_ptr<RandomStateGenerator> inStateGenerator,
					   DomainRenderer* inDomainRenderer, SDL_Renderer* sdl_renderer, SDL_Window* window) {

	trace = make_shared<vector<Trace>>();

	agent = inAgent;
	agent->init(domain, instances, {}, trace);
	agent->setDomainTester(domainTester);
	agent->setEngine(this);

	stateGenerator = inStateGenerator;

	if (problem)
		agent->updateProblem(instances, problem->goal, problem->headstartActions);

	renderer = inDomainRenderer;
	renderer->init(domain, sdl_renderer, window);
	renderer->renderState(currentState, instances);
}

void LogicEngine::setRandomState() {
	setState(stateGenerator->generateState());
}

void LogicEngine::setState(State newState) {
	trace->push_back(Trace(currentState, Literal(), false, newState));
	currentState = newState;
	renderer->renderState(currentState, instances);
}

void LogicEngine::step() {
	chrono::high_resolution_clock::time_point startTime = chrono::high_resolution_clock::now();

	Literal instAct = agent->getNextAction(currentState);

	if (instAct != Literal()) {
		State prevState = currentState;
		bool authorized = false;

		Opt<State> optState = domain->tryAction(currentState, instances, instAct);

		if (optState.there) {
			currentState = optState.obj;
			renderer->renderState(currentState, instances);
			authorized = true;
		}
		else {
			//cout << "Action not allowed." << endl;
		}

		trace->push_back(Trace(prevState, instAct, authorized, currentState));
	}

	chrono::high_resolution_clock::time_point endTime = chrono::high_resolution_clock::now();
	chrono::duration<double, std::milli> elapsedTime = endTime - startTime;
	//cout << "Elapsed time: " << fixed << setprecision(3) << elapsedTime.count() / 1000 << "s" << endl;
}

void LogicEngine::handleEvent(SDL_Event event) {
	if (agent->receivesEvents) {
		agent->handleEvent(event);
	}
}
