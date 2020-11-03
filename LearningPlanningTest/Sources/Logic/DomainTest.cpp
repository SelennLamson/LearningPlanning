/**
 * @author Thomas Lamson
 * Contact: thomas.lamson@cea.fr
 */

#include "pch.h"

#include "Logic/Domain.h"

using namespace std;

class DomainTest : public ::testing::Test {
protected:
	void SetUp() override {
	}

	Predicate pred0 = Predicate("p0", 0);
	Predicate pred1 = Predicate("p1", 1);
	Predicate pred2 = Predicate("p2", 2);
	
	shared_ptr<TermType> type1 = make_shared<TermType>("t1");
	shared_ptr<TermType> type2_child1 = make_shared<TermType>("t2", type1);
	shared_ptr<TermType> type3_child1 = make_shared<TermType>("t3", type1);
	shared_ptr<TermType> type4_child2 = make_shared<TermType>("t4", type2_child1);
	shared_ptr<TermType> type5 = make_shared<TermType>("t5");

	Variable u = Variable("U");
	Variable v = Variable("V");
	Variable w = Variable("W");
	Variable x = Variable("X");
	Variable y = Variable("Y");
	Variable z = Variable("Z");

	Instance a = Instance("a");
	Instance b = Instance("b");
	Instance c = Instance("c");
	Instance d = Instance("d");
	Instance e = Instance("e");

	Action act = Action(pred2(u, v),
		{	// True preconds
			pred2(u, v),
			pred1(u)
		},
		{	// False preconds
			pred1(v)
		},
		{
			pred0(),
			pred2(v, u)
		},
		{
			-pred2(u, v)
		});
};

TEST_F(DomainTest, SubstitutionOps) {
	// MERGING
	Substitution sub1 = Substitution();
	sub1.set(a, x);
	sub1.set(b, y);
	sub1.set(d, w); // Not present in 2
	sub1.set(e, u);

	Substitution sub2 = Substitution();
	sub2.set(a, x); // Same
	sub2.set(y, z); // Bridge
	sub2.set(c, v); // Not present in 1
	sub2.set(e, y); // Already mapped in 1

	Substitution merged = Substitution(false);
	merged.set(a, x); // Same remains
	merged.set(b, z); // Bridge directly directs to new value...
	merged.set(y, z); // ... but we keep sub2's mapping
	merged.set(d, w); // Keep 1
	merged.set(c, v); // Keep 2
	merged.set(e, u); // First mapping cancels second mapping

	EXPECT_TRUE(sub1.merge(sub2) == merged);
	
	// EXTENSION CHECKS
	sub2 = Substitution(sub1);
	sub2.set(c, z);
	EXPECT_TRUE(sub2.extends(sub1));
	EXPECT_FALSE(sub1.extends(sub2));

	// INVERSE
	Substitution subInv = sub1.inverse();
	Substitution subInvMadeup;
	subInvMadeup.set(x, a);
	subInvMadeup.set(y, b);
	subInvMadeup.set(w, d);
	subInvMadeup.set(u, e);
	EXPECT_TRUE(subInv == subInvMadeup);

	// APPLY
	EXPECT_TRUE(sub1.apply(Literal(pred2, { a, w })) == Literal(pred2, { x, w }));
	EXPECT_TRUE(sub1.apply(Literal(pred2, { c, d })) == Literal(pred2, { c, w }));

	// UNCOVERED & EXPANSION
	Instance f = Instance("f");
	Instance g = Instance("g");
	EXPECT_TRUE(allEqNoOrder(sub1.getUncovered({ a, b, c, d, e, u, v, w, x, y, z }), { c, u, v, w, x, y, z }));

	EXPECT_TRUE(allEqNoOrder(sub1.expandUncovered(set<Term>{ c, a, v, z }, set<Term>{ x, y, c, f, g }, true),
		{
			Substitution({ v, z, a, b, d, e }, { c, f, x, y, w, u }),
			Substitution({ v, z, a, b, d, e }, { c, g, x, y, w, u }),
			Substitution({ v, z, a, b, d, e }, { f, c, x, y, w, u }),
			Substitution({ v, z, a, b, d, e }, { f, g, x, y, w, u }),
			Substitution({ v, z, a, b, d, e }, { g, c, x, y, w, u }),
			Substitution({ v, z, a, b, d, e }, { g, f, x, y, w, u })
		}
	)); // Should only expand V and Z, as A and C are constant and we skip them

	EXPECT_TRUE(allEqNoOrder(sub1.expandUncovered(set<Term>{ a, c, v }, set<Term>{ x, y, f, g }, false),
		{
			Substitution({ c, v, a, b, d, e }, { f, g, x, y, w, u }),
			Substitution({ c, v, a, b, d, e }, { g, f, x, y, w, u })
		}
	)); // Should only expand C and V, as A is already mapped to X

	// OI-SUBSUME
	sub1 = Substitution();
	sub1.set(x, a);
	sub1.set(y, c);
	set<Literal> source = {
		Literal(pred2, {x, y}),
		Literal(pred2, {y, u}),
		Literal(pred1, {x}),
		Literal(pred1, {z})
	};
	set<Literal> dest = {
		Literal(pred2, {a, c}),
		Literal(pred2, {c, b}),
		Literal(pred2, {c, e}),
		Literal(pred1, {e}),
		Literal(pred1, {a}),
		Literal(pred1, {b})
	};
	EXPECT_TRUE(allEqNoOrder(toVec(sub1.oiSubsume(source, dest)),
		{
			Substitution({ x, y, u, z }, { a, c, b, e }),
			Substitution({ x, y, u, z }, { a, c, e, b })
		}
	));

	source = {
		Literal(pred2, {x, y}),
		Literal(pred2, {y, u}),
		Literal(pred1, {x}),
		Literal(pred1, {z})
	};
	dest = {
		Literal(pred2, {a, b}),
		Literal(pred2, {b, c}),
		Literal(pred1, {e}),
		Literal(pred1, {a}),
		Literal(pred1, {b})
	};
	EXPECT_TRUE(allEqNoOrder(toVec(sub1.oiSubsume(source, dest)), {})); // Does not subsume with sub1's constraints

	source = {
		Literal(pred2, {x, y}),
		Literal(pred2, {y, u}),
		Literal(pred1, {x}),
		Literal(pred1, {z}),
		Literal(pred1, {w})
	};
	dest = {
		Literal(pred2, {a, b}),
		Literal(pred2, {b, c}),
		Literal(pred1, {e}),
		Literal(pred1, {a}),
		Literal(pred1, {b})
	};
	EXPECT_TRUE(allEqNoOrder(toVec(Substitution().oiSubsume(source, dest)), {})); // Does not subsume in any way (injectively, y=b so w=/=b)
}

TEST_F(DomainTest, TypesOps) {
	// First child
	EXPECT_TRUE(type1->subsumes(type2_child1));
	EXPECT_TRUE(TermType::typeSubsumes(type1, type2_child1));

	// Second child
	EXPECT_TRUE(type1->subsumes(type3_child1));
	EXPECT_TRUE(TermType::typeSubsumes(type1, type3_child1));

	// No link between children
	EXPECT_FALSE(type2_child1->subsumes(type3_child1));
	EXPECT_FALSE(TermType::typeSubsumes(type2_child1, type3_child1));

	// Two level inheritance
	EXPECT_TRUE(type1->subsumes(type4_child2));
	EXPECT_TRUE(TermType::typeSubsumes(type1, type4_child2));

	// No link between cousins
	EXPECT_FALSE(type3_child1->subsumes(type4_child2));
	EXPECT_FALSE(TermType::typeSubsumes(type3_child1, type4_child2));

	// Nothing subsumes null
	EXPECT_FALSE(type1->subsumes(nullptr));
	EXPECT_FALSE(TermType::typeSubsumes(type1, nullptr));
	EXPECT_FALSE(type2_child1->subsumes(nullptr));
	EXPECT_FALSE(TermType::typeSubsumes(type2_child1, nullptr));

	// Null subsumes all
	EXPECT_TRUE(TermType::typeSubsumes(nullptr, type1));
	EXPECT_TRUE(TermType::typeSubsumes(nullptr, type2_child1));
	EXPECT_TRUE(TermType::typeSubsumes(nullptr, nullptr));

	// Most general types
	EXPECT_TRUE(getMostGeneralType(type2_child1) == type1);
	EXPECT_TRUE(getMostGeneralType(type4_child2) == type1);
	EXPECT_TRUE(getMostGeneralType(type5) == type5);
	EXPECT_TRUE(getMostGeneralType(nullptr) == nullptr);
}

TEST_F(DomainTest, LiteralUnification) {
	// Different predicates
	Literal lit1 = Literal(pred1, { u });
	Literal lit2 = Literal(pred2, { u, v });
	EXPECT_FALSE(lit1.unifies(lit2));

	// Consts to variables: shouldn't unify
	lit1 = Literal(pred2, { a, b });
	lit2 = Literal(pred2, { u, v });
	EXPECT_FALSE(lit1.unifies(lit2));

	// Variables to consts, correct types, should unify
	Variable ut1 = Variable("U", type1);
	Variable vt5 = Variable("V", type5);
	Instance at4 = Instance("A", type4_child2);
	Instance bt5 = Instance("B", type5);
	lit1 = Literal(pred2, { ut1, vt5 });
	lit2 = Literal(pred2, { at4, bt5 });
	EXPECT_TRUE(lit1.unifies(lit2));
	
	// Variables to consts, wrong types, shouldn't unify
	Variable ut3 = Variable("U", type3_child1);
	lit1 = Literal(pred2, { ut3, vt5 });
	lit2 = Literal(pred2, { at4, bt5 });
	EXPECT_FALSE(lit1.unifies(lit2));
}

TEST_F(DomainTest, StateOps) {
	Instance at = Instance("at", type1);
	Instance bt = Instance("bt", type2_child1);
	Instance ct = Instance("ct", type5);

	Variable vt = Variable("vt", type1);

	State state({
		pred1(a),
		pred1(b),
		pred2(a, b),
		pred2(a, c),
		pred2(b, c),
		
		pred1(at),
		pred1(bt),
		pred2(at, bt),
		pred2(at, ct),
		pred2(bt, ct)
	});

	// Queries
	EXPECT_TRUE(allEqNoOrder(toVec(state.query(pred2(a, u))), {
		pred2(a, b),
		pred2(a, c)
	}));
	EXPECT_TRUE(allEqNoOrder(toVec(state.query(pred2(vt, u))), {
		pred2(at, bt),
		pred2(at, ct),
		pred2(bt, ct)
	}));
	EXPECT_TRUE(allEqNoOrder(toVec(state.query(pred1(c))), {}));

	// State distance and similarity
	State state2({
			pred1(a),
			pred2(a, b),
			pred2(a, c),

			pred1(at),
			pred1(bt),
			pred2(bt, ct)
		});

	EXPECT_TRUE(State::distance(state, state) == 0.0f);
	EXPECT_TRUE(State::distance(state, state2) == 4.0f / 17.0f);
	EXPECT_TRUE(State::similarity(state, state) == 1.0f);
	EXPECT_TRUE(State::similarity(state, state2) == 13.0f / 17.0f);

	// Difference
	set<Literal> added, removed;
	state2 = State({
		pred1(a),
		pred2(a, b),
		pred2(a, c),

		pred1(at),
		pred1(bt),
		pred2(bt, ct),

		pred1(c),
		pred2(d, e)
	});

	state.difference(state2, &added, &removed);
	EXPECT_TRUE(allEqNoOrder(toVec(added), { pred1(c), pred2(d, e) }) &&
				allEqNoOrder(toVec(removed), { -pred1(b), -pred2(b, c), -pred2(at, bt), -pred2(at, ct) }));

	state.difference(state, &added, &removed);
	EXPECT_TRUE(allEqNoOrder(toVec(added), {}) && allEqNoOrder(toVec(removed), {}));

	// Unify action
	EXPECT_TRUE(allEqNoOrder(state.unifyAction(act), {
		Substitution({u, v}, {a, c}),
		Substitution({u, v}, {b, c}),
		Substitution({u, v}, {at, ct}),
		Substitution({u, v}, {bt, ct}),
	}));
}

TEST_F(DomainTest, DomainOps) {
	shared_ptr<Domain> domain = make_shared<Domain>(
		vector<shared_ptr<TermType>>{ type1, type2_child1, type3_child1, type4_child2 },
		set<Predicate>{ pred0, pred1 },
		set<Term>{ },
		vector<Action>{ act });
	Term constant = Instance("cst", type1);

	// Get actions and action literals
	EXPECT_TRUE(domain->getActions(true).size() == 4);
	EXPECT_TRUE(allEq(domain->getActions(), { act }));
	EXPECT_TRUE(allEq(toVec(domain->getActionLiterals()), { act.actionLiteral }));

	// Meta actions
	EXPECT_TRUE(allEqNoOrder(toVec(domain->getActionLiterals(true)), {
		act.actionLiteral,
		Predicate("delete", 1)({ Variable("obj") }),
		Predicate("remove-fact", 1)({ Variable("obj") }),
		Predicate("reset", 0)()
	}));

	// Get members
	EXPECT_TRUE(allEqNoOrder(toVec(domain->getPredicates()),
		{ pred0, pred1, Predicate("delete", 1), Predicate("remove-fact", 1), Predicate("reset", 0) }));
	EXPECT_TRUE(allEqNoOrder(toVec(domain->getConstants()), { }));
	EXPECT_TRUE(allEqNoOrder(domain->getTypes(), { type1, type2_child1, type3_child1, type4_child2 }));

	// Adding members
	domain->addType(type5);
	domain->addPredicate(pred2);
	domain->addConstant(constant);
	EXPECT_TRUE(allEqNoOrder(toVec(domain->getPredicates()),
		{ pred0, pred1, pred2, Predicate("delete", 1), Predicate("remove-fact", 1), Predicate("reset", 0) }));
	EXPECT_TRUE(allEqNoOrder(toVec(domain->getConstants()), { constant }));
	EXPECT_TRUE(allEqNoOrder(domain->getTypes(), { type1, type2_child1, type3_child1, type4_child2, type5 }));

	// Name searches
	EXPECT_TRUE(domain->getPredByName("p1") == pred1);
	EXPECT_TRUE(domain->getPredByName("nonExistingPred") == Predicate());
	EXPECT_TRUE(domain->getConstantByName("cst").obj == constant);
	EXPECT_FALSE(domain->getConstantByName("nonExistingConstant").there);
	EXPECT_TRUE(domain->getTypeByName("t1") == type1);
	EXPECT_TRUE(domain->getTypeByName("nonExistingType") == nullptr);
	EXPECT_TRUE(domain->getActionPredByName("p2") == pred2);
	EXPECT_TRUE(domain->getActionPredByName("nonExistingPred") == Predicate());

	// Literal parsing
	vector<Term> instances { a, b, c, d, e };
	EXPECT_TRUE(domain->parseLiteral("p2(a, b)", instances, false) == pred2(a, b));
	EXPECT_TRUE(domain->parseLiteral("p1(e)", instances, false) == pred1(e));
	EXPECT_TRUE(domain->parseLiteral("p0()", instances, false) == pred0());
	EXPECT_TRUE(domain->parseLiteral("p2(d, e)", instances, true) == pred2(d, e));

	// Wrong parsing
	EXPECT_TRUE(domain->parseLiteral("p8(d, e)", instances, false, false) == Predicate());
	EXPECT_TRUE(domain->parseLiteral("p2(d, f)", instances, false, false) == Predicate());
	EXPECT_TRUE(domain->parseLiteral("p2g)aaazd, eb)", instances, false, false) == Predicate());

	// Negative literals
	EXPECT_TRUE(domain->parseLiteral("-p2(a, b)", instances, false) == -pred2(a, b));
	EXPECT_TRUE(domain->parseLiteral("-p1(e)", instances, false) == -pred1(e));
	EXPECT_TRUE(domain->parseLiteral("-p0()", instances, false) == -pred0());
	EXPECT_TRUE(domain->parseLiteral("-p2(d, e)", instances, true) == -pred2(d, e));

	// Performing action in legal and illegal states
	State state1({
		pred1(a),
		pred1(b),
		pred2(a, b),
		pred2(a, c),
		pred2(b, c)
	});

	State state2({
		pred1(a),
		pred1(b),
		pred1(c),
		pred2(a, c)
	});

	Opt<State> legalAct = domain->tryAction(state1, instances, pred2(a, c), false);
	Opt<State> illegalAct = domain->tryAction(state2, instances, pred2(a, c), false);
	Opt<State> onlyAddAct = domain->tryAction(state1, instances, pred2(a, c), true);

	EXPECT_TRUE(legalAct.there);
	EXPECT_FALSE(illegalAct.there);
	EXPECT_TRUE(onlyAddAct.there);

	EXPECT_TRUE(legalAct.obj == State(state1.facts + set<Literal>{pred0(), pred2(c, a)} - set<Literal>{pred2(a, c)}));
	EXPECT_TRUE(illegalAct.obj == state2);
	EXPECT_TRUE(onlyAddAct.obj == State(state1.facts + set<Literal>{pred0(), pred2(c, a)}));
}

