/**
 * @author Thomas Lamson
 * Contact: thomas.lamson@cea.fr
 */

#pragma once

#include <windows.h>
#include <memory>

#include "SDL.h"
#include <ctime>
#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <chrono>

#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"

#include "Logic/LogicEngine.h"
#include "Logic/DomainTester.h"
#include "Logic/RandomStateGenerator.h"

#include "Agents/AStarAgent.h"
#include "Agents/ManualAgent.h"
#include "Agents/RandomExploreAgent.h"
#include "Agents/FFAgent.h"
#include "Agents/LearningAgent/LearningAgent.h"
#include "Agents/Strips/StripsAgent.h"
#include "Agents/PartialOrderPlanner/PopAgentMultivariate.h"
#include "Agents/DataGeneratorAgent.h"

#include "Render/BlocksWorldRenderer.h"
#include "Render/LogisticsRenderer.h"
#include "Render/ComplexWorldRenderer.h"
#include "Render/SokobanRenderer.h"

#include "Utils.h"
#include "ConfigReader.h"

using namespace std;

map<string, tuple<uint8_t, uint8_t, uint8_t>> colorMap = {
   {"red", { 255, 50, 100 }},
   {"blue", { 50, 100, 255 }},
   {"green", { 100, 255, 50 }},
   {"white", { 255, 255, 255 }},
   {"black", { 0, 0, 0 }}
};

bool debugPrints = false;

ConfigReader* config = nullptr;

int main(int argc, char **argv) {
	string configFilePath = "config.json";
	if (argc > 1)
		configFilePath = argv[1];

	string displayPath = configFilePath;
	if (displayPath.length() > 30)
		displayPath = "..." + displayPath.substr(displayPath.length() - 28);

	string windowTitle = "Learning Planning Domain - CONFIG: " + displayPath;
	SetConsoleTitle(windowTitle.c_str());

	config = ConfigReader::fromFile(configFilePath);

	string agentName = config->getString("agent");
	string domain = config->getString("domain");
	
	vector<string> problems;
	cJSON* prbs = config->getArray("problems");
	cJSON* elem;
	cJSON_ArrayForEach(elem, prbs)
		problems.push_back(elem->valuestring);

	vector<string> headstarts;
	cJSON* hdst = config->getArray("headstart");
	cJSON_ArrayForEach(elem, hdst)
		headstarts.push_back(elem->valuestring);

	int width = config->getInt("width");
	int height = config->getInt("height");
	bool verbose = config->getBool("verbose");
	debugPrints = config->getBool("debug");
	int waitMs = config->getInt("waitms");

	bool running = false;
	bool runstill = config->getBool("defaultauto");
	bool useHeadstart = config->getBool("useheadstart");
	int flags = SDL_WINDOW_SHOWN;

	ConfigReader* testConfig = config->getSubconfig("test");
	bool testing = testConfig->getBool("performtests");
	string testFile = testConfig->getString("testfile");
	string testingProblem = testConfig->getString("testproblem");
	int testProblems = testConfig->getInt("planningproblems");

	bool useSeed = config->getUint("seed") != 0;
	unsigned int seed = 0;
	if (useSeed)
		seed = config->getUint("seed");

	SDL_Window* window = nullptr;
	SDL_Renderer* renderer = nullptr;

	if (SDL_Init(SDL_INIT_EVERYTHING) == 0) {
		std::cout << "Substystem initialized." << std::endl;

		window = SDL_CreateWindow(windowTitle.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, flags);
		if (window) {
			std::cout << "Window created." << std::endl;

			renderer = SDL_CreateRenderer(window, -1, 0);
			if (renderer) {
				std::cout << "Renderer created." << std::endl;

				SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
				running = true;
			}
		}
	}
	
	// Initialize logic system
	unique_ptr<LogicEngine> engine = make_unique<LogicEngine>();
	size_t problemIndex = 0;

	string basePath = "data/";
	string extension = ".json";
	string headstartExt = ".txt";

	engine->loadDomain(basePath + domain + extension);
	engine->loadProblem(basePath + testingProblem + extension);

	shared_ptr<DomainTester> domainTester = make_shared<DomainTester>();
	if (testing)
		domainTester->init(engine->domain, engine->problem, basePath + testFile, testProblems);

	if (useHeadstart && headstarts.size() > problemIndex)
		engine->loadProblem(basePath + problems[problemIndex] + extension,
							basePath + headstarts[problemIndex] + headstartExt);
	else
		engine->loadProblem(basePath + problems[problemIndex] + extension);

	shared_ptr<RandomStateGenerator> stateGenerator = make_shared<RandomStateGenerator>();
	stateGenerator->init(engine->domain, engine->problem, domain);
	if (useSeed)
		stateGenerator->setSeed(seed);

	shared_ptr<Agent> agent = nullptr;
	DomainRenderer* domainRenderer = nullptr;

	if (agentName == "AStarAgent")					agent = make_shared<AStarAgent>(verbose);
	else if (agentName == "ManualAgent")			agent = make_shared<ManualAgent>(verbose);
	else if (agentName == "RandomExploreAgent")		agent = make_shared<RandomExploreAgent>(verbose);
	else if (agentName == "FFAgent")				agent = make_shared<FFAgent>(verbose);
	else if (agentName == "LearningAgent")			agent = make_shared<LearningAgent>(verbose);
	else if (agentName == "StripsAgent")			agent = make_shared<StripsAgent>(verbose);
	else if (agentName == "PopAgentMultivariate")	agent = make_shared<PopAgentMultivariate>(verbose);
	else if (agentName == "DataGeneratorAgent")		agent = make_shared<DataGeneratorAgent>(verbose);

	if (domain == "logistics")			domainRenderer = new LogisticsRenderer();
	if (domain == "logistics_onebox")	domainRenderer = new LogisticsRenderer();
	if (domain == "blocksworld")		domainRenderer = new BlocksWorldRenderer();
	if (domain == "colorblocksworld")	domainRenderer = new BlocksWorldRenderer();
	if (domain == "complex")			domainRenderer = new ComplexWorldRenderer();
	if (domain == "complex_lessvars")	domainRenderer = new ComplexWorldRenderer();
	if (domain == "sokoban")			domainRenderer = new SokobanRenderer();

	engine->init(agent, domainTester, stateGenerator, domainRenderer, renderer, window);

	chrono::high_resolution_clock::time_point lastUpdate = chrono::high_resolution_clock::now();
	while (running) {
		// Handling events
		SDL_Event event;
		SDL_PollEvent(&event);
		bool handled = false;
		switch (event.type) {
		case SDL_QUIT:
			running = false;
			handled = true;
			break;
		case SDL_KEYDOWN:
			if (event.key.keysym.sym == SDLK_SPACE) {
				engine->step();
				handled = true;
			}
			else if (event.key.keysym.sym == SDLK_a) {
				runstill = !runstill;
				handled = true;
			}
			else if (event.key.keysym.sym == SDLK_r) {
				if (useHeadstart && headstarts.size() > problemIndex)
					engine->loadProblem(basePath + problems[problemIndex] + extension,
						basePath + headstarts[problemIndex] + headstartExt);
				else
					engine->loadProblem(basePath + problems[problemIndex] + extension);
			}
			else if (event.key.keysym.sym == SDLK_n) {
				if (problemIndex + 1 < problems.size()) {
					problemIndex++;

					if (useHeadstart && headstarts.size() > problemIndex)
						engine->loadProblem(basePath + problems[problemIndex] + extension,
							basePath + headstarts[problemIndex] + headstartExt);
					else
						engine->loadProblem(basePath + problems[problemIndex] + extension);
				}
			}
		default:
			break;
		}

		chrono::duration<double, std::milli> elapsedTime = chrono::high_resolution_clock::now() - lastUpdate;
		if (runstill && elapsedTime.count() > waitMs) {
			engine->step();

			lastUpdate = chrono::high_resolution_clock::now();
		}

		if (!handled) {
			engine->handleEvent(event);
		}
	}

	SDL_DestroyWindow(window);
	SDL_DestroyRenderer(renderer);
	SDL_Quit();
	std::cout << "System cleaned." << std::endl;

	return 0;
}
