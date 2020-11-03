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

class DomainTester {
public:
	DomainTester() {}

	void init(shared_ptr<Domain> inDomain, shared_ptr<Problem> inProblem, string datasetPath, int inTestProblems);
	void testDomain(shared_ptr<Domain> testedDomain, /*r*/ float& variationalDistance, /*r*/ float& planningDistance);

private:
	shared_ptr<Domain> groundTruthDomain;
	vector<Term> instances;
	map<Predicate, vector<Trace>> samples;
	vector<Problem> problems;

	shared_ptr<vector<Trace>> trace;
	int testProblems;

	bool initialized = false;
};
