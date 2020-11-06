/**
 * @author Thomas Lamson
 * Contact: thomas.lamson@cea.fr
 *
 * These ActionRule implements Extended Deterministic STRIPS  rules and operators as described
 * in the paper on which the LearningAgent is based.
 *
 * Based on paper: "Incremental Learning of Relational Action Rules", Christophe Rodrigues, Pierre Gérard,
 *                 Céline Rouveirol, Henry Soldano, https://doi.org/10.1109/ICMLA.2010.73
 *
 * Openly available here: https://lipn.univ-paris13.fr/~rodrigues/icmla2010PP.pdf
 */

#include "Agents/LearningAgent/ActionRule.h"
#include "Agents/LearningAgent/LearningAgent.h"

#define PRECISION 0.001f
#define SUBS_FOR_FULFILMENT 20
#define SUBS_FOR_CORROBORATION 20

bool varOccurs(Term var, set<Literal> literals) {
	foreach(litit, literals)
		if (in(litit->parameters, var))
			return true;
	return false;
}

bool linked(map<Term, set<Term>> links, Term from, set<Term> to) {
	if (in(to, from)) return true;

	set<Term> toVisit = { from };
	set<Term> visited;

	while (toVisit.size() > 0) {
		Term current = *toVisit.begin();
		toVisit.erase(current);
		visited.insert(current);

		foreach(linkedit, links[current]) {
			if (in(visited, *linkedit)) continue;
			if (in(to, *linkedit)) return true;
			toVisit.insert(*linkedit);
		}
	}

	return false;
}

ActionRule::ActionRule(set<Literal> inPreconditions, Literal inActionLiteral, set<Literal> inAdd, set<Literal> inDel,
					   set<shared_ptr<ActionRule>> inParents, float startPuIn, bool filter)
	: actionLiteral(inActionLiteral), add(inAdd), del(inDel), parents(inParents), startPu(startPuIn) {

	if (filter) {
		map<Term, set<Term>> links;
		set<Term> vars;
		foreach(precond, inPreconditions)
			foreach(pit1, precond->parameters) {
			if (!in(links, *pit1)) {
				links[*pit1] = {};
			}

			foreach(pit2, precond->parameters) {
				if (*pit1 != *pit2) {
					links[*pit1].insert(*pit2);
				}
			}
		}
		foreach(eff, add)
			vars = vars + eff->parameters;
		foreach(eff, del)
			vars = vars + eff->parameters;
		vars = vars + actionLiteral.parameters;

		foreach(precond, inPreconditions) {
			bool allLinked = true;
			foreach(param, precond->parameters)
				if (!linked(links, *param, vars)) {
					allLinked = false;
					break;
				}
			if (allLinked)
				preconditions.insert(*precond);
		}
	}
	else {
		preconditions = inPreconditions;
	}

	//if (filter)
	//	assert(wellFormed());

	extractParameters();

	set<Term> constants;
	foreach(precond, preconditions)
		foreach(param, precond->parameters)
		if (!param->isVariable)
			constants.insert(*param);
	float components = (float)preconditions.size() + (float)constants.size() - (float)del.size();

	foreach(precond, preconditions) {

		// Del effects must be in preconditions, and we are sure of them, so they are 100% necessary
		if (in(del, -*precond))
			precondsNecessities[*precond] = 1.0f;
		else
			precondsNecessities[*precond] = 1.0f - powf(startPu, 1.0f / components);
		
		foreach(param, precond->parameters)
			if (!param->isVariable)
				constsNecessities[*param] = 1.0f - powf(startPu, 1.0f / components);
	}
}

ActionRule::ActionRule(Trace trace, float startPuIn, bool filter) : actionLiteral(trace.instAct), startPu(startPuIn) {
	trace.state.difference(trace.newState, &add, &del);

	set<Literal> tempPreconds;
	foreach(it, trace.state.facts)
		tempPreconds.insert(*it);

	if (filter) {
		map<Term, set<Term>> links;
		set<Term> vars;

		foreach(precond, tempPreconds)
			foreach(pit1, precond->parameters) {
			if (!in(links, *pit1)) {
				links[*pit1] = {};
			}

			foreach(pit2, precond->parameters) {
				if (*pit1 != *pit2) {
					links[*pit1].insert(*pit2);
				}
			}
		}
		foreach(eff, add)
			vars = vars + eff->parameters;
		foreach(eff, del)
			vars = vars + eff->parameters;
		foreach(param, actionLiteral.parameters)
			vars.insert(*param);

		foreach(precond, tempPreconds) {
			bool allLinked = true;
			foreach(param, precond->parameters)
				if (!linked(links, *param, vars)) {
					allLinked = false;
					break;
				}
			if (allLinked)
				preconditions.insert(*precond);
		}
	}
	else {
		preconditions = tempPreconds;
	}

	extractParameters();

	if (trace.authorized && filter) {
		if (!wellFormed()) {
			cout << "NOT WELL FORMED --> " << toString() << endl;
		}

		assert(wellFormed());
	}

	set<Term> constants;
	foreach(precond, preconditions)
		foreach(param, precond->parameters)
			if (!param->isVariable)
				constants.insert(*param);
	float components = (float)preconditions.size() + (float)constants.size() - (float)del.size();

	foreach(precond, preconditions) {

		// Del effects must be in preconditions, and we are sure of them, so they are 100% necessary
		if (in(del, -*precond))
			precondsNecessities[*precond] = 1.0f;
		else
			precondsNecessities[*precond] = 1.0f - powf(startPu, 1.0f / components);

		foreach(param, precond->parameters)
			if (!param->isVariable)
				constsNecessities[*param] = 1.0f - powf(startPu, 1.0f / components);
	}
}

void unifyWithState(set<Literal> toUnify, State s, /*r*/ set<Substitution>& subs) {
	bool couldSet;

	foreach(f, toUnify) {
		set<Substitution> nextSubs;

		foreach(sub, subs) {
			Literal subbed = sub->apply(*f);
			set<Literal> query = s.query(subbed);

			if (query.size() == 0) {
				continue;
			}

			if (subbed.grounded()) {
				nextSubs.insert(*sub);
				continue;
			}

			foreach(q, query) {
				Substitution extendedSub = Substitution(*sub);
				couldSet = extendedSub.setSafeMultiple(subbed.parameters, q->parameters);
				if (couldSet)
					nextSubs.insert(extendedSub);
			}
		}

		if (nextSubs.size() == 0) {
			subs = {};
			return;
		}
		subs = nextSubs;
	}
}

set<Substitution> ActionRule::prematchingSubs(shared_ptr<ActionRule> x, Substitution sub) {
	if (!Literal::compatible(actionLiteral, x->actionLiteral)) return {};

	set<Term> constants;
	foreach(p, actionLiteral.parameters)
		if (!p->isVariable)
			constants.insert(*p);
	foreach(precond, preconditions)
		foreach(p, precond->parameters)
			if (!p->isVariable)
				constants.insert(*p);
	foreach(c, constants)
		if (!sub.get(*c).there)
			sub.set(*c, *c);

	bool couldSet = sub.setSafeMultiple(actionLiteral.parameters, x->actionLiteral.parameters);
	if (!couldSet) return {};

	set<Substitution> subs = { sub };
	unifyWithState(preconditions, State(x->preconditions), subs);

	set<Substitution> cleanSubs;
	foreach(sub, subs) {
		Substitution clean = *sub;
		clean.cleanConstants();
		cleanSubs.insert(clean);
	}

	return cleanSubs;
}

bool ActionRule::prematches(shared_ptr<ActionRule> x, Substitution sub) {
	return prematchingSubs(x, sub).size() == 1;
}

set<Substitution> ActionRule::postmatchingSubs(shared_ptr<ActionRule> x, Substitution sub) {
	if (!Literal::compatible(actionLiteral, x->actionLiteral)) return {};

	State s = State(x->add + x->del);
	if (add.size() != x->add.size() || del.size() != x->del.size()) return {};

	set<Term> constants;
	foreach(p, actionLiteral.parameters)
		if (!p->isVariable)
			constants.insert(*p);
	foreach(a, add)
		foreach(p, a->parameters)
			if (!p->isVariable)
				constants.insert(*p);
	foreach(d, del)
		foreach(p, d->parameters)
			if (!p->isVariable)
				constants.insert(*p);
	foreach(c, constants)
		if (!sub.get(*c).there && !sub.getInverse(*c).there)
			sub.set(*c, *c);

	bool couldSet = sub.setSafeMultiple(actionLiteral.parameters, x->actionLiteral.parameters);
	if (!couldSet) return {};

	set<Substitution> subs = { sub };

	unifyWithState(add + del, s, subs);

	set<Substitution> cleanSubs;
	foreach(sub, subs) {
		Substitution clean = *sub;
		clean.cleanConstants();
		cleanSubs.insert(clean);
	}

	return cleanSubs;
}

bool ActionRule::postmatches(shared_ptr<ActionRule> x, Substitution sub) {
	return postmatchingSubs(x, sub).size() > 0;
}

set<Substitution> ActionRule::coveringSubs(shared_ptr<ActionRule> x, Substitution sub) {
	set<Substitution> prematchSubs = prematchingSubs(x, sub);
	set<Substitution> covSubs;
	foreach(pre, prematchSubs)
		covSubs = covSubs + postmatchingSubs(x, *pre);
	return covSubs;
}

bool ActionRule::covers(shared_ptr<ActionRule> x, Substitution sub) {
	return coveringSubs(x, sub).size() > 0;
}

Opt<Literal> ActionRule::generalizeLiteralsOI(Literal l1, Literal l2, /*r*/ set<Term>& genVars, /*r*/ Substitution& theta1, /*r*/ Substitution& theta2) {
	if (!Literal::compatible(l1, l2)) return Opt<Literal>();

	Substitution tmpTheta1 = Substitution(theta1);
	Substitution tmpTheta2 = Substitution(theta2);
	set<Term> tmpGenVars = genVars;
	vector<Term> params;
	bool stop = false;
	foreachindex(i, l1.parameters) {
		Term term1 = l1.parameters[i];
		Term term2 = l2.parameters[i];
		Term genTerm1 = term1, genTerm2 = term2;

		Opt<Term> invTerm1 = tmpTheta1.getInverse(term1);
		if (invTerm1.there)
			genTerm1 = invTerm1.obj;

		Opt<Term> invTerm2 = tmpTheta2.getInverse(term2);
		if (invTerm2.there)
			genTerm2 = invTerm2.obj;

		if (genTerm1 == genTerm2) {
			if (!genTerm1.isVariable) {
				Term varTerm = makeNewVar(tmpGenVars, term1);
				params.push_back(varTerm);
				tmpTheta1.set(varTerm, term1);
				tmpTheta2.set(varTerm, term2);
			}
			else params.push_back(genTerm1);
		}
		else {
			if ((invTerm1.there && invTerm1.obj == term1) || (invTerm2.there && invTerm2.obj == term2)) stop = true;
			else if (genTerm1.isVariable && genTerm2.isVariable) stop = true;
			else if (genTerm1.isVariable) {
				if (!tmpTheta2.get(genTerm1).there) {
					params.push_back(genTerm1);
					if (!tmpTheta1.get(genTerm1).there) tmpTheta1.set(genTerm1, genTerm1);
					tmpTheta2.set(genTerm1, term2);
				}
				else stop = true;
			}
			else if (genTerm2.isVariable) {
				if (!tmpTheta1.get(genTerm2).there) {
					params.push_back(genTerm2);
					if (!tmpTheta2.get(genTerm2).there) tmpTheta2.set(genTerm2, genTerm2);
					tmpTheta1.set(genTerm2, term1);
				}
				else stop = true;
			}
			else {
				Term varTerm = makeNewVar(tmpGenVars, term1);
				params.push_back(varTerm);
				tmpTheta1.set(varTerm, term1);
				tmpTheta2.set(varTerm, term2);
			}
		}

		if (stop) break;
	}

	if (!stop) {
		theta1 = tmpTheta1;
		theta2 = tmpTheta2;
		genVars = tmpGenVars;
		return Opt<Literal>(Literal(l1.pred, { params }, l1.positive));
	}
	return Opt<Literal>();
}

bool ActionRule::selection(set<Literal>lr, set<Literal> lx, shared_ptr<ActionRule> x,
						   /*r*/ Substitution& subr, /*r*/ Substitution& subx,
						   /*r*/ set<Term>& genVars, /*r*/ set<Literal>& genLits,
						   /*r*/ Literal& chosenLr, /*r*/ Literal& chosenLx) {

	Substitution invSubR = subr.inverse();
	Literal genAct = invSubR.apply(actionLiteral);
	set<Literal> genAdd = invSubR.apply(add);
	set<Literal> genDel = invSubR.apply(del);

	Substitution tmpSubR, tmpSubX;
	set<Term> tmpGenVars;

	set<Literal> drawLr = lr;
	while (drawLr.size() > 0) {
		chosenLr = *select_randomly(drawLr.begin(), drawLr.end());
		drawLr.erase(chosenLr);

		set<Literal> drawLx;
		foreach(l, lx)
			if (Literal::compatible(chosenLr, *l))
				drawLx.insert(*l);

		while (drawLx.size() > 0) {
			chosenLx = *select_randomly(drawLx.begin(), drawLx.end());
			drawLx.erase(chosenLx);

			tmpSubR = Substitution(subr);
			tmpSubX = Substitution(subx);
			tmpGenVars = genVars;

			Opt<Literal> genLit = generalizeLiteralsOI(chosenLr, chosenLx, tmpGenVars, tmpSubR, tmpSubX);

			if (!genLit.there) continue;

			set<Literal> newGenLits = genLits + set<Literal>{ genLit.obj };
			ActionRule actRule = ActionRule(newGenLits, genAct, genAdd, genDel, parents, startPu, false);

			if (actRule.prematches(x, tmpSubX)) {
			//if (actRule.covers(x)) {
				genLits = newGenLits;
				subr = tmpSubR;
				subx = tmpSubX;
				genVars = tmpGenVars;
				return true;
			}
		}
	}
	return false;
}

set<Literal> ActionRule::anyGeneralization(set<Literal> lr, set<Literal> lx, shared_ptr<ActionRule> x,
										   /*r*/ Substitution& subr, /*r*/ Substitution& subx, /*r*/ set<Term>& genVars) {
	set<Literal> genLits;
	Literal chosenLr, chosenLx;

	//cout << "Generalizing: " << endl << join(lr) << endl << join(lx) << endl;
	//cout << "Starting with: " << subr << " - " << subx << endl;

	while (lr.size() > 0 && lx.size() > 0) {
		if (selection(lr, lx, x, subr, subx, genVars, genLits, chosenLr, chosenLx))
			lx.erase(chosenLx);
		lr.erase(chosenLr);

		//cout << "Chose: " << chosenLr << " and " << chosenLx << ". Gen: " << join(genLits) << " - " << subr << " - " << subx << endl;
	}

	return genLits;
}

set<Literal> ActionRule::anyGeneralization(shared_ptr<ActionRule> x, /*r*/ Substitution& subr, /*r*/ Substitution& subx, /*r*/ set<Term>& genVars) {
	return anyGeneralization(subr.inverse().apply(preconditions), subx.inverse().apply(x->preconditions), x, subr, subx, genVars);
}

bool ActionRule::exactGeneralizationLxChoice(Literal chosenLr, set<Literal> lr, set<Literal> lx,
	/*r*/ Substitution& subr, /*r*/ Substitution& subx, /*r*/ set<Term>& genVars, /*r*/ set<Literal>& genLits) {

	set<Literal> tmpLx = lx;
	vector<Literal> shuffledLx;
	while (tmpLx.size() > 0) {
		Literal l = *select_randomly(tmpLx.begin(), tmpLx.end());
		if (Literal::compatible(l, chosenLr))
			shuffledLx.push_back(l);
		tmpLx.erase(l);
	}

	foreach(chosenLx, shuffledLx) {
		Substitution tmpSubR = Substitution(subr);
		Substitution tmpSubX = Substitution(subx);
		set<Term> tmpGenVars = genVars;
		set<Literal> tmpLx = lx;
		tmpLx.erase(*chosenLx);

		Opt<Literal> genLit = generalizeLiteralsOI(chosenLr, *chosenLx, tmpGenVars, tmpSubR, tmpSubX);
		
		if (!genLit.there) continue;

		set<Literal> tmpGenLits = genLits + set<Literal>{ genLit.obj };

		bool success = exactGeneralizationLrChoice(lr, tmpLx, tmpSubR, tmpSubX, tmpGenVars, tmpGenLits);
		if (success) {
			genLits = tmpGenLits;
			subr = tmpSubR;
			subx = tmpSubX;
			genVars = tmpGenVars;
			return true;
		}
	}

	return false;
}

bool ActionRule::exactGeneralizationLrChoice(set<Literal> lr, set<Literal> lx,
	/*r*/ Substitution& subr, /*r*/ Substitution& subx, /*r*/ set<Term>& genVars, /*r*/ set<Literal>& genLits) {
	if (lr.size() == 0) return true;

	vector<Literal> shuffledLr;
	while (lr.size() > 0) {
		Literal l = *select_randomly(lr.begin(), lr.end());
		shuffledLr.push_back(l);
		lr.erase(l);
	}

	foreach(chosenLr, shuffledLr) {
		Substitution tmpSubR = Substitution(subr);
		Substitution tmpSubX = Substitution(subx);
		set<Term> tmpGenVars = genVars;
		set<Literal> tmpLr = toSet(shuffledLr);
		tmpLr.erase(*chosenLr);
		set<Literal> tmpGenLits = genLits;

		bool success = exactGeneralizationLxChoice(*chosenLr, tmpLr, lx, tmpSubR, tmpSubX, tmpGenVars, tmpGenLits);
		if (success) {
			genLits = tmpGenLits;
			subr = tmpSubR;
			subx = tmpSubX;
			genVars = tmpGenVars;
			return true;
		}
	}

	return false;
}

bool ActionRule::postGeneralizes(shared_ptr<ActionRule> x, /*r*/ Substitution& subr, /*r*/ Substitution& subx, /*r*/ set<Term>& genVars) {
	if (add.size() != x->add.size() || del.size() != x->del.size()) return false;

	Opt<Literal> genAct = generalizeLiteralsOI(actionLiteral, x->actionLiteral, genVars, subr, subx);

	if (!genAct.there) return false;

	set<Literal> effGen;
	bool success = exactGeneralizationLrChoice(add + del, x->add + x->del, subr, subx, genVars, effGen);

	subr.cleanConstants();

	foreach(p, actionLiteral.parameters)
		if (!subr.getInverse(*p).there)
			subr.set(*p, *p);
	foreach(eff, add)
		foreach(p, eff->parameters)
			if (!subr.getInverse(*p).there)
				subr.set(*p, *p);
	foreach(eff, del)
		foreach(p, eff->parameters)
		if (!subr.getInverse(*p).there)
			subr.set(*p, *p);

	foreach(p, x->actionLiteral.parameters)
		if (!subx.getInverse(*p).there)
			subx.set(*p, *p);
	foreach(eff, x->add)
		foreach(p, eff->parameters)
		if (!subx.getInverse(*p).there)
			subx.set(*p, *p);
	foreach(eff, x->del)
		foreach(p, eff->parameters)
		if (!subx.getInverse(*p).there)
			subx.set(*p, *p);

	return success;
}

shared_ptr<ActionRule> ActionRule::makeUseOfVariables() {
	set<Term> genVars;
	Substitution genSub;

	foreach(param, actionLiteral.parameters)
		if (!param->isVariable)
			genSub.set(*param, makeNewVar(genVars, *param));

	Literal newActionLit = genSub.apply(actionLiteral);
	set<Literal> newPreconds, newAdd, newDel;

	foreach(precond, preconditions) {
		foreach(param, precond->parameters)
			if (!param->isVariable && !genSub.get(*param).there)
				genSub.set(*param, makeNewVar(genVars, *param));
		newPreconds.insert(genSub.apply(*precond));
	}

	foreach(addeff, add) {
		foreach(param, addeff->parameters)
			if (!param->isVariable && !genSub.get(*param).there)
				genSub.set(*param, makeNewVar(genVars, *param));
		newAdd.insert(genSub.apply(*addeff));
	}

	foreach(deleff, del) {
		foreach(param, deleff->parameters)
			if (!param->isVariable && !genSub.get(*param).there)
				genSub.set(*param, makeNewVar(genVars, *param));
		newDel.insert(genSub.apply(*deleff));
	}

	set<shared_ptr<ActionRule>> newParents = { shared_from_this() };
	return make_shared<ActionRule>(newPreconds, newActionLit, newAdd, newDel, newParents, startPu);
}

void ActionRule::extractParameters() {
	set<Term> params;
	foreach(it, actionLiteral.parameters)
		params.insert(*it);
	foreach(it, preconditions)
		foreach(pit, (*it).parameters)
			params.insert(*pit);
	foreach(it, add)
		foreach(pit, (*it).parameters)
			params.insert(*pit);
	foreach(it, del)
		foreach(pit, (*it).parameters)
			params.insert(*pit);

	parameters = toVec(params);
}

int ActionRule::getFreeParameterId(set<Term> blackList) const {
	int id = 0;
	string name = "";
	bool available = false;

	while (!available) {
		id++;
		name = varName(id);
		available = true;

		foreach(param, parameters)
			if (param->name == name) {
				available = false;
				break;
			}
		if (available)
			foreach(bl, blackList)
				if (bl->name == name) {
					available = false;
					break;
				}
	}

	return id;
}

Variable ActionRule::makeNewVar(set<Term>& genVars, Term param) const {
	int id = getFreeParameterId(genVars);
	Variable var = Variable(varName(id), getMostGeneralType(param.type));
	genVars.insert(var);
	return var;
}

bool ActionRule::contradicts(shared_ptr<ActionRule> x) {
	set<Substitution> subs = prematchingSubs(x);
	if (subs.size() > 0)
		foreach(sub, subs)
			if (!postmatches(x, *sub))
				return true;
	return false;
}

void ActionRule::insertParent(shared_ptr<ActionRule> parent) {
	parents.insert(parent);
}

void ActionRule::removeParentRecursive(shared_ptr<ActionRule> parent) {
	parents.erase(parent);
	foreach(pit, parents)
		(*pit)->removeParentRecursive(parent);
}

bool ActionRule::wellFormed() {
	set<Term> vars;
	set<Term> addVars;
	map<Term, set<Term>> links;
	set<Term> linkTarget = toSet(actionLiteral.parameters);

	// Del effects are included in preconditions
	foreach(delit, del) {
		if (!in(preconditions, -*delit))
			return false;
		foreach(p, delit->parameters)
			linkTarget.insert(*p);
	}

	// Add effects are not in preconditions
	foreach(addit, add) {
		if (in(preconditions, *addit))
			return false;

		foreach(pit, addit->parameters) {
			addVars.insert(*pit);
			linkTarget.insert(*pit);
		}
	}

	// Any variable in add effects should appear in preconditions
	foreach(var, addVars) {
		if (!varOccurs(*var, preconditions)) return false;
	}

	// Any variable in preconditions should be linked to a variable in the action literal or in the effects
	foreach(precond, preconditions) {
		foreach(pit1, precond->parameters) {
			if (!in(vars, *pit1)) {
				vars.insert(*pit1);
				links[*pit1] = {};
			}

			foreach(pit2, precond->parameters) {
				if (*pit1 != *pit2) {
					links[*pit1].insert(*pit2);
				}
			}
		}
	}
	foreach(varit, vars)
		if (!linked(links, *varit, linkTarget)) return false;

	return true;
}

// Generality level of a node is the max of its parents generality levels + 1, OR 0 if it's a leaf (it is an example).
int ActionRule::generalityLevel() const {
	int maxGenerality = 0;
	foreach(ruleit, parents) {
		int parentGenerality = (*ruleit)->generalityLevel() + 1;
		if (parentGenerality > maxGenerality)
			maxGenerality = parentGenerality;
	}
	return maxGenerality;
}

int ActionRule::countLeaves() {
	if (parents.empty())
		return 1;
	
	int sum = 0;
	foreach(rule, parents)
		sum += (*rule)->countLeaves();
	return sum;
}

float ActionRule::maxLeafSimilarity(State state) {
	if (parents.empty())
		return State::similarity(state, State(preconditions));
	
	float maxSim = 0;
	foreach(rule, parents) {
		float sim = (*rule)->maxLeafSimilarity(state);
		if (maxSim < sim)
			maxSim = sim;
	}
	return maxSim;
}

shared_ptr<ActionRule> ActionRule::getLeastGeneralRuleCovering(shared_ptr<ActionRule> example) {
	shared_ptr<ActionRule> result = nullptr;
	if (covers(example)) result = shared_from_this();

	int minGenerality = -1;
	foreach(pit, parents) {
		shared_ptr<ActionRule> lgr = (*pit)->getLeastGeneralRuleCovering(example);
		if (lgr) {
			int genLevel = lgr->generalityLevel();

			if (minGenerality > genLevel || minGenerality == -1) {
				minGenerality = genLevel;
				result = lgr;
			}
		}
	}

	return result;
}

vector<SigmaTheta> ActionRule::applies(State state, vector<Term> instances, Literal inActionLiteral, bool onlyFirst=false) {
	if (inActionLiteral != actionLiteral) return vector<SigmaTheta>();

	Substitution sigma = Substitution(actionLiteral.parameters, inActionLiteral.parameters);

	set<Term> uncovered = sigma.getUncovered(toSet(parameters));
	vector<Substitution> thetas = Substitution().expandUncovered(uncovered, instances, true);

	vector<SigmaTheta> validatedSigmaThetas;

	foreach(subit, thetas) {
		SigmaTheta sigmaTheta = SigmaTheta(sigma, *subit);

		bool preconditionsVerified = true;
		foreach(precondit, preconditions) {
			Literal precond = sigmaTheta.st.apply(*precondit);
			if (!state.contains(precond)) {
				preconditionsVerified = false;
				break;
			}
		}

		if (preconditionsVerified) {
			validatedSigmaThetas.push_back(sigmaTheta);
			if (onlyFirst) break;
		}
	}

	return validatedSigmaThetas;
}

State ActionRule::apply(State state, SigmaTheta sigmaTheta) {
	State newState = State(state);
	newState.addFacts(sigmaTheta.st.apply(add));
	newState.removeFacts(sigmaTheta.st.apply(del));
	return newState;
}

int ActionRule::specificity() const {
	int specificity = 0;
	foreach(precond, preconditions) {
		specificity++;
		foreach(param, precond->parameters)
			if (!param->isVariable)
				specificity++;
	}
	return specificity;
}

void ActionRule::setRemovedPreconditions(set<Literal> remPreconds) {
	removedPreconditions = remPreconds;

	foreach(it, removedPreconditions)
		foreach(pit, (*it).parameters)
			insertUnique(&parameters, *pit);
}

float ActionRule::fulfilmentProbability(State state, Literal action, vector<Term> instances, /*r*/ bool& prematches, /*r*/ set<Substitution>& subs) {
	subs = this->prematchingSubs(make_shared<ActionRule>(Trace(state, action, true, state), startPu, false));
	prematches = subs.size() > 0;
	generateRandomSubs(state, action, instances, Substitution(), Substitution(), SUBS_FOR_FULFILMENT, subs);

	return 1.0f - computeCdProb(state, action, toVec(subs));
}

void ActionRule::generateRandomSubs(State state, Literal action, vector<Term> instances, Substitution rho, Substitution sigma, size_t maxRandomSubs, set<Substitution>& subs) {
	set<Term> genVars;
	set<Term> varsToMap;
	set<Term> remainConstants;
	set<Term> linkedVars;

	// First, for each parameter in action literal...
	Literal preSubbedActLit = sigma.apply(rho.apply(actionLiteral));
	foreachindex(i, preSubbedActLit.parameters) {
		Term t1 = preSubbedActLit.parameters[i];
		Term t2 = action.parameters[i];

		// Parameter is still equal to the original
		if (t1 == t2) {
			if (t1 == actionLiteral.parameters[i])
				remainConstants.insert(t1);

			linkedVars.insert(t1);
			continue;
		}

		// We applied subs already, so if t2 has an inverse and it is not equal to t1, there is a problem
		if (sigma.getInverse(t2).there) return;

		if (t1.isVariable) {
			// Associate variable
			sigma.set(t1, t2);

			linkedVars.insert(t1);
		}
		else {
			// Temporarily generalize term (generated variable is not to map as it is directly mapped to t2)
			Variable var = makeNewVar(genVars, t1);
			rho.set(t1, var);
			sigma.set(var, t2);

			linkedVars.insert(var);
		}
	}

	// Second, for each add and del effects...
	foreach(a, add) {
		Literal genAdd = rho.apply(*a);
		foreach(param, genAdd.parameters) {
			if (sigma.apply(*param).isVariable) {
				// Variable (generalized or originally) is yet to map
				varsToMap.insert(*param);
				linkedVars.insert(*param);
			}
			else if (!param->isVariable && !in(remainConstants, *param)) {
				// Constant was not generalized and doesn't have to remain constant
				// -> We create a new generalization variable and it is yet to map
				Variable var = makeNewVar(genVars, *param);
				rho.set(*param, var);
				varsToMap.insert(var);
				linkedVars.insert(var);
			}
		}
	}

	foreach(d, del) {
		Literal genDel = rho.apply(*d);
		foreach(param, genDel.parameters) {
			if (sigma.apply(*param).isVariable) {
				// Variable (generalized or originally) is yet to map
				varsToMap.insert(*param);
				linkedVars.insert(*param);
			}
			else if (!param->isVariable && !in(remainConstants, *param)) {
				// Constant was not generalized and doesn't have to remain constant
				// -> We create a new generalization variable and it is yet to map
				Variable var = makeNewVar(genVars, *param);
				rho.set(*param, var);
				varsToMap.insert(var);
				linkedVars.insert(var);
			}
		}
	}

	// Third, for each remaining parameter of ActionRule...
	foreach(pit, parameters) {
		Term gen = rho.apply(*pit);

		if (sigma.apply(gen) != gen) continue; // Term was already mapped to a value
		if (in(remainConstants, gen)) continue; // Term is constant and should remain as such

		// Otherwise...
		if (gen.isVariable) {
			// It was either generalized, or a variable originally
			// -> It is yet to map
			varsToMap.insert(gen);
		}
		else {
			// We CHOOSE not to generalize constant terms that do not appear in action literal or in effects
			// It is due to the fact that only a few substitutions can be considered, so we limit generalizations to the most important terms
			//remainConstants.insert(gen);
		}
	}

	// Fourth, for every removed precondition, if there are still variables to assign...
	foreach(remPrec, removedPreconditions) {
		foreach(param, remPrec->parameters) {
			Term gen = rho.apply(*param);

			if (sigma.apply(gen) != gen) continue;
			if (in(remainConstants, gen)) continue;

			if (gen.isVariable)
				varsToMap.insert(gen);
			else {
				//remainConstants.insert(gen);
			}
		}
	}

	// Fifth, we gather every remaining instances that we can still assign our variables to
	set<Term> availableInstances;
	foreach(inst, instances)
		if (!in(remainConstants, *inst) && !sigma.getInverse(*inst).there)
			availableInstances.insert(*inst);

	// Computing the maximum number of substitutions that could be formed from there
	int maxSubs = 1;
	foreachindex(i, varsToMap)
		maxSubs *= max(0, (int)availableInstances.size() - (int)i);

	// If the maximum number of theoretical substitutions exceeds the number to sample, randomly generate them
	if (maxSubs > maxRandomSubs) {

		// 1. Sort the variables to map in order of decreasing necessity impact
		vector<pair<float, Term>> sortedVariablesToMap;
		foreach(var, varsToMap) {
			float necessityImpact = 0.0f;

			// Necessity impact of a variable:
			// A. Sum up the necessities of every precondition the variable appears in
			foreach(prec, preconditions)
				if (in(rho.apply(*prec).parameters, *var))
					necessityImpact += precondsNecessities[*prec];
			// B. If variable is a generalization of a constant, add up that constant's necessity
			Opt<Term> original = rho.getInverse(*var);
			if (original.there && !original.obj.isVariable)
				necessityImpact += constsNecessities[original.obj];
			
			// Registering necessity impacts negated so that the biggest ones are sorted at the beginning of the vector
			sortedVariablesToMap.push_back({ -necessityImpact, *var });
		}
		sort(sortedVariablesToMap.begin(), sortedVariablesToMap.end());

		//	2. For each substitution to sample
		for (size_t i = subs.size(); i < maxRandomSubs; i++) {
			Substitution randomSigma = sigma;
			set<Term> instancesLeft = availableInstances;

			// 2.1. For each variable to map in order of decreasing necessity impact
			foreach(pair, sortedVariablesToMap) {
				Term var = pair->second;
				if (instancesLeft.size() == 0) break;

				vector<float> necessityLosses;

				// 2.1.1. Evaluate the necessity loss of each remaining instance, given current substitutions
				foreach(instit, instancesLeft) {
					Term inst = *instit;
					float necessityLoss = 0.0f;

					Substitution tempSigma = randomSigma;
					tempSigma.set(var, inst);

					// Necessity loss of a variable substitution:
					foreach(prec, preconditions) {
						// A. Apply the new substitution to all preconditions
						Literal subbed = tempSigma.apply(rho.apply(*prec));

						// B. For each grounded fact from precs·sub which is not in state, accumulate precondition's necessity value
						if (subbed.grounded()) {
							if (!state.contains(subbed))
								necessityLoss += precondsNecessities[*prec];
						}
						// C. For each non-grounded fact from precs·sub:
						else {
							// C.1. Query the state for facts corresponding to the non-grounded fact
							set<Literal> query = state.query(subbed);

							// C.2. Check if state contains at least a fact which could be obtained with current sub and remaining instances
							bool found = false;
							foreach(q, query) {
								bool valid = true;
								foreachindex(pi, subbed.parameters) {
									if (!subbed.parameters[pi].isVariable) continue;
									if (in(instancesLeft, q->parameters[pi])) continue;

									valid = false;
									break;
								}
								if (valid) {
									found = true;
									break;
								}
							}

							// C.3. If it does not, accumulate precondition's necessity value
							if (!found)
								necessityLoss += precondsNecessities[*prec];
						}
					}

					// D. If a constant was contradicted by the substitution, accumulate its necessity value
					Opt<Term> original = rho.getInverse(var);
					if (original.there && !original.obj.isVariable && original.obj != inst)
						necessityLoss += constsNecessities[original.obj];
					if ((!original.there || original.obj != inst) && in(constsNecessities, inst))
						necessityLoss += constsNecessities[inst];

					necessityLosses.push_back(necessityLoss);
				}

				// 2.1.2. Sample an instance based by weighting low necessity losses more heavily
				vector<float> weights;
				
				float maxLoss = 0.0f;
				float lossSum = 0.0f;
				foreach(loss, necessityLosses) {
					maxLoss = max(maxLoss, *loss);
					lossSum += *loss;
				}
				maxLoss *= 2.0f;
				lossSum = maxLoss * (float)necessityLosses.size() - lossSum;

				foreach(loss, necessityLosses)
					weights.push_back((maxLoss - *loss) / lossSum);

				Term selectedInstance = *select_randomly_weighted(instancesLeft.begin(), weights);

				// 2.1.3. Add choice to current substitution
				randomSigma.set(var, selectedInstance);
				instancesLeft.erase(selectedInstance);
			}

			// 3. Return the set of sampled substitutions
			subs.insert(rho.merge(randomSigma));
		}
	}
	// Otherwise we can directly generate all possible substitutions
	else {
		vector<Substitution> expandedSigmas = sigma.expandUncovered(varsToMap, availableInstances, true);
		foreach(sig, expandedSigmas)
			subs.insert(rho.merge(*sig));
	}
}

float ActionRule::computeCdProb(State state, Literal action, vector<Substitution> subs) {
	if (!Literal::compatible(actionLiteral, action)) return 1.0f;

	set<Unverified> cds;

	foreach(sub, subs) {
		vector<Literal> unverifiedPreconditions;
		vector<Term> unpreservedConstants;

		foreach(pair, precondsNecessities)
			if (!state.contains(sub->apply(pair->first)))
				unverifiedPreconditions.push_back(pair->first);
		foreach(pair, constsNecessities)
			if (sub->apply(pair->first) != pair->first || (sub->getInverse(pair->first).there && sub->getInverse(pair->first).obj != pair->first))
				unpreservedConstants.push_back(pair->first);

		cds.insert({ unverifiedPreconditions, unpreservedConstants });
	}

	return cdProb(precondsNecessities, constsNecessities, toVec(cds));
}

float cdProbTree(vector<pair<Literal, float>>& precNecs, vector<pair<Term, float>>& cstNecs,
	size_t choiceIndex, float branchPower, vector<Unverified> cds,
	set<Literal> truePreconds, set<Term> trueConsts) {

	if (choiceIndex >= precNecs.size() + cstNecs.size())
		return branchPower;

	float choicePower = 0.0f;
	bool lit = true;
	Literal niPrec;
	Term niCst;
	if (choiceIndex < precNecs.size()) {
		choicePower = precNecs[choiceIndex].second;
		niPrec = precNecs[choiceIndex].first;
	}
	else {
		choicePower = cstNecs[choiceIndex - precNecs.size()].second;
		niCst = cstNecs[choiceIndex - precNecs.size()].first;
		lit = false;
	}

	vector<Unverified> cdsTrue, cdsFalse;
	bool pruneFalseBranch = false;
	bool foundChoiceInDisjunction = false;

	foreach(disj, cds) {
		if (disj->first.size() + disj->second.size() == 0) return 0.0f;

		if ((!lit || in(disj->first, niPrec)) && (lit || in(disj->second, niCst))) {
			foundChoiceInDisjunction = true;
			
			set<Literal> remPrecs = toSet(disj->first);
			set<Term> remCsts = toSet(disj->second);
			if (lit) remPrecs.erase(niPrec);
			else remCsts.erase(niCst);
			if (remPrecs.size() + remCsts.size() == 0)
				pruneFalseBranch = true;
			else
				cdsFalse.push_back({ toVec(remPrecs), toVec(remCsts) });
		}
		else {
			cdsTrue.push_back(*disj);
			cdsFalse.push_back(*disj);
		}
	}

	if (!foundChoiceInDisjunction)
		return cdProbTree(precNecs, cstNecs, choiceIndex + 1, branchPower, cdsTrue, truePreconds, trueConsts);

	set<Literal> newTruePreconds = truePreconds;
	set<Term> newTrueConsts = trueConsts;
	if (lit) newTruePreconds.insert(niPrec);
	else newTrueConsts.insert(niCst);
	float trueBranchValue = choicePower * branchPower;
	if (choicePower * branchPower >= PRECISION)
		trueBranchValue = cdProbTree(precNecs, cstNecs, choiceIndex + 1, branchPower * choicePower, cdsTrue, newTruePreconds, newTrueConsts);

	pruneFalseBranch |= branchPower * (1.0f - choicePower) < PRECISION;
	if (pruneFalseBranch)
		return trueBranchValue;

	return trueBranchValue + cdProbTree(precNecs, cstNecs, choiceIndex + 1, branchPower * (1.0f - choicePower), cdsFalse, truePreconds, trueConsts);
}

float ActionRule::cdProb(map<Literal, float>& precondNecs, map<Term, float>& constNecs, vector<Unverified> cds) {
	vector<pair<Literal, float>> precNecs;
	vector<pair<Term, float>> cstNecs;
	foreach(pair, precondNecs)
		if (pair->second == 1.0f)
			precNecs.insert(precNecs.begin(), *pair);
		else
			precNecs.push_back(*pair);
	foreach(pair, constNecs)
		if (pair->second == 1.0f)
			cstNecs.insert(cstNecs.begin(), *pair);
		else
			cstNecs.push_back(*pair);
	return cdProbTree(precNecs, cstNecs, 0, 1.0f, cds, {}, {});
}

float ActionRule::dgcdProb(map<Literal, float>& precondNecs, map<Term, float>& constNecs, Unverified disj, vector<Unverified> conditionalCds) {
	float dgcdVal = 0.0f;
	float condFactor = 1.0f;

	while (disj.first.size() > 0 || disj.second.size() > 0) {
		Literal niL;
		Term niT;
		float niVal;
		bool lit;
		if (disj.first.size() > 0) {
			niL = disj.first.back();
			disj.first.pop_back();
			lit = true;
			niVal = precondNecs[niL];
		}
		else {
			niT = disj.second.back();
			disj.second.pop_back();
			lit = false;
			niVal = constNecs[niT];
		}

		float cdVal = cdProb(precondNecs, constNecs, conditionalCds);
		float ngcdVal = niVal;
		if (cdVal > 0) {
			vector<Unverified> filteredCds;
			foreach(p, conditionalCds)
				if ((!lit || !in(p->first, niL)) && (lit || !in(p->second, niT)))
					filteredCds.push_back(*p);

			ngcdVal *= cdProb(precondNecs, constNecs, filteredCds) / cdVal;
		}

		dgcdVal += condFactor * ngcdVal;
		condFactor *= 1 - ngcdVal;

		vector<Unverified> tmpCds = conditionalCds;
		conditionalCds.clear();
		foreach(p, tmpCds) {
			vector<Literal> precs;
			if (lit) {
				foreach(prec, p->first)
					if (*prec != niL)
						precs.push_back(*prec);
			}
			else precs = p->first;

			vector<Term> csts;
			if (!lit) {
				foreach(cst, p->second)
					if (*cst != niT)
						csts.push_back(*cst);
			}
			else csts = p->second;

			conditionalCds.push_back({ precs, csts });
		}
	}

	return dgcdVal;
}

void ActionRule::processEffects(/*r*/ set<Unverified>& sigmaPos, /*r*/ set<Unverified>& sigmaNeg,
	State state, Literal action, State effects, vector<Term> instances) {

	set<Substitution> subs;
	generateRandomSubs(state, action, instances, Substitution(), Substitution(), SUBS_FOR_CORROBORATION, subs);
	
	foreach(sub, subs) {
		vector<Literal> unverifiedPreconditions;
		vector<Term> unpreservedConstants;

		foreach(pair, precondsNecessities)
			if (!state.contains(sub->apply(pair->first)))
				unverifiedPreconditions.push_back(pair->first);
		foreach(pair, constsNecessities)
			if (sub->apply(pair->first) != pair->first || (sub->getInverse(pair->first).there && sub->getInverse(pair->first).obj != pair->first))
				unpreservedConstants.push_back(pair->first);

		Unverified disj = { unverifiedPreconditions, unpreservedConstants };

		if (action == sub->apply(actionLiteral) && effects == State(sub->apply(add + del)))
			sigmaPos.insert(disj);
		else
			sigmaNeg.insert(disj);
	}
}

string ActionRule::toString() const {
	string result = "Preconds: ";

	bool first = true;
	foreach(precondit, preconditions) {
		if (!first) result += ", ";
		else first = false;
		result += precondit->toString() +":" + formatPercent(precondsNecessities.at(*precondit));
	}

	result += "\nRemoved preconds: ";
	first = true;
	foreach(precondit, removedPreconditions) {
		if (!first) result += ", ";
		else first = false;
		result += precondit->toString() + ":" + formatPercent(precondsNecessities.at(*precondit));
	}

	result += "\nConstants: ";
	first = true;
	foreach(pair, constsNecessities) {
		if (!first) result += ", ";
		else first = false;
		result += pair->first.toString() + ": " + formatPercent(pair->second);
	}

	result += "\nAction: " + actionLiteral.toString() + "\nEffects: ";

	result += join(", ", add);
	if (add.size() > 0 && del.size() > 0) result += ", ";
	result += join(", ", del);

	//result += "\nCorroboration: " + to_string(corroboration()) + " - Specificity: " + to_string(specificity());

	return result;
}

ostream& operator<<(ostream& os, ActionRule &rule) {
	os << rule.toString();
	return os;
}
