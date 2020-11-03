#include "LearningAgent.h"

#include  <random>
#include  <iterator>

template<typename Iter, typename RandomGenerator>
Iter select_randomly(Iter start, Iter end, RandomGenerator& g) {
	std::uniform_int_distribution<__int64> dis(0, std::distance(start, end) - 1);
	std::advance(start, dis(g));
	return start;
}

template<typename Iter>
Iter select_randomly(Iter start, Iter end) {
	static std::random_device rd;
	static std::mt19937 gen(rd());
	return select_randomly(start, end, gen);
}

template<typename T>
bool contains(vector<T>* vec, T elem) {
	for(auto it = vec->begin(); it != vec->end(); it++)
		if (elem == *it)
			return true;
	return false;
}

template<typename T>
void insertAsSet(vector<T>* vec, T elem) {
	if (!contains(vec, elem))
		vec->push_back(elem);
}

ActionRule::ActionRule(vector<Literal> inPreconditions, Literal inActionLiteral, vector<Literal> inAdd, vector<Literal> inDel, vector<ActionRule*> inParents)
	: preconditions(inPreconditions), actionLiteral(inActionLiteral), add(inAdd), del(inDel), parents(inParents) {
	foreach(it, preconditions)
		foreach(pit, (*it).parameters)
			insertAsSet(&parameters, *pit);
	foreach(it, add)
		foreach(pit, (*it).parameters)
			insertAsSet(&parameters, *pit);
	foreach(it, del)
		foreach(pit, (*it).parameters)
			insertAsSet(&parameters, *pit);
}

vector<Substitution> ActionRule::prematch(State state, InstantiatedAction instAct, vector<Instance> instances) {
	if (actionLiteral.pred != instAct.action.actionLiteral.pred) return vector<Substitution>();

	// Generate all possible Theta substitutions starting from a Sigma substitution
	vector<Substitution> subs = instAct.substitution.generate(instances, parameters);
	vector<Substitution> validatedSubstitutions;

	foreach(subit, subs) {
		bool covers = true;

		foreach(precondit, preconditions) {
			Literal precond = (*subit).apply(*precondit);
			if (!state.contains(precond)) {
				covers = false;
				break;
			}
		}

		if (covers)
			validatedSubstitutions.push_back(*subit);
	}

	return validatedSubstitutions;
}

vector<Substitution> ActionRule::postmatch(State state, InstantiatedAction instAct, vector<Instance> instances) {
	if (actionLiteral.pred != instAct.action.actionLiteral.pred) return vector<Substitution>();

	vector<Substitution> inverseSubs = Substitution(true).generate(instances, parameters);



	vector<Substitution> substitutions;
	vector<Substitution> validatedSubstitutions;
	substitutions.push_back(instAct.substitution);

	for (auto paramit = parameters.begin(); paramit != parameters.end(); paramit++) {
		vector<Substitution> prevSubs = substitutions;
		substitutions = vector<Substitution>();

		for (auto subit = prevSubs.begin(); subit != prevSubs.end(); subit++) {
			Substitution sub = *subit;

			for (auto instit = instances.begin(); instit != instances.end(); instit++) {
				Substitution newSub = Substitution(sub);
				//newSub.mapping[*paramit] = *instit;
				substitutions.push_back(newSub);
			}
		}
	}

	for (auto subit = substitutions.begin(); subit != substitutions.end(); subit++) {
		bool covers = true;

		for (auto precondit = preconditions.begin(); precondit != preconditions.end(); precondit++) {
			Literal precond = (*subit).apply(*precondit);
			if (!state.contains(precond)) {
				covers = false;
				break;
			}
		}

		if (covers)
			validatedSubstitutions.push_back(*subit);
	}

	return validatedSubstitutions;
}

vector<State> ActionRule::apply(State state, vector<Substitution> substitutions) {
	vector<State> newStates;

	foreach(subit, substitutions) {
		State newState = State(state);

		foreach(it, add)
			newState.facts.insert((*subit).apply(*it));

		foreach(it, del)
			newState.facts.erase((*subit).apply(*it));

		newStates.push_back(newState);
	}
	
	return newStates;
}

LearningAgent::LearningAgent(Domain* inDomain, vector<Instance> inInstances, Goal inGoal, vector<Trace>* inTrace) : Agent(inDomain, inInstances, inGoal, inTrace) {
	actionsToPerform = 10;
}

InstantiatedAction LearningAgent::getNextAction(State state) {
	// Update the knowledge based on the last trace
	if (trace->size() > 0) {
		Trace lastTrace = trace->back();
		updateKnowledge(lastTrace);
	}

	// Generate a random action with no knowledge of domain, except actions' name and arity, and available instances.
	if (actionsToPerform > 0) {
		actionsToPerform--;
		return *select_randomly(allSubstitutions.begin(), allSubstitutions.end());
	}
	else {
		return InstantiatedAction();
	}
}

void LearningAgent::updateKnowledge(Trace trace) {
	State state = trace.state;
	State newState = trace.newState;
	InstantiatedAction instAct = trace.instAct;
	
	vector<Literal> addedFacts, removedFacts;
	state.difference(newState, &addedFacts, &removedFacts);

	// Checking coverage and consistency
	vector<ActionRule*> coverage;
	vector<ActionRule*> contradiction;
	foreach(it, rules) {

		// Producing all possible substitutions that can cover example
		vector<Substitution> coveringSubs = (*it)->prematch(state, instAct, instances);
		if (coveringSubs.size() > 0) {

			// At least one substitution covers example
			coverage.push_back(*it);

			// Generating the states after application of the different substitutions
			vector<State> newStates = (*it)->apply(state, coveringSubs);
			foreach(stateit, newStates) {

				// Computing the differences between obtained states and real state
				vector<Literal> added, removed;
				newState.difference(*stateit, &added, &removed);
				if (added.size() > 0 || removed.size() > 0) {

					// A difference occured, hence there is a contradiction
					contradiction.push_back(*it);
					break;
				}
			}
		}
	}
	
	if (coverage.size() == 0) {
		// GENERALIZE
	}
	else if (contradiction.size() > 0) {
		// SPECIALIZE
	}
}
