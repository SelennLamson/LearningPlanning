/**
 * @author Thomas Lamson
 * Contact: thomas.lamson@cea.fr
 */

#include "Logic/RandomStateGenerator.h"

#include <chrono>

int randInt(mt19937& rng, int min, int max) {
	return uniform_int_distribution<>(min, max)(rng);
}

int randInt(mt19937& rng, int max) {
	return randInt(rng, 0, max);
}

bool randBool(mt19937& rng, float prob) {
	return static_cast<float>(randInt(rng, 0, INT_MAX)) / static_cast<float>(INT_MAX) < prob;
}

bool randBool(mt19937& rng) {
	return randBool(rng, 0.5f);
}

void RandomStateGenerator::init(shared_ptr<Domain> inDomain, shared_ptr<Problem> inProblem, string inDomainName) {
	domain = inDomain;
	instances.clear();
	foreach(inst, inProblem->instances)
		instances.push_back(*inst);
	domainName = inDomainName;

	rng = mt19937{ unsigned(chrono::high_resolution_clock::now().time_since_epoch().count()) };
}

void RandomStateGenerator::setSeed(unsigned int seed) {
	rng = mt19937{ seed };
}

State RandomStateGenerator::generateState() {
	vector<string> strState;
	if (domainName == "logistics")				strState = generateLogisticsState();
	if (domainName == "logistics_onebox")		strState = generateLogisticsState();
	if (domainName == "blocksworld")			strState = generateBlocksWorldState();
	if (domainName == "colorblocksworld")		strState = generateColorBlocksWorldState();
	if (domainName == "complex")				strState = generateComplexWorldState();
	if (domainName == "complex_lessvars")		strState = generateComplexWorldState();
	if (domainName == "sokoban")				strState = generateSokobanState();

	return parseState(strState);
}

State RandomStateGenerator::parseState(vector<string> strState) {
	State s;
	foreach(str, strState)
		s.addFact(domain->parseLiteral(*str, instances, false));
	return s;
}

vector<string> RandomStateGenerator::generateBlocksWorldState() {
	vector<string> facts;

	if (instances.size() == 0) return facts;

	// Randomly select blocks to put on the floor
	set<Term> onFloor;
	set<Term> toPlace;
	set<Term> clear;
	foreach(block, instances)
		if (randBool(rng)) {
			onFloor.insert(*block);
			clear.insert(*block);
			facts.push_back("on-table(" + block->name + ")");
		}
		else
			toPlace.insert(*block);

	// At least one block must be on the floor, so add a random one if none of them are
	if (onFloor.size() == 0) {
		Term block = *select_randomly(instances.begin(), instances.end(), rng);
		onFloor.insert(block);
		clear.insert(block);
		toPlace.erase(block);

		facts.push_back("on-table(" + block.name + ")");
	}

	// Do we hold a block?
	if (onFloor.size() != instances.size() && randBool(rng, 0.1f)) {
		Term block = *select_randomly(toPlace.begin(), toPlace.end(), rng);
		toPlace.erase(block);

		facts.push_back("holding(" + block.name + ")");
	}
	else {
		facts.push_back("arm-empty()");
	}

	// Place each block one by one so that they end up on another placed block
	while (toPlace.size() > 0) {
		Term block = *select_randomly(toPlace.begin(), toPlace.end(), rng);
		toPlace.erase(block);

		Term onBlock = *select_randomly(clear.begin(), clear.end(), rng);
		clear.erase(onBlock);

		facts.push_back("on(" + block.name + ", " + onBlock.name + ")");

		clear.insert(block);
	}

	// All blocks with no block over them are now clear
	foreach(block, clear)
		facts.push_back("clear(" + block->name + ")");

	return facts;
}

vector<string> RandomStateGenerator::generateColorBlocksWorldState() {
	vector<string> facts;

	if (instances.size() == 0) return facts;

	vector<Term> blocks;
	foreach(inst, instances)
		if (TermType::typeSubsumes(domain->getTypeByName("object"), inst->type))
			blocks.push_back(*inst);

	// Randomly select blocks to put on the floor
	set<Term> onFloor;
	foreach(block, blocks)
		if (randBool(rng))
			onFloor.insert(*block);

	// At least one block must be on the floor, so add a random one if none of them are
	if (onFloor.size() == 0)
		onFloor.insert(*select_randomly(blocks.begin(), blocks.end(), rng));

	set<Term> toPlace;
	set<Term> clear;
	foreach(block, blocks)
		if (!in(onFloor, *block)) {
			toPlace.insert(*block);
		}
		else {
			clear.insert(*block);

			facts.push_back("on(" + block->name + ", floor)");
		}

	// Place each block one by one so that they end up on another placed block
	while (toPlace.size() > 0) {
		Term block = *select_randomly(toPlace.begin(), toPlace.end(), rng);
		toPlace.erase(block);

		Term onBlock = *select_randomly(clear.begin(), clear.end(), rng);
		clear.erase(onBlock);

		facts.push_back("on(" + block.name + ", " + onBlock.name + ")");

		clear.insert(block);
	}

	// All blocks with no block over them are now clear
	foreach(block, clear)
		facts.push_back("clear(" + block->name + ")");

	// Select a random color for each block: this might result in unreachable states, but they are never blocking for the future
	int blacks = 0, whites = 0;
	int halfCeil = (int)blocks.size() / 2 + ((int)blocks.size() % 2 == 0 ? 0 : 1);
	foreach(block, instances)
		if ((randBool(rng) && whites < halfCeil) || blacks >= halfCeil) {
			facts.push_back("w(" + block->name + ")");
			whites++;
		}
		else {
			facts.push_back("b(" + block->name + ")");
			blacks++;
		}

	return facts;
}

vector<string> RandomStateGenerator::generateLogisticsState() {
	vector<string> facts;

	vector<Term> boxes = filterByType(instances, domain->getTypeByName("box"));
	vector<Term> trucks = filterByType(instances, domain->getTypeByName("truck"));
	vector<Term> cities = filterByType(instances, domain->getTypeByName("city"));

	// Randomising trucks position
	foreach(truck, trucks) {
		Term city = *select_randomly(cities.begin(), cities.end(), rng);
		facts.push_back("truckin(" + truck->name + ", " + city.name + ")");
	}

	//Randomising boxes position (either on truck or in a city)
	foreach(box, boxes) {
		if (randBool(rng)) {
			Term city = *select_randomly(cities.begin(), cities.end(), rng);
			facts.push_back("boxin(" + box->name + ", " + city.name + ")");
		}
		else {
			Term truck = *select_randomly(trucks.begin(), trucks.end(), rng);
			facts.push_back("boxin(" + box->name + ", " + truck.name + ")");
		}
	}

	return facts;
}

vector<string> RandomStateGenerator::generateComplexWorldState() {
	vector<string> facts;

	vector<Term> objects = filterByType(instances, domain->getTypeByName("object"));
	vector<Term> locations = filterByType(instances, domain->getTypeByName("location"));
	vector<Term> colors = filterByType(instances, domain->getTypeByName("color"));

	map<Term, set<Term>> rooms;
	foreach(loc, locations)
		rooms[*loc] = {};

	// Each object can: be pushable, be grabbable, have a color, be a light
	foreach(obj, objects) {
		if (randBool(rng, 0.8f))
			facts.push_back("pushable(" + obj->name + ")");

		if (randBool(rng, 0.7f))
			facts.push_back("grabbable(" + obj->name + ")");

		if (randBool(rng, 0.5f)) {
			Term col = *select_randomly(colors.begin(), colors.end(), rng);
			facts.push_back("has-color(" + obj->name + ", " + col.name + ")");
		}

		if (randBool(rng, 0.3f))
			facts.push_back("light(" + obj->name + ")");

		// Each object is in a room
		Term room = *select_randomly(locations.begin(), locations.end(), rng);
		rooms[room].insert(*obj);
		facts.push_back("at(" + obj->name + ", " + room.name + ")");
	}

	// One paint-bucket per color
	foreach(col, colors) {
		Term obj = *select_randomly(objects.begin(), objects.end(), rng);

		facts.push_back("is-paint(" + obj.name + ", " + col->name + ")");
	}

	// Bigger-relation complete order on objects
	set<Term> bigger = toSet(objects);
	vector<Term> orderedObjects;
	while (bigger.size() > 0) {
		Term obj = *select_randomly(bigger.begin(), bigger.end(), rng);
		bigger.erase(obj);
		orderedObjects.insert(orderedObjects.begin(), obj);

		if (in(facts, "grabbable(" + obj.name + ")"))
			foreach(biggerObj, bigger)
				facts.push_back("bigger(" + biggerObj->name + ", " + obj.name + ")");
	}

	// Robot's location
	Term robotLoc = *select_randomly(locations.begin(), locations.end(), rng);
	facts.push_back("robot-at(" + robotLoc.name + ")");

	// Treating objects in each room
	foreach(loc, locations) {
		set<Term> objectsInRoom = rooms[*loc];

		// Is room powered?
		bool powered = randBool(rng);
		if (powered)
			facts.push_back("powered(" + loc->name + ")");
		else
			facts.push_back("not-powered(" + loc->name + ")");

		// Is there any pluggable object in room?
		vector<Term> lightObjects;
		foreach(obj, objectsInRoom)
			if (in(facts, "light(" + obj->name + ")"))
				lightObjects.push_back(*obj);

		// If so, is any of them plugged?
		if (lightObjects.size() > 0 && randBool(rng)) {
			Term light = *select_randomly(lightObjects.begin(), lightObjects.end(), rng);

			facts.push_back("plugged(" + light.name + ", " + loc->name + ")");
			if (powered)
				facts.push_back("lit(" + light.name + ")");
		}
		else
			facts.push_back("clear(" + loc->name + ")");

		// If robot is here, does it have an object in hand?
		if (robotLoc == *loc) {
			vector<Term> grabObjects;
			foreach(obj, objectsInRoom)
				if (in(facts, "grabbable(" + obj->name + ")"))
					grabObjects.push_back(*obj);
			if (grabObjects.size() > 0 && randBool(rng)) {
				Term obj = *select_randomly(grabObjects.begin(), grabObjects.end(), rng);
				objectsInRoom.erase(obj);
				facts.push_back("holding(" + obj.name + ")");
			}
			else
				facts.push_back("arm-empty()");
		}

		// Remaining objects need to be placed in room
		set<Term> clear;
		foreach(obj, orderedObjects) {
			if (!in(objectsInRoom, *obj)) continue;
			
			// Do we place the object onto another?
			if (clear.size() > 0 && randBool(rng) && in(facts, "grabbable(" + obj->name + ")")) {
				Term onObj = *select_randomly(clear.begin(), clear.end(), rng);
				clear.erase(onObj);

				facts.push_back("on(" + obj->name + ", " + onObj.name + ")");

				clear.insert(*obj);
			}
			else {
				facts.push_back("on-floor(" + obj->name + ")");
				clear.insert(*obj);
			}
		}

		// Finally, objects are clear if no objects sit on top of them
		foreach(obj, clear)
			facts.push_back("clear(" + obj->name + ")");
	}

	return facts;
}

vector<string> RandomStateGenerator::generateSokobanState() {
	vector<string> facts;

	// TODO

	return facts;
}
