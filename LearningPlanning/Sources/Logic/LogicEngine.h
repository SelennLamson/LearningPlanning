/**
 * @author Thomas Lamson
 * Contact: thomas.lamson@cea.fr
 */

#pragma once

#include <string>
#include <vector>
#include <memory>
#include "SDL.h"

#include "Logic/Domain.h"
#include "Logic/DomainTester.h"
#include "Logic/RandomStateGenerator.h"
#include "Agents/Agent.h"
#include "Render/DomainRenderer.h"

using namespace std;

class Agent;

class LogicEngine {
public:
	LogicEngine();
	~LogicEngine();

	void loadDomain(string path);
	void loadProblem(string path);
	void loadProblem(string path, string headstartPath);

	void init(shared_ptr<Agent> inAgent, shared_ptr<DomainTester> domainTester, shared_ptr<RandomStateGenerator> inStateGenerator,
			  DomainRenderer* inDomainRenderer, SDL_Renderer* sdl_renderer, SDL_Window* window);

	void setRandomState();
	void setState(State newState);
	void step();
	void handleEvent(SDL_Event event);

public:
	shared_ptr<Agent> agent;
	shared_ptr<Domain> domain;
	shared_ptr<Problem> problem;

	shared_ptr<RandomStateGenerator> stateGenerator;

	State currentState;
	vector<Term> instances;
	shared_ptr<vector<Trace>> trace;

	DomainRenderer* renderer;
};
