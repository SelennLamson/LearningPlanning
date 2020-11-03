/**
 * @author Thomas Lamson
 * Contact: thomas.lamson@cea.fr
 */

#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>

#include "Logic/Domain.h"

class RandomStateGenerator {
public:
	RandomStateGenerator() {}

	void init(shared_ptr<Domain> inDomain, shared_ptr<Problem> inProblem, string domainName);
	void setSeed(unsigned int seed);

	State generateState();
	State parseState(vector<string> strState);

	vector<string> generateBlocksWorldState();
	vector<string> generateColorBlocksWorldState();
	vector<string> generateLogisticsState();
	vector<string> generateComplexWorldState();
	vector<string> generateSokobanState();

private:
	shared_ptr<Domain> domain;
	vector<Term> instances;
	string domainName;

	mt19937 rng;
};
