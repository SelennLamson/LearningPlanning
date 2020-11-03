/**
 * @author Thomas Lamson
 * Contact: thomas.lamson@cea.fr
 */

#include "Logic/Domain.h"
#include <iostream>
#include<stdio.h>
#include <stdarg.h>

string LogicObject::toString() const {
	return "LogicObject";
}

bool TermType::subsumes(shared_ptr<TermType> other) {
	return other != nullptr && (name == other->name || (other->parent != nullptr && subsumes(other->parent)));
}

bool TermType::typeSubsumes(shared_ptr<TermType> typeA, shared_ptr<TermType> typeB) {
	if (typeA == nullptr) return true;
	if (typeA != nullptr && typeB == nullptr) return false;
	return typeA->subsumes(typeB);
}

string TermType::toString() const {
	return name + "(" + (parent == nullptr ? "null" : parent->name) + ")";
}

bool Term::operator==(Term const& other) const {
	return name == other.name || (isVariable && other.isVariable && (name == Variable::anyVar().name || other.name == Variable::anyVar().name));
}

bool Term::operator!=(Term const& other) const {
	return !(operator==(other));
}

bool Term::operator<(Term const& other) const {
	return name < other.name;
}

string Term::toString() const {
	string res = "";
	if (isVariable) res += "?";
	res += name;
	if (type) res += ":" + type->name;
	return res;
}

bool Predicate::operator==(Predicate const& other) const {
	return name == other.name;
}

bool Predicate::operator!=(Predicate const& other) const {
	return !(*this == other);
}

bool Predicate::operator<(Predicate const& other) const {
	return name < other.name;
}

Literal Predicate::operator()(vector<Term> params) const {
	return Literal(*this, params);
}

Literal Predicate::operator()() const {
	assert(arity == 0);
	return operator()(vector<Term>{});
}

Literal Predicate::operator()(Term p1) const {
	assert(arity == 1);
	return operator()(vector<Term>{ p1 });
}

Literal Predicate::operator()(Term p1, Term p2) const {
	assert(arity == 2);
	return operator()(vector<Term>{ p1, p2 });
}

Literal Predicate::operator()(Term p1, Term p2, Term p3) const {
	assert(arity == 3);
	return operator()(vector<Term>{ p1, p2, p3 });
}

string Predicate::toString() const {
	return name;
}

Variable Variable::anyVar() {
	return Variable("ANY", nullptr);
}

bool Literal::grounded() const {
	foreach(it, parameters)
		if ((*it).isVariable)
			return false;
	return true;
}

bool Literal::repeatsArg() const {
	for (int i = 0; i < parameters.size() - 1; i++)
		for (int j = i + 1; j < parameters.size(); j++)
			if (i != j && parameters[i] == parameters[j])
				return true;
	return false;
}

bool Literal::unifies(Literal const& other) const {
	if (pred != other.pred) return false;

	foreachindex(i, parameters) {
		Term p = parameters[i];
		Term o = other.parameters[i];

		if (p != o && !p.isVariable)
			return false;
		if (p.isVariable && !TermType::typeSubsumes(p.type, o.type))
			return false;
	}

	return true;
}

bool Literal::operator==(Literal const& other) const {
	if (positive != other.positive) return false;
	if (pred != other.pred) return false;
	for (size_t i = 0; i < pred.arity; i++)
		if (parameters[i] != other.parameters[i])
			return false;
	return true;
}

bool Literal::operator!=(Literal const& other) const {
	return !((*this) == other);
}

bool Literal::operator<(Literal const& other) const {
	if (positive && !other.positive) return true;
	if (!positive && other.positive) return false;
	if (pred != other.pred) return pred < other.pred;
	for (size_t i = 0; i < pred.arity; i++)
		if (parameters[i] != other.parameters[i])
			return parameters[i] < other.parameters[i];
	return false;
}

Literal Literal::operator-() const {
	return Literal(pred, parameters, !positive);
}

Literal Literal::abs() const {
	return Literal(pred, parameters, true);
}

string Literal::toString() const {
	string result = (positive ? "" : "-") + pred.name + "(";
	bool first = true;
	foreach(pit, parameters) {
		if (!first) result += ", ";
		else first = false;
		result += pit->name;
	}
	result += ")";
	return result;
}

bool Literal::compatible(Literal l1, Literal l2) {
	return l1.pred == l2.pred && l1.positive == l2.positive;
}

Substitution::Substitution(vector<Term> from, vector<Term> to, bool inInjective) : injective(inInjective) {
	assert(from.size() == to.size());
	foreachindex(i, from)
		if (from[i] != to[i])
			set(from[i], to[i]);
}

map<Term, Term> Substitution::getMapping() const {
	return map<Term, Term>(mapping);
}

Opt<Term> Substitution::get(Term from) const {
	auto f = mapping.find(from);
	if (f != mapping.end())
		return Opt<Term>(f->second);
	return Opt<Term>();
}

Opt<Term> Substitution::getInverse(Term to) const {
	foreach(pairit, mapping)
		if (pairit->second == to)
			return Opt<Term>(pairit->first);
	return Opt<Term>();
}

void Substitution::set(Term from, Term to) {
	if (injective) {
		Opt<Term> opt = getInverse(to);
		assert(!opt.there || opt.obj == from);
	}

	mapping[from] = to;
}

bool Substitution::setSafe(Term from, Term to, bool onlyVar) {
	if (from == to) return true;
	if (onlyVar && !from.isVariable) return false;
	if (in(mapping, from)) return mapping[from] == to;
	if (injective && getInverse(to).there) return false;
	mapping[from] = to;
	return true;
}

bool Substitution::setSafeMultiple(vector<Term> from, vector<Term> to, bool onlyVars) {
	bool success = true;
	foreachindex(i, from)
		success &= setSafe(from[i], to[i], onlyVars);
	return success;
}

void Substitution::remove(Term from) {
	if (contains(from))
		mapping.erase(from);
}

bool Substitution::contains(Term from) const {
	return mapping.find(from) != mapping.end();
}

bool Substitution::containsBoth(Term fromto) const {
	foreach(pairit, mapping)
		if (pairit->first == fromto || pairit->second == fromto)
			return true;
	return false;
}

bool Substitution::extends(Substitution other) const {
	foreach(pairit, other.mapping) {
		Opt<Term> val = get(pairit->first);
		if (!val.there)
			return false;
		if (val.obj != pairit->second)
			return false;
	}
	return true;
}

bool Substitution::checkInjective() const {
	std::set<Term> values;
	foreach(pair, mapping)
		if (in(values, pair->second)) return false;
		else values.insert(pair->second);
	return true;
}

Substitution Substitution::inverse() const {
	Substitution invSub = Substitution(true);
	foreach(pair, mapping)
		invSub.set(pair->second, pair->first);
	return invSub;
}

Literal Substitution::apply(Literal lit) const {
	Literal newLit = Literal(lit.pred, lit.positive);

	foreach(it, lit.parameters) {
		auto subit = mapping.find(*it);
		if (subit != mapping.end()) {
			newLit.parameters.push_back((*subit).second);
		}
		else {
			newLit.parameters.push_back(*it);
		}
	}

	return newLit;
}

std::set<Literal> Substitution::apply(std::set<Literal> lits) const {
	std::set<Literal> res;
	foreach(l, lits)
		res.insert(apply(*l));
	return res;
}

GroundedAction Substitution::apply(GroundedAction act) const {
	vector<Condition> newPreconds, newPostconds;
	foreach(precond, act.preConditions)
		newPreconds.push_back(Condition(apply(precond->lit), precond->truth));
	foreach(postcond, act.postConditions)
		newPostconds.push_back(Condition(apply(postcond->lit), postcond->truth));

	return GroundedAction(apply(act.actionLiteral), newPreconds, newPostconds);
}

Term Substitution::apply(Term term) const {
	Opt<Term> opt = get(term);
	if (opt.there) return opt.obj;
	return term;
}

vector<Substitution> Substitution::expandUncovered(std::set<Term> from, vector<Term> to, bool skipConstants) {
	return expandUncovered(from, toSet(to), skipConstants);
}

vector<Substitution> Substitution::expandUncovered(vector<Term> from, std::set<Term> to, bool skipConstants) {
	return expandUncovered(toSet(from), to, skipConstants);
}

vector<Substitution> Substitution::expandUncovered(vector<Term> from, vector<Term> to, bool skipConstants) {
	return expandUncovered(toSet(from), toSet(to), skipConstants);
}

vector<Substitution> Substitution::expandUncovered(std::set<Term> from, std::set<Term> to, bool skipConstants) {
	vector<Substitution> generated, prevSubs;
	generated.push_back(*this);

	std::set<Term> uncovered = getUncovered(from);

	foreach(fromit, uncovered) {
		if (fromit->isVariable || !skipConstants) {
			prevSubs = generated;
			generated.clear();

			foreach(sit, prevSubs) {
				foreach(toit, to) {
					Substitution sub = *sit;

					if (!sub.getInverse(*toit).there && TermType::typeSubsumes(fromit->type, toit->type)) {
						sub.set(*fromit, *toit);
						generated.push_back(sub);
					}
				}
			}
		}
	}

	return generated;
}

set<Term> Substitution::getUncovered(std::set<Term> parameters) {
	std::set<Term> uncovered;

	foreach(it, parameters) {
		if (mapping.find(*it) == mapping.end()) {
			uncovered.insert(*it);
		}
	}

	return uncovered;
}

Substitution Substitution::merge(Substitution other) {
	Substitution result = Substitution(other, false);

	foreach(pairit, mapping) {
		Term from1 = (*pairit).first;
		Term to1 = (*pairit).second;

		if (other.contains(to1)) {
			Opt<Term> to2 = other.get(to1);

			if (to2.obj != from1) {
				result.set(from1, to2.obj);
			}
		}
		else {
			result.set(from1, to1);
		}
	}

	return result;
}

void Substitution::cleanConstants() {
	std::set<Term> toRem;
	foreach(pair, mapping)
		if (pair->first == pair->second)
			toRem.insert(pair->first);
	foreach(rem, toRem)
		remove(*rem);
}

bool Substitution::unify(Literal const& from, Literal const& to) {
	if (from.pred != to.pred) return false;
	if (from.parameters.size() != to.parameters.size()) return false;

	injective = false;

	foreachindex(i, from.parameters) {
		Term fromAtom = from.parameters[i];
		Term fromAtomSave = fromAtom;
		Opt<Term> fromAtomConv = get(fromAtom);
		if (fromAtomConv.there) fromAtom = fromAtomConv.obj;

		Term toAtom = to.parameters[i];
		Term toAtomSave = toAtom;
		Opt<Term> toAtomConv = get(toAtom);
		if (toAtomConv.there) toAtom = toAtomConv.obj;

		if (fromAtom == toAtom) continue;
		
		if (fromAtom.isVariable && !toAtom.isVariable) {
			set(fromAtom, toAtom);
			set(fromAtomSave, toAtom);
			continue;
		}

		if (toAtom.isVariable && !fromAtom.isVariable) {
			set(toAtom, fromAtom);
			set(toAtomSave, fromAtom);
			continue;
		}

		if (fromAtom.isVariable && toAtom.isVariable) {
			set(fromAtom, toAtom);
			set(fromAtomSave, toAtom);
			continue;
		}

		return false;
	}
	return true;
}

std::set<Substitution> Substitution::oiSubsume(std::set<Literal> source, std::set<Literal> dst) {
	return oiSubsume(toVec(source), dst);
}

std::set<Substitution> Substitution::oiSubsume(vector<Literal> source, std::set<Literal> dst) {
	if (source.size() == 0) return { *this };

	Literal firstSource = apply(source.back());
	source.pop_back();

	Term srcParam, dstParam;
	Opt<Term> currMapping, invMapping;
	Substitution sub;
	bool couldMatch;

	std::set<Substitution> subs;
	foreach(d, dst) {
		if (d->pred != firstSource.pred) continue;

		sub = Substitution(*this);
		couldMatch = true;
		foreachindex(pi, firstSource.parameters) {
			srcParam = firstSource.parameters[pi];
			dstParam = d->parameters[pi];
			currMapping = sub.get(srcParam);
			invMapping = sub.getInverse(dstParam);

			if (srcParam == dstParam)
				continue;

			if (srcParam.isVariable && !currMapping.there && !invMapping.there)
				sub.set(srcParam, dstParam);
			else {
				couldMatch = false;
				break;
			}
		}
		if (!couldMatch) continue;

		subs = subs + sub.oiSubsume(source, dst);
	}

	return subs;
}

bool Substitution::operator== (Substitution const& other) const {
	if (injective != other.injective) return false;
	if (mapping.size() != other.mapping.size()) return false;

	foreach(ait, mapping) {
		auto bit = other.mapping.find((*ait).first);
		if (bit == other.mapping.end())
			return false;
		if ((*bit).second != (*ait).second)
			return false;
	}

	return true;
}

bool Substitution::operator!=(Substitution const& other) const {
	return !(*this == other);
}

bool Substitution::operator<(Substitution const& other) const {
	if (injective && !other.injective) return true;
	if (!injective && other.injective) return false;
	if (mapping.size() != other.mapping.size()) return mapping.size() < other.mapping.size();

	vector<Term> keys;
	foreach(pairit, mapping)
		insertUnique(&keys, pairit->first);
	foreach(pairit, other.mapping)
		insertUnique(&keys, pairit->first);
	sort(keys.begin(), keys.end());

	foreach(it, keys) {
		Term key = *it;
		Opt<Term> val1 = get(key);
		Opt<Term> val2 = other.get(key);
		if (!val1.there) return true;
		if (!val2.there) return false;
		if (val1.obj != val2.obj) return val1.obj < val2.obj;
	}

	return false;
}

string Substitution::toString() const {
	string result = "";
	foreach(pairit, mapping) result += pairit->first.name + "/" + pairit->second.name + " ";
	return result;
}

Action::Action(Literal inActionLiteral, vector<Literal> inTruePrecond, vector<Literal> inFalsePrecond, vector<Literal> inAdd, vector<Literal> inDel)
	: actionLiteral(inActionLiteral), truePrecond(inTruePrecond), falsePrecond(inFalsePrecond), add(inAdd), del(inDel) {
	initParams();
}

void Action::initParams() {
	foreach(p, actionLiteral.parameters)
		insertUnique(&parameters, *p);
	foreach(precond, truePrecond)
		foreach(p, precond->parameters)
		insertUnique(&parameters, *p);
	foreach(precond, falsePrecond)
		foreach(p, precond->parameters)
		insertUnique(&parameters, *p);
	foreach(eff, add)
		foreach(p, eff->parameters)
		insertUnique(&parameters, *p);
	foreach(eff, del)
		foreach(p, eff->parameters)
		insertUnique(&parameters, *p);
}

bool Action::operator==(Action const& other) const {
	if (actionLiteral != other.actionLiteral) return false;
	if (truePrecond.size() != other.truePrecond.size()) return false;
	if (falsePrecond.size() != other.falsePrecond.size()) return false;
	if (add.size() != other.add.size()) return false;
	if (del.size() != other.del.size()) return false;

	foreachindex(i, truePrecond)
		if (truePrecond[i] != other.truePrecond[i]) return false;
	foreachindex(i, falsePrecond)
		if (falsePrecond[i] != other.falsePrecond[i]) return false;
	foreachindex(i, add)
		if (add[i] != other.add[i]) return false;
	foreachindex(i, del)
		if (del[i] != other.del[i]) return false;

	return true;
}

bool Action::operator!=(Action const& other) const {
	return !(*this == other);
}

bool Action::operator<(Action const& other) const {
	if (actionLiteral != other.actionLiteral) return actionLiteral < other.actionLiteral;
	if (truePrecond.size() != other.truePrecond.size()) return truePrecond.size() < other.truePrecond.size();
	if (falsePrecond.size() != other.falsePrecond.size()) return falsePrecond.size() < other.falsePrecond.size();
	if (add.size() != other.add.size()) return add.size() < other.add.size();
	if (del.size() != other.del.size()) return del.size() < other.del.size();

	foreachindex(i, truePrecond)
		if (truePrecond[i] != other.truePrecond[i]) return truePrecond[i] < other.truePrecond[i];
	foreachindex(i, falsePrecond)
		if (falsePrecond[i] != other.falsePrecond[i]) return falsePrecond[i] < other.falsePrecond[i];
	foreachindex(i, add)
		if (add[i] != other.add[i]) return add[i] < other.add[i];
	foreachindex(i, del)
		if (del[i] != other.del[i]) return del[i] < other.del[i];

	return false;
}

string Action::toString() const {
	string res = "";
	res += "-------------------------------------------------------\n";
	res += "Action literal: " + actionLiteral.toString() + "\n";
	if (truePrecond.size() > 0)
		res += "True preconds: " + join(", ", truePrecond) + "\n";
	if (falsePrecond.size() > 0)
		res += "False preconds: " + join(", ", falsePrecond) + "\n";
	if (add.size() > 0)
		res += "Add effects: " + join(", ", add) + "\n";
	if (del.size() > 0)
		res += "Add effects: " + join(", ", del) + "\n";
	res += "-------------------------------------------------------";
	return res;
}

string InstantiatedAction::toString() const {
	string res = action.actionLiteral.pred.name + "(";
	vector<Term> params;
	foreach(p, action.actionLiteral.parameters)
		params.push_back(substitution.get(*p).obj);
	res += join(", ", params);
	res += ")";
	return res;
}

State::State() {
	facts = set<Literal>();
}

State::State(const State& other) {
	foreach(it, other.facts) {
		assert((*it).grounded());
		facts.insert(*it);
	}
}

void State::addFact(Literal newFact) {
	facts.insert(newFact.abs());
}

void State::addFacts(set<Literal> newFacts) {
	foreach(f, newFacts)
		facts.insert(f->abs());
}

void State::removeFact(Literal delFact) {
	facts.erase(delFact.abs());
}

void State::removeFacts(set<Literal> delFacts) {
	foreach(f, delFacts)
		facts.erase(f->abs());
}

bool State::contains(Literal fact) const {
	return facts.find(fact) != facts.end();
}

set<Literal> State::query(Literal searchFor) const {
	set<Literal> result = set<Literal>();

	bool corresponds;

	Predicate searchForPred = searchFor.pred;
	foreach(it, facts) {
		Literal current = *it;

		if (current.pred == searchForPred &&
			current.parameters.size() == searchFor.parameters.size()) {
			corresponds = true;

			for (int pi = 0; pi < current.parameters.size(); pi++) {
				if (!searchFor.parameters[pi].isVariable &&
					searchFor.parameters[pi] != current.parameters[pi]) {
					corresponds = false;
					break;
				}
				else if (searchFor.parameters[pi].isVariable &&
					!TermType::typeSubsumes(searchFor.parameters[pi].type, current.parameters[pi].type)) {
					corresponds = false;
					break;
				}
			}

			if (corresponds) {
				result.insert(current);
			}
		}
	}

	return result;
}

vector<Substitution> State::unifyAction(Action action) const {
	vector<Substitution> allSubs = { Substitution() };
	vector<Substitution> newSubs;
	foreach(precond, action.truePrecond) {
		newSubs.clear();

		foreach(sub, allSubs) {
			Literal subPrecond = sub->apply(*precond);
			set<Literal> unifiedFacts = query(subPrecond);

			foreach(fact, unifiedFacts) {
				Substitution newSub = Substitution(*sub);

				bool valid = true;
				foreachindex(i, subPrecond.parameters)
					if (subPrecond.parameters[i].isVariable) {
						if (newSub.getInverse(fact->parameters[i]).there) {
							valid = false;
							break;
						}

						newSub.set(subPrecond.parameters[i], fact->parameters[i]);
					}
				if (valid) newSubs.push_back(newSub);
			}
		}
		allSubs = newSubs;
	}

	newSubs.clear();
	foreach(sub, allSubs) {
		bool ok = true;
		foreach(precond, action.falsePrecond)
			if (contains(sub->apply(*precond))) {
				ok = false;
				break;
			}
		if (ok) newSubs.push_back(*sub);
	}

	return newSubs;
}

void State::difference(State const& other, set<Literal>* added, set<Literal>* removed) const {
	added->clear();
	removed->clear();

	foreach(it, facts)
		if (!other.contains(*it))
			removed->insert(Literal(it->pred, it->parameters, false));
	foreach(it, other.facts)
		if (!contains(*it))
			added->insert(*it);
}

float State::distance(const State& s1, const State& s2) {
	float distance = 0;

	foreach(f1, s1.facts)
		if (!s2.contains(*f1))
			distance++;
	foreach(f2, s2.facts)
		if (!s1.contains(*f2))
			distance++;

	return distance / (s1.facts.size() + s2.facts.size() + 1.0f);
}

float State::similarity(const State& s1, const State& s2) {
	float dist = State::distance(s1, s2);
	float maxDist = 1.0f;
	/*foreach(f, s1.facts)
		maxDist += (float) f->pred.arity;
	foreach(f, s2.facts)
		maxDist += (float) f->pred.arity;*/
	return 1.0f - dist * 1.0f / (maxDist * 1.0f);
}

bool State::operator==(State const& other) const {
	if (facts.size() != other.facts.size()) return false;

	foreach(f, facts)
		if (!other.contains(*f))
			return false;

	return true;
}

bool State::operator!=(State const& other) const {
	return !(*this == other);
}

bool State::operator<(State const& other) const {
	if (facts.size() != other.facts.size()) return facts.size() < other.facts.size();

	auto it1 = facts.begin();
	auto it2 = other.facts.begin();
	while (it1 != facts.end()) {
		if (*it1 != *it2)
			return *it1 < *it2;
		it1++;
		it2++;
	}

	return false;
}

string State::toString() const {
	return join(", ", facts);
}

bool Goal::reached(State state) {
	foreach(f, trueFacts)
		if (!state.contains(*f))
			return false;
	foreach(f, falseFacts)
		if (state.contains(*f))
			return false;
	return true;
}

string Goal::toString() const {
	return "Goal: " + join(", ", trueFacts) + " AND NOT " + join(", ", falseFacts);
}

Condition Condition::ground(Substitution sub) {
	return Condition(sub.apply(lit), truth);
}

bool Condition::reached(State state) const {
	if (!lit.grounded()) return false;
	return state.contains(lit) == truth;
}

bool Condition::operator==(Condition const& other) const {
	return lit == other.lit && truth == other.truth;
}

bool Condition::operator!=(Condition const& other) const {
	return !(*this == other);
}

bool Condition::operator<(Condition const& other) const {
	if (truth != other.truth) return truth;
	return lit < other.lit;
}

string Condition::toString() const {
	if (truth) return lit.toString();
	else return "!" + lit.toString();
}

GroundedAction::GroundedAction(Action action, Substitution sub) : actionLiteral(sub.apply(action.actionLiteral)) {
	foreach(pit, action.truePrecond)
		preConditions.push_back(Condition(sub.apply(*pit), true));
	foreach(pit, action.falsePrecond)
		preConditions.push_back(Condition(sub.apply(*pit), false));

	foreach(pit, action.add)
		postConditions.push_back(Condition(sub.apply(*pit), true));
	foreach(pit, action.del)
		postConditions.push_back(Condition(sub.apply(*pit), false));
}

GroundedAction GroundedAction::unify(Substitution sub) {
	Literal newActionLiteral = sub.apply(actionLiteral);

	vector<Condition> newPreconds;
	foreach(it, preConditions)
		newPreconds.push_back(Condition(sub.apply(it->lit), it->truth));

	vector<Condition> newPostconds;
	foreach(it, postConditions)
		newPreconds.push_back(Condition(sub.apply(it->lit), it->truth));

	return GroundedAction(newActionLiteral, newPreconds, newPostconds);
}

set<Term> GroundedAction::getVariables() {
	set<Term> vars;
	foreach(pit, actionLiteral.parameters)
		if (pit->isVariable)
			vars.insert(*pit);
	foreach(cit, preConditions)
		foreach(pit, cit->lit.parameters)
		if (pit->isVariable)
			vars.insert(*pit);
	foreach(cit, postConditions)
		foreach(pit, cit->lit.parameters)
		if (pit->isVariable)
			vars.insert(*pit);
	return vars;
}

bool GroundedAction::operator==(GroundedAction const& other) const {
	return actionLiteral == other.actionLiteral;
}

bool GroundedAction::operator!=(GroundedAction const& other) const {
	return !(*this == other);
}

bool GroundedAction::operator<(GroundedAction const& other) const {
	return actionLiteral < other.actionLiteral;
}

string GroundedAction::toString() const {
	return actionLiteral.toString();
}

bool Trace::operator==(Trace const& other) const {
	return state == other.state && instAct == other.instAct && authorized == other.authorized && newState == other.newState;
}

bool Trace::operator!=(Trace const& other) const {
	return !(*this == other);
}

string Trace::toString() const {
	string res = "";
	res += "-------------------------------------------------------\n";
	res += "Initial state: " + state.toString() + "\n";
	res += "Action: " + instAct.toString() + " - " + (authorized ? "Authorized" : "Illegal") + "\n";
	res += "Final state: " + newState.toString() + "\n";
	res += "-------------------------------------------------------";
	return res;
}

vector<Substitution> unifyFacts(State state, vector<Term> instances, vector<Literal> facts, Substitution sub, bool trueFacts) {
	if (facts.size() == 0) return { sub };
	Literal fact = facts.back();
	facts.pop_back();

	vector<Substitution> expanded = sub.expandUncovered(fact.parameters, instances, true);

	vector<Substitution> result;
	foreach(newSub, expanded) {
		bool found = state.contains(newSub->apply(fact));

		if ((found && trueFacts) || (!found && !trueFacts))
			result = result + unifyFacts(state, instances, facts, *newSub, trueFacts);
	}
	return result;
}

Domain::Domain(vector<shared_ptr<TermType>> inTypes, set<Predicate> inPreds, set<Term> inConsts, vector<Action> inActions) :
	types(inTypes), predicates(inPreds), constants(inConsts), actions(inActions) {

	Predicate resetPred = Predicate();
	foreach(pred, predicates)
		if (pred->name == "reset")
			resetPred = *pred;
		else if (pred->name == "delete")
			deletePred = *pred;
		else if (pred->name == "remove-fact")
			removeFactPred = *pred;
	
	if (resetPred == Predicate()) {
		resetPred = Predicate("reset", 0);
		addPredicate(resetPred);
	}
	
	if (deletePred == Predicate()) {
		deletePred = Predicate("delete", 1);
		addPredicate(deletePred);
	}

	if (removeFactPred == Predicate()) {
		removeFactPred = Predicate("remove-fact", 1);
		addPredicate(removeFactPred);
	}

	resetAction = Action(Literal(resetPred, {}, true), {}, {}, {}, {});

	Variable obj = Variable("obj");
	deleteAction = Action(Literal(deletePred, { obj }), {}, {}, {}, {});
	removeFactAction = Action(Literal(removeFactPred, { obj }), {}, {}, {}, {});
}

Opt<State> Domain::tryAction(State state, vector<Term> instances, Literal actionLiteral, bool onlyAdd) {
	vector<Term> allInsts = instances + constants;

	if (actionLiteral.pred == resetAction.actionLiteral.pred) {
		if (resetState.there)
			return resetState;
		else
			return Opt<State>(state, false);
	}

	if (actionLiteral.pred == deleteAction.actionLiteral.pred) {
		if (state.contains(actionLiteral))
			return Opt<State>(state, false);

		State newState;
		foreach(fact, state.facts)
			if (!in(fact->parameters, actionLiteral.parameters[0]))
				newState.addFact(*fact);
		newState.addFact(actionLiteral);
		return Opt<State>(newState, true);
	}

	if (actionLiteral.pred == removeFactAction.actionLiteral.pred) {
		if (actionLiteral.parameters.size() == 0) {
			State newState = State(state);
			newState.addFacts(removedFacts);
			removedFacts = {};
			return Opt<State>(newState, true);
		}
		else {
			Literal toRemove = parseLiteral(actionLiteral.parameters[0].name, instances, false);

			if (state.contains(toRemove))
				removedFacts.insert(toRemove);

			State newState = State(state);
			newState.removeFact(toRemove);
			return Opt<State>(newState, true);
		}
	}

	foreach(pit, actionLiteral.parameters)
		if (state.contains(Literal(deletePred, { *pit })))
			return Opt<State>(state, false);

	foreach(act, actions) {
		if (act->actionLiteral.pred != actionLiteral.pred) continue;

		Substitution sub;
		bool valid = true;
		foreachindex(pi, act->actionLiteral.parameters) {
			Term actParam = act->actionLiteral.parameters[pi];
			Term litParam = actionLiteral.parameters[pi];

			if (!TermType::typeSubsumes(actParam.type, litParam.type)) {
				valid = false;
				break;
			}

			if (actParam == litParam)
				continue;
			if (sub.getInverse(litParam).there)
				continue;

			sub.set(actParam, litParam);
		}
		if (!valid) continue;

		vector<Substitution> positiveSubs = unifyFacts(state, allInsts, act->truePrecond, sub, true);

		if (positiveSubs.empty()) continue;
		vector<Substitution> allSubs;
		foreach(pSub, positiveSubs)
			allSubs = allSubs + unifyFacts(state, allInsts, act->falsePrecond, *pSub, false);
		if (allSubs.empty()) continue;

		Substitution appliedSub = allSubs.back();

		State newState = State(state);
		newState.addFacts(appliedSub.apply(toSet(act->add)));
		if (!onlyAdd)
			newState.removeFacts(appliedSub.apply(toSet(act->del)));

		return Opt<State>(newState, true);
	}

	return Opt<State>(state, false);
}

Literal Domain::parseLiteral(string str, vector<Term> instances, bool action, bool verbose) {
	Predicate pred;
	vector<Term> params;
	bool positive = true;
	bool first = true;
	char c = ' ';
	string tmp = "";

	vector<Term> allInsts = instances + constants;

	for (unsigned int i = 0; i < str.length() + 1; i++) {
		if (i < str.length())
			c = str[i];

		if (i == str.length() || c == ' ' || c == '(' || c == ' ' || c == ')' || c == ',') {
			if (tmp != "") {
				if (first) {
					first = false;

					if (action)
						pred = getActionPredByName(tmp);
					else
						pred = getPredByName(tmp);

					if (pred == Predicate()) {
						if (verbose) cout << "Didn't recognized predicate \"" << tmp << "\"." << endl;
					}
				}
				else {
					Term inst;
					bool found = false;
					foreach(it, allInsts) {
						if (it->name == tmp) {
							inst = *it;
							found = true;
							break;
						}
					}
					if (found)
						params.push_back(inst);
					else {
						if (verbose) cout << "Didn't recognized object \"" << tmp << "\"." << endl;
						return Literal();
					}
				}

				tmp = "";
			}
		}
		else {
			if (c == '-' && tmp == "" && first)
				positive = false;
			else
				tmp += c;
		}
	}

	if (params.size() != pred.arity) {
		if (verbose) cout << "Predicate " << pred.name << " requires " << pred.arity << " arguments." << endl;
		return Literal();
	}

	return Literal(pred, params, positive);
}

vector<Action> Domain::getActions(bool learning) {
	if (learning)
		return actions + vector<Action>{ resetAction, deleteAction, removeFactAction };
	return actions;
}

set<Literal> Domain::getActionLiterals(bool learning) {
	vector<Action> allActions = getActions(learning);
	set<Literal> res;
	foreach(act, allActions)
		res.insert(act->actionLiteral);
	return res;
}

set<Predicate> Domain::getPredicates() {
	return predicates;
}

set<Term> Domain::getConstants() {
	return constants;
}

vector<shared_ptr<TermType>> Domain::getTypes() {
	return types;
}

Predicate Domain::getPredByName(string name) {
	foreach(pred, predicates)
		if (pred->name == name)
			return *pred;
	return Predicate();
}

Opt<Term> Domain::getConstantByName(string name) {
	foreach(cst, constants)
		if (cst->name == name)
			return Opt<Term>(*cst);
	return Opt<Term>();
}

shared_ptr<TermType> Domain::getTypeByName(string name) {
	foreach(t, types)
		if ((*t)->name == name)
			return *t;
	return nullptr;
}

Predicate Domain::getActionPredByName(string name) {
	if (name == resetAction.actionLiteral.pred.name) return resetAction.actionLiteral.pred;
	if (name == deleteAction.actionLiteral.pred.name) return deleteAction.actionLiteral.pred;
	if (name == removeFactAction.actionLiteral.pred.name) return removeFactAction.actionLiteral.pred;

	foreach(act, actions)
		if (act->actionLiteral.pred.name == name)
			return act->actionLiteral.pred;
	return Predicate();
}

void Domain::addType(shared_ptr<TermType> type) {
	assert(!in(types, type));
	types.push_back(type);
}

void Domain::addPredicate(Predicate pred) {
	predicates.insert(pred);
}

void Domain::addConstant(Term cst) {
	constants.insert(cst);
}

void Domain::addAction(Action action) {
	actions.push_back(action);
}

void Domain::setResetState(State state) {
	resetState = Opt<State>(state);
}

Term Problem::getInstByName(string name) {
	Opt<Term> opt = domain->getConstantByName(name);
	if (opt.there) return opt.obj;

	foreach(inst, instances)
		if (inst->name == name)
			return *inst;
	return Instance();
}

set<Term> filterByType(set<Term> atoms, shared_ptr<TermType> type) {
	set<Term> res;
	foreach(atom, atoms)
		if (TermType::typeSubsumes(atom->type, type))
			res.insert(*atom);
	return res;
}

vector<Term> filterByType(vector<Term> atoms, shared_ptr<TermType> type) {
	vector<Term> res;
	foreach(atom, atoms)
		if (TermType::typeSubsumes(atom->type, type))
			res.push_back(*atom);
	return res;
}

vector<Term> filterDeleted(vector<Term> atoms, State state, Predicate deletePredicate) {
	vector<Term> res;
	foreach(atom, atoms)
		if (!state.contains(Literal(deletePredicate, { *atom })))
			res.push_back(*atom);
	return res;
}

shared_ptr<TermType> getMostGeneralType(shared_ptr<TermType> type) {
	if (type == nullptr) return nullptr;
	while (type->parent)
		type = type->parent;
	return type;
}
