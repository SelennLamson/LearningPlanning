/**
 * @author Thomas Lamson
 * Contact: thomas.lamson@cea.fr
 */

#pragma once

#include <string>
#include <vector>
#include <set>
#include <map>
#include <iostream>
#include <assert.h>

#include "Utils.h"

using namespace std;

struct LogicObject {
	virtual string toString() const;
};

template<typename T>
struct Opt : public LogicObject {
	T obj;
	bool there;

	Opt() : there(false) { }
	Opt(T inObj) : there(true), obj(inObj) { }
	Opt(T inObj, bool inThere) : there(inThere), obj(inObj) { }

	string toString() const override {
		if (there)
			return obj.toString();
		else
			return "None";
	}
};

struct TermType : public LogicObject {
	string name;
	shared_ptr<TermType> parent;

	TermType(string inName, shared_ptr<TermType> inParent) : name(inName), parent(inParent) {}
	TermType(string inName) : name(inName), parent(nullptr) {}
	TermType() : name("base"), parent(nullptr) {}

	bool subsumes(shared_ptr<TermType> other);
	bool static typeSubsumes(shared_ptr<TermType> typeA, shared_ptr<TermType> typeB);

	string toString() const override;
};

struct Term : public LogicObject {
	string name;
	bool isVariable;
	shared_ptr<TermType> type;

	Term(string inName, bool inIsVar, shared_ptr<TermType> inType) : name(inName), isVariable(inIsVar), type(inType) {}
	Term(string inName, bool inIsVar) : name(inName), isVariable(inIsVar), type(nullptr) {}
	Term() : Term("NoName", false) {}

	bool operator==(Term const& other) const;
	bool operator!=(Term const& other) const;
	bool operator<(Term const& other) const;

	string toString() const override;
};

struct Literal;

struct Predicate : public LogicObject {
	string name;
	size_t arity;

	Predicate(string inName, size_t inArity) : name(inName), arity(inArity) { }
	Predicate() : Predicate("None", 0) { }

	bool operator==(Predicate const& other) const;
	bool operator!=(Predicate const& other) const;
	bool operator<(Predicate const& other) const;

	Literal operator()(vector<Term> params) const;
	Literal operator()() const;
	Literal operator()(Term p1) const;
	Literal operator()(Term p1, Term p2) const;
	Literal operator()(Term p1, Term p2, Term p3) const;

	string toString() const override;
};

struct Variable : public Term {
	Variable() : Term("NoName", true) {}
	Variable(string inName) : Term(inName, true) {}
	Variable(string inName, shared_ptr<TermType> inType) : Term(inName, true, inType) {}

	static Variable anyVar();
};

struct Instance : public Term {
	Instance(string inName) : Term(inName, false) {}
	Instance(string inName, shared_ptr<TermType> inType) : Term(inName, false, inType) {}
	Instance() : Instance("NoName") {}
};

struct Substitution;

struct Literal : public LogicObject {
	Predicate pred;
	vector<Term> parameters = vector<Term>();
	bool positive = true;

	Literal(Predicate inPred) : pred(inPred) {}
	Literal(Predicate inPred, bool inPos) : pred(inPred), positive(inPos) {}
	Literal(Predicate inPred, vector<Term> inParams) : pred(inPred), parameters(inParams) {}
	Literal(Predicate inPred, vector<Term> inParams, bool inPos) : pred(inPred), parameters(inParams), positive(inPos) {}
	Literal() : Literal(Predicate()) {}

	bool grounded() const;
	bool repeatsArg() const;
	bool unifies(Literal const& other) const;

	bool operator==(Literal const& other) const;
	bool operator!=(Literal const& other) const;
	bool operator<(Literal const& other) const;
	Literal operator-() const;
	Literal abs() const;

	string toString() const override;

	static bool compatible(Literal l1, Literal l2);
};

struct GroundedAction;

struct Substitution : public LogicObject {
public:
	Substitution(bool inInjective=true) : injective(inInjective) { }
	Substitution(Substitution const& source, bool inInjective) : injective(inInjective), mapping(source.mapping) { }
	Substitution(Substitution const& source) : Substitution(source, source.injective) { }
	Substitution(vector<Term> from, vector<Term> to, bool inInjective = true);

	map<Term, Term> getMapping() const;
	Opt<Term> get(Term from) const;
	Opt<Term> getInverse(Term to) const;
	bool contains(Term from) const;
	bool containsBoth(Term fromto) const;
	bool extends(Substitution other) const;
	bool checkInjective() const;

	void set(Term from, Term to);
	bool setSafe(Term from, Term to, bool onlyVar=true);
	bool setSafeMultiple(vector<Term> from, vector<Term> to, bool onlyVars=true);
	void remove(Term from);

	Substitution inverse() const;
	Literal apply(Literal lit) const;
	std::set<Literal> apply(std::set<Literal> lits) const;
	GroundedAction apply(GroundedAction act) const;
	Term apply(Term term) const;
	
	vector<Substitution> expandUncovered(vector<Term> from, vector<Term> to, bool skipConstants);
	vector<Substitution> expandUncovered(std::set<Term> from, vector<Term> to, bool skipConstants);
	vector<Substitution> expandUncovered(vector<Term> from, std::set<Term> to, bool skipConstants);
	vector<Substitution> expandUncovered(std::set<Term> from, std::set<Term> to, bool skipConstants);

	std::set<Term> getUncovered(std::set<Term> parameters);
	Substitution merge(Substitution other);
	void cleanConstants();
	bool unify(Literal const& from, Literal const& to);

	std::set<Substitution> oiSubsume(std::set<Literal> source, std::set<Literal> dst);
	std::set<Substitution> oiSubsume(vector<Literal> source, std::set<Literal> dst);

	bool operator== (Substitution const& other) const;
	bool operator!=(Substitution const& other) const;
	bool operator<(Substitution const& other) const;

	bool getInjective() const {
		return injective;
	}
	string toString() const override;

private:
	map<Term, Term> mapping;
	bool injective;
};

struct Action : public LogicObject {
	Literal actionLiteral;
	vector<Literal> truePrecond;
	vector<Literal> falsePrecond;
	vector<Literal> add;
	vector<Literal> del;
	vector<Term> parameters;

	Action(Literal inActionLiteral, vector<Literal> inTruePrecond, vector<Literal> inFalsePrecond, vector<Literal> inAdd, vector<Literal> inDel);
	Action(Literal inActionLiteral) : Action(inActionLiteral, vector<Literal>(), vector<Literal>(), vector<Literal>(), vector<Literal>()) { }
	Action() : Action(Literal(Predicate()), vector<Literal>(), vector<Literal>(), vector<Literal>(), vector<Literal>()) { }

	void initParams();

	bool operator==(Action const& other) const;
	bool operator!=(Action const& other) const;
	bool operator<(Action const& other) const;

	string toString() const override;
};

struct InstantiatedAction : public LogicObject {
	Action action;
	Substitution substitution;
	bool empty;

	InstantiatedAction(Action inAction, Substitution inSubstitution) : action(inAction), substitution(inSubstitution), empty(false) {}
	InstantiatedAction() : action(Action()), substitution(Substitution()), empty(true) {}

	string toString() const override;
};

struct GroundedAction;

struct State : public LogicObject {
	set<Literal> facts;

	State();
	State(const State& other);
	State(set<Literal> inFacts) : facts(inFacts) { };

	void addFact(Literal newFact);
	void addFacts(set<Literal> newFacts);
	void removeFact(Literal delFact);
	void removeFacts(set<Literal> delFacts);

	bool contains(Literal fact) const;
	set<Literal> query(Literal searchFor) const;
	vector<Substitution> unifyAction(Action action) const;
	void difference(State const& other, set<Literal>* added, set<Literal>* removed) const;
	
	static float distance(const State& s1, const State& s2);
	static float similarity(const State& s1, const State& s2);

	bool operator==(State const& other) const;
	bool operator!=(State const& other) const;
	bool operator<(State const& other) const;

	string toString() const override;
};

struct Goal : public LogicObject {
	vector<Literal> trueFacts;
	vector<Literal> falseFacts;

	Goal(Goal const& other) : trueFacts(other.trueFacts), falseFacts(other.falseFacts) { }
	Goal() { }

	bool reached(State state);

	string toString() const override;
};

struct Condition : public LogicObject {
	Literal lit;
	bool truth;

	Condition(Literal inLiteral, bool inTruth) : lit(inLiteral), truth(inTruth) { }
	Condition(Predicate inPred, vector<Term> inParams, bool inTruth) : Condition(Literal(inPred, inParams), inTruth) { }
	Condition() : Condition(Predicate(), {}, false) { }

	Condition ground(Substitution sub);
	bool reached(State state) const;

	bool operator==(Condition const& other) const;
	bool operator!=(Condition const& other) const;
	bool operator<(Condition const& other) const;

	string toString() const override;
};

struct GroundedAction : public LogicObject {
	Literal actionLiteral;
	vector<Condition> preConditions;
	vector<Condition> postConditions;

	GroundedAction(Literal inActionLiteral, vector<Condition> inPreconds, vector<Condition> inPostconds) :
		actionLiteral(inActionLiteral), preConditions(inPreconds), postConditions(inPostconds) { }
	GroundedAction(Action action, Substitution sub);
	GroundedAction(Action action) : GroundedAction(action, Substitution()) { }
	GroundedAction(InstantiatedAction instAct) : GroundedAction(instAct.action, instAct.substitution) { }
	GroundedAction() : GroundedAction(InstantiatedAction()) { }

	GroundedAction unify(Substitution sub);
	set<Term> getVariables();

	bool operator==(GroundedAction const& other) const;
	bool operator!=(GroundedAction const& other) const;
	bool operator<(GroundedAction const& other) const;

	string toString() const override;
};

struct Trace : public LogicObject {
	State state;
	Literal instAct;
	bool authorized;
	State newState;

	Trace() : Trace(State(), Literal(), false, State()) { }
	Trace(State inState, Literal inInstAct, bool inAuthorized, State inNewState) :
		state(inState), instAct(inInstAct), authorized(inAuthorized), newState(inNewState) { }

	bool operator==(Trace const& other) const;
	bool operator!=(Trace const& other) const;

	string toString() const override;
};

class Domain {
public:
	Domain(vector<shared_ptr<TermType>> inTypes, set<Predicate> inPreds, set<Term> inConsts, vector<Action> inActions);
	Domain() : Domain({}, {}, {},  {}) { }

	virtual Opt<State> tryAction(State state, vector<Term> instances, Literal actionLiteral, bool onlyAdd = false);
	Literal parseLiteral(string str, vector<Term> instances, bool action = false, bool verbose = true);

	vector<Action> getActions(bool learning = false);
	set<Literal> getActionLiterals(bool learning = false);
	set<Predicate> getPredicates();
	set<Term> getConstants();
	vector<shared_ptr<TermType>> getTypes();

	Predicate getPredByName(string name);
	Opt<Term> getConstantByName(string name);
	shared_ptr<TermType> getTypeByName(string name);
	Predicate getActionPredByName(string name);

	void addType(shared_ptr<TermType> type);
	void addPredicate(Predicate pred);
	void addConstant(Term cst);
	void addAction(Action action);
	void setResetState(State state);
	
public:
	vector<shared_ptr<TermType>> types;
	set<Predicate> predicates;
	set<Term> constants;
	vector<Action> actions;

	Predicate deletePred = Predicate();
	Predicate removeFactPred = Predicate();
	Action resetAction;
	Action deleteAction;
	Action removeFactAction;
	Opt<State> resetState;
	set<Literal> removedFacts;
};

struct Problem {
	shared_ptr<Domain> domain;
	set<Term> instances;
	State initialState;
	Goal goal;
	vector<Literal> headstartActions;

	Problem() : domain(nullptr), initialState(State()), goal(Goal()) { }
	Problem(shared_ptr<Domain> inDomain, set<Term> inInstances, State inInitialState, Goal inGoal) :
		domain(inDomain), instances(inInstances), initialState(inInitialState), goal(inGoal) { }

	Term getInstByName(string name);
};

set<Term> filterByType(set<Term> atoms, shared_ptr<TermType> type);
vector<Term> filterByType(vector<Term> atoms, shared_ptr<TermType> type);
vector<Term> filterDeleted(vector<Term> atoms, State state, Predicate deletePredicate);
shared_ptr<TermType> getMostGeneralType(shared_ptr<TermType> type);

template<typename Stream>
Stream& operator<<(Stream& os, LogicObject& obj) {
	os << obj.toString().c_str();
	return os;
}

