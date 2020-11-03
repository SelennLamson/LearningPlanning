/**
 * @author Thomas Lamson
 * Contact: thomas.lamson@cea.fr
 *
 * This IRALe Active Explorer implements Rodrigues (2012) paper which uses LGG to predict
 * whether an action is likely to lead to a generalization of the current model.
 * It combines this active exploration with a proportion of random actions.
 */

#include "Agents/LearningAgent/IRALeExplorer.h"

IRALeExplorer::IRALeExplorer(bool inVerbose) : ExplorerAgentBase(inVerbose) {
	explorerConfig = config->getSubconfig("irale_explorer");
	epsilon = explorerConfig->getFloat("epsilon");
}

void IRALeExplorer::init(shared_ptr<Domain> inDomain, vector<Term> inInstances, Goal inGoal, shared_ptr<vector<Trace>> inTrace) {
	Agent::init(inDomain, inInstances, inGoal, inTrace);
	prepareActionSubstitutions();
	prevState = State();
}

void IRALeExplorer::setRules(vector<shared_ptr<ActionRule>> newRules) {
	rules = newRules;
}

void IRALeExplorer::setActionLiterals(set<Literal> baseActionLiterals) {
	actionLiterals.clear();
	actionPredicates.clear();
	foreach(lit, baseActionLiterals) {
		actionPredicates.insert(lit->pred);
		vector<Substitution> subs = Substitution().expandUncovered(lit->parameters, instances + domain->getConstants(), true);
		foreach(sub, subs)
			actionLiterals.insert(sub->apply(*lit));
	}
}

Literal IRALeExplorer::getNextAction(State state) {
	iteration++;

	vector<Term> allInsts = instances + domain->getConstants();

	if (static_cast<float>(rand()) / static_cast<float>(RAND_MAX) > epsilon) {
		return *select_randomly(actionLiterals.begin(), actionLiterals.end());
	}
	else if (state != prevState) {
		debug << endl << "Active Learning: " << endl;

		interestingActions.clear();
		prevState = state;

		foreach(rit, rules) {
			shared_ptr<ActionRule> rule = *rit;
			
			debug << "Rule: " << endl << rule->toString() << endl;

			if (Substitution().oiSubsume(rule->preconditions, state.facts).size() == 0) {
				debug << "RULE DOESN'T APPLY, performing anticipated generalization" << endl;

				// Looking for subr and subx such that del新ubr-1新ubx in S, add新ubr-1新ubx not in S and (del+add)新ubr-1新ubx is grounded
				
				Substitution subr;
				set<Term> genVars;
				set<Literal> genDels;
				set<Literal> genAdds;
				Literal genAct;

				// Generalizing del effects
				foreach(deleff, rule->del) {
					foreach(param, deleff->parameters) {
						if (!param->isVariable && !subr.getInverse(*param).there) {
							Term var = rule->makeNewVar(genVars, *param);
							subr.set(var, *param);
						}
					}

					genDels.insert(subr.inverse().apply(*deleff));
				}

				debug << "Generalized del effects: " << join(", ", genDels) << endl;

				// Generalizing add effects
				foreach(addeff, rule->add) {
					foreach(param, addeff->parameters) {
						if (!param->isVariable && !subr.getInverse(*param).there) {
							Term var = rule->makeNewVar(genVars, *param);
							subr.set(var, *param);
						}
					}

					genAdds.insert(subr.inverse().apply(*addeff));
				}

				debug << "Generalized add effects: " << join(", ", genAdds) << endl;

				// Generalizing action literal
				foreach(param, rule->actionLiteral.parameters) {
					if (!param->isVariable && !subr.getInverse(*param).there) {
						Term var = rule->makeNewVar(genVars, *param);
						subr.set(var, *param);
					}
				}
				genAct = subr.inverse().apply(rule->actionLiteral);

				set<Term> uncovered;
				foreach(a, genAdds) foreach(p, a->parameters)
					if (p->isVariable)
						uncovered.insert(*p);
				foreach(d, genDels) foreach(p, d->parameters)
					if (p->isVariable)
						uncovered.insert(*p);
				foreach(p, genAct.parameters)
					if (p->isVariable)
						uncovered.insert(*p);

				debug << "Generalized action literal: " << genAct << endl;

				// At this stage, we have generalized the rule completely, replacing every constant by a variable
				// in add or del effects and in action literal. We didn't touched the preconds yet.

				// We begin with all the possible subx for which genDels新ubx is in state
				set<Substitution> subxs = Substitution().oiSubsume(genDels, state.facts);

				bool foundAddInState;
				foreach(subit1, subxs) {
					Substitution subx = *subit1;

					debug << endl << "-- Testing incomplete substitution: " << subx << endl;

					// Even though genAdds could still contain variables (only genDels are fully instanciated by subx)
					// we can check a first time if any of its element is grounded and appears in state. If so, pass.
					foundAddInState = false;
					foreach(a, genAdds)
						if (state.contains(subx.apply(*a))) {
							foundAddInState = true;
							break;
						}
					if (foundAddInState) {
						debug << "   Add effect found in state, skipping." << endl;
						continue;
					}

					// Instantiate all remaining variables in genAdds and genAct with all possible combination of instances
					// that are not covered by subx yet.
					vector<Substitution> subxxs = subx.expandUncovered(toVec(uncovered), allInsts, true);

					foreach(subit2, subxxs) {
						Substitution subxx = *subit2;

						if (subx == subxx) debug << "   Substitution was complete, continuing with it." << endl;
						else debug << "   Trying complete substitution: " << subxx << endl;

						// Now that every add effect from genAdds can be instantiated through subxx,
						// check that they do not appear in state. If they do, pass.
						foundAddInState = false;
						foreach(a, genAdds)
							if (state.contains(subxx.apply(*a))) {
								foundAddInState = true;
								break;
							}
						if (foundAddInState) {
							debug << "   Add effect found in state, skipping." << endl;
							continue;
						}

						// At this stage:
						// genDels新ubr = r->del
						// genAdds新ubr = r->add
						// genAct新ubr = r->actionLiteral
						// genDels.subxx in S
						// For any a in genAdds新ubxx, a not in S
						// genAct新ubxx is instantiated

						// We apply the rule as if it would work with subxx substitution
						State newState = state;
						newState.addFacts(subxx.apply(genAdds));
						newState.removeFacts(subxx.apply(genDels));
						Literal actLit = subxx.apply(genAct);
						shared_ptr<ActionRule> example = make_shared<ActionRule>(Trace(state, actLit, true, newState), true);

						assert(actLit.grounded());
						debug << "   APPLIED ANTICIPATED EXAMPLE:" << endl << example->toString() << endl;

						// We generalize the rule as if it just worked in a state with incomplete preconditions (no prematching)
						Substitution subrr = subr;
						set<Term> genVarsTmp = genVars;
						set<Literal> genPreconds = rule->anyGeneralization(example, subrr, subxx, genVarsTmp);
						
						// The size of genPreconds is the size of an eventual new rule after generalization
						interestingActions.push_back({ actLit, genPreconds.size() });

						debug << "   ADDING ACTION: " << actLit << ", generalized preconds: " << join(", ", genPreconds) << endl;
					}
				}
			}
			else {
				debug << "RULE APPLIES, skipping" << endl;
			}
		}
	}

	if (interestingActions.size() > 0) {
		size_t maxSize = 0;
		set<Literal> maxLits;

		foreach(pair, interestingActions) {
			if (pair->second > maxSize) {
				maxLits = { pair->first };
				maxSize = pair->second;
			}
			else if (pair->second == maxSize)
				maxLits.insert(pair->first);
		}

		Literal selected = *select_randomly(maxLits.begin(), maxLits.end());

		vector<pair<Literal, size_t>> tmpActions = interestingActions;
		interestingActions.clear();
		foreach(pair, tmpActions)
			if (pair->first != selected)
				interestingActions.push_back(*pair);

		debug << "Selected: " << selected << endl;
		return selected;
	}

	return *select_randomly(actionLiterals.begin(), actionLiterals.end());
}

void IRALeExplorer::updateProblem(vector<Term> inInstances, Goal inGoal, vector<Literal> inHeadstart) {
	Agent::updateProblem(inInstances, inGoal, inHeadstart);
	prepareActionSubstitutions();
}

void IRALeExplorer::prepareActionSubstitutions() {
}
