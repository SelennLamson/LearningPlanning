/**
 * @author Thomas Lamson
 * Contact: thomas.lamson@cea.fr
 */

#include "pch.h"

#include <memory>

#include "Logic/Domain.h"
#include "Agents/LearningAgent/LearningAgent.h"
#include "Agents/LearningAgent/ActionRule.h"
#include "Logic/JSON_Parsing.h"

using namespace std;

//ConfigReader* config = nullptr;

class LearningAgentTest : public ::testing::Test {
protected:
	void SetUp() override {
		config = ConfigReader::fromFile("config.json");

		movePred2 = Predicate("move", 2);
		movePred3 = Predicate("move", 3);
		Literal move_lit = movePred3(x, y, z);
		action_move = Action(move_lit);
		action_move.truePrecond = {
			on(x, z),
			block(x),
			clear(x),
			clear(y)
		};
		action_move.add = {
			clear(z),
			on(x, y),
		};
		action_move.del = {
			-clear(y),
			-on(x, z),
		};

		set<Predicate> preds;
		preds.insert(on);
		preds.insert(block);
		preds.insert(clear);

		vector<Action> actions = { action_move };
		domain = make_shared<Domain>(vector<shared_ptr<TermType>>(), preds, set<Term>(), actions);

		problem = make_shared<Problem>();
		problem->instances.insert(a);
		problem->instances.insert(b);
		problem->instances.insert(c);
		problem->instances.insert(d);
		problem->instances.insert(f1);
		problem->instances.insert(f2);

		problem->initialState = State();
		problem->initialState.facts = {
			block(a),
			block(b),
			on(a, b),
			on(b, f1),
			clear(a),
			clear(f2)
		};

		problem->goal = Goal();
		problem->goal.trueFacts = {
			on(a, f2),
			on(b, a)
		};

		trace = make_shared<vector<Trace>>();
		agent = make_shared<LearningAgent>(false);
		vector<Term> instances = toVec(problem->instances);
		agent->init(domain, instances, problem->goal, trace);
	}

	shared_ptr<Domain> domain;
	shared_ptr<Problem> problem;
	shared_ptr<LearningAgent> agent;
	shared_ptr<vector<Trace>> trace;

	Predicate on = Predicate("on", 2);
	Predicate block = Predicate("block", 1);
	Predicate block2 = Predicate("block2", 1);
	Predicate clear = Predicate("clear", 1);

	Predicate movePred2 = Predicate("move", 2);
	Predicate movePred3 = Predicate("move", 3);

	Action action_move;

	Variable x = Variable("X");
	Variable y = Variable("Y");
	Variable z = Variable("Z");

	Instance a = Instance("a");
	Instance b = Instance("b");
	Instance c = Instance("c");
	Instance d = Instance("d");
	Instance e = Instance("e");
	Instance f1 = Instance("f1");
	Instance f2 = Instance("f2");
	Instance f3 = Instance("f3");

	vector<Term> instances = { a, b, c, d, e, f1, f2, f3 };
};

TEST_F(LearningAgentTest, ActionRuleConstruction) {
	State initState = problem->initialState;

	Substitution sub = Substitution();
	sub.set(x, a);
	sub.set(y, f2);
	sub.set(z, b);

	Literal instAct = sub.apply(action_move.actionLiteral);

	Opt<State> optState = domain->tryAction(initState, instances, instAct);
	EXPECT_TRUE(optState.there);
	State newState = optState.obj;
	shared_ptr<ActionRule> rule = make_shared<ActionRule>(Trace(initState, instAct, true, newState), 0.01f, false);

	EXPECT_TRUE(rule->actionLiteral.pred == action_move.actionLiteral.pred);
	EXPECT_TRUE(allEqNoOrder(toVec(rule->preconditions), {
		block(a),
		block(b),
		on(a, b),
		on(b, f1),
		clear(a),
		clear(f2)
	}));
	EXPECT_TRUE(allEqNoOrder(toVec(rule->add), {
		on(a, f2),
		clear(b)
	}));
	EXPECT_TRUE(allEqNoOrder(toVec(rule->del), {
		-on(a, b),
		-clear(f2)
	}));
	EXPECT_TRUE(in(rule->parameters, a));
	EXPECT_TRUE(in(rule->parameters, b));
	EXPECT_TRUE(in(rule->parameters, f2));
}

TEST_F(LearningAgentTest, LitGenTests) {
	shared_ptr<ActionRule> rule = make_shared<ActionRule>(set<Literal>(), Literal(), set<Literal>(), set<Literal>(), set<shared_ptr<ActionRule>>(), 0.01f);

	Substitution theta1, theta2;
	set<Term> genVars;
	Opt<Literal> g;

	// on(a, b), on(a, b), {}, {} -> on(a, b), {}, {}
	theta1 = Substitution(); theta2 = Substitution(); genVars.clear();
	g = rule->generalizeLiteralsOI(on(a, b), on(a, b), genVars, theta1, theta2);

	EXPECT_TRUE(g.there && g.obj == on({ Variable::anyVar(), Variable::anyVar() })
		&& genVars.size() == 2
		&& theta1.getInverse(a).obj == theta2.getInverse(a).obj
		&& theta1.getInverse(b).obj == theta2.getInverse(b).obj);

	// on(a, b), on(b, a), {}, {} -> on(X, Y), {X/a, Y/b}, {X/b, Y/a}
	theta1 = Substitution(); theta2 = Substitution(); genVars.clear();
	g = rule->generalizeLiteralsOI(on(a, b), on(b, a), genVars, theta1, theta2);
	EXPECT_TRUE(g.there && g.obj == on({ Variable::anyVar(), Variable::anyVar() })
		&& genVars.size() == 2
		&& theta1.getInverse(a).there
		&& theta1.getInverse(b).there
		&& theta2.getInverse(a).there
		&& theta2.getInverse(b).there);

	// on(X, b), on(a, b), {}, {} -> on(X, b), {}, {X/a}
	theta1 = Substitution(); theta2 = Substitution(); genVars.clear();
	g = rule->generalizeLiteralsOI(on(x, b), on(a, b), genVars, theta1, theta2);
	theta1.cleanConstants();
	theta2.cleanConstants();
	EXPECT_TRUE(g.there && g.obj == on({ x, Variable::anyVar() })
		&& allEqNoOrder(genVars, { Variable::anyVar() })
		&& theta1.getInverse(b).obj == theta2.getInverse(b).obj
		&& theta2.getInverse(a).obj == x);

	// on(X, c), on(a, b), {}, {} -> on(X, Y), {Y/c}, {X/a, Y/b}
	theta1 = Substitution(); theta2 = Substitution(); genVars.clear();
	g = rule->generalizeLiteralsOI(on(x, c), on(a, b), genVars, theta1, theta2);
	EXPECT_TRUE(g.there && g.obj == on({ x, Variable::anyVar() })
		&& allEqNoOrder(genVars, { Variable::anyVar() })
		&& theta1.getInverse(c).there
		&& theta2.getInverse(a).obj == x
		&& theta2.getInverse(b).there);

	// on(a, b), block(a), {}, {} -> None
	theta1 = Substitution(); theta2 = Substitution(); genVars.clear();
	g = rule->generalizeLiteralsOI(on(a, b), block(a), genVars, theta1, theta2);
	EXPECT_TRUE(!g.there
		&& allEqNoOrder(genVars, {})
		&& theta1 == Substitution()
		&& theta2 == Substitution());

	// on(X, b), on(b, c), {}, {} -> None
	theta1 = Substitution(); theta2 = Substitution({ y }, { b }); genVars.clear();
	g = rule->generalizeLiteralsOI(on(x, b), on(b, c), genVars, theta1, theta2);
	EXPECT_TRUE(!g.there
		&& allEqNoOrder(genVars, {})
		&& theta1 == Substitution()
		&& theta2 == Substitution({ y }, { b }));

	// on(X, Y), on(X, Y), {}, {} -> on(X, Y)
	theta1 = Substitution(); theta2 = Substitution(); genVars.clear();
	g = rule->generalizeLiteralsOI(on(x, y), on(x, y), genVars, theta1, theta2);
	EXPECT_TRUE(g.there && g.obj == on(x, y)
		&& allEqNoOrder(genVars, {})
		&& theta1 == Substitution()
		&& theta2 == Substitution());

	// on(X, Y), on(X, Y), {X/X, Y/Y}, {X/X, Y/Y} -> on(X, Y), {X/X, Y/Y}, {X/X, Y/Y}
	theta1 = Substitution({x, y}, {x, y}); theta2 = Substitution({ x, y }, { x, y }); genVars.clear();
	g = rule->generalizeLiteralsOI(on(x, y), on(x, y), genVars, theta1, theta2);
	EXPECT_TRUE(g.there && g.obj == on(x, y)
		&& allEqNoOrder(genVars, {})
		&& theta1 == Substitution({ x, y }, { x, y })
		&& theta2 == Substitution({ x, y }, { x, y }));
}

TEST_F(LearningAgentTest, PrematchingTests) {
	set<Literal> preconds, add, del;
	Literal actLit;
	set<shared_ptr<ActionRule>> parents;

	// Establishing basic rule with three variables, one being defined existentially
	preconds = {
		clear(x),
		clear(y),
		on(x, z),
		block(x)
	};
	actLit = movePred2(x, y);
	shared_ptr<ActionRule> rule = make_shared<ActionRule>(preconds, actLit, add, del, parents, false);

	// Perfect match x/a, y/b, z/c
	preconds = {
		clear(a),
		clear(b),
		on(a, c),
		block(a)
	};
	actLit = movePred2(a, b);
	shared_ptr<ActionRule> example = make_shared<ActionRule>(preconds, actLit, add, del, parents, false);
	EXPECT_TRUE(allEqNoOrder(toVec(rule->prematchingSubs(example)), {
		Substitution({ x, y, z }, { a, b, c })
		}));
	EXPECT_TRUE(rule->prematches(example));

	// No match (missing block(a))
	preconds = {
		clear(a),
		clear(b),
		on(a, c)
	};
	actLit = movePred2(a, b);
	example = make_shared<ActionRule>(preconds, actLit, add, del, parents, false);
	EXPECT_TRUE(allEqNoOrder(toVec(rule->prematchingSubs(example)), {}));
	EXPECT_FALSE(rule->prematches(example));

	// Match would require non-injectivity x/a y/b z/b
	preconds = {
		clear(a),
		clear(b),
		on(a, b),
		block(a)
	};
	actLit = movePred2(a, b);
	example = make_shared<ActionRule>(preconds, actLit, add, del, parents, false);
	EXPECT_TRUE(allEqNoOrder(toVec(rule->prematchingSubs(example)), {}));
	EXPECT_FALSE(rule->prematches(example));

	// Match but supplementary -useless- facts are true
	preconds = {
		clear(a),
		clear(b),
		on(a, c),
		block(a),
		block(b),
		block(c),
		on(c, f1),
		on(b, f2)
	};
	actLit = movePred2(a, b);
	example = make_shared<ActionRule>(preconds, actLit, add, del, parents, false);
	EXPECT_TRUE(allEqNoOrder(toVec(rule->prematchingSubs(example)), {
		Substitution({ x, y, z }, { a, b, c })
		}));
	EXPECT_TRUE(rule->prematches(example));

	// Multiple matches possible, should return every substitution
	preconds = {
		clear(a),
		clear(b),
		block(a),
		on(a, c),
		on(a, d),
		on(a, e)
	};
	actLit = movePred2(a, b);
	example = make_shared<ActionRule>(preconds, actLit, add, del, parents, false);
	EXPECT_TRUE(allEqNoOrder(toVec(rule->prematchingSubs(example)), {
		Substitution({ x, y, z }, { a, b, c }),
		Substitution({ x, y, z }, { a, b, d }),
		Substitution({ x, y, z }, { a, b, e })
		}));
	EXPECT_FALSE(rule->prematches(example));

	// Wrong action
	preconds = {
		clear(a),
		clear(b),
		on(a, b),
		block(a)
	};
	actLit = on(a, b);
	example = make_shared<ActionRule>(preconds, actLit, add, del, parents, false);
	EXPECT_TRUE(allEqNoOrder(toVec(rule->prematchingSubs(example)), {}));
	EXPECT_FALSE(rule->prematches(example));

	// Wrong action (different arity)
	preconds = {
		clear(a),
		clear(b),
		on(a, b),
		block(a)
	};
	actLit = block(a);
	example = make_shared<ActionRule>(preconds, actLit, add, del, parents, false);
	EXPECT_TRUE(allEqNoOrder(toVec(rule->prematchingSubs(example)), {}));
	EXPECT_FALSE(rule->prematches(example));

	// Wrong constants in action literal (but state is correct, if action literal was okay)
	preconds = {
		clear(b),
		on(b, b),
		block(b)
	};
	actLit = movePred2(b, b);
	example = make_shared<ActionRule>(preconds, actLit, add, del, parents, false);
	EXPECT_TRUE(allEqNoOrder(toVec(rule->prematchingSubs(example)), {}));
	EXPECT_FALSE(rule->prematches(example));

	// Presetting a substitution that matches the rule (and reduces the possibilities of prematching to only one)
	Substitution preSub = Substitution({ z }, { d });
	preconds = {
		clear(a),
		clear(b),
		block(a),
		on(a, c),
		on(a, d),
		on(a, e)
	};
	actLit = movePred2(a, b);
	example = make_shared<ActionRule>(preconds, actLit, add, del, parents, false);
	EXPECT_TRUE(allEqNoOrder(toVec(rule->prematchingSubs(example, preSub)), {
		Substitution({ x, y, z }, { a, b, d })
	}));
	EXPECT_TRUE(rule->prematches(example, preSub));

	// Presetting a substitution that doesn't mach the rule
	preconds = {
		clear(a),
		clear(b),
		on(a, c),
		block(a)
	};
	actLit = movePred2(a, b);
	example = make_shared<ActionRule>(preconds, actLit, add, del, parents, false);
	EXPECT_TRUE(allEqNoOrder(toVec(rule->prematchingSubs(example, preSub)), {}));
	EXPECT_FALSE(rule->prematches(example, preSub));

	// Modifying the rule to have constants inside
	preconds = {
		clear(x),
		clear(b),
		on(x, f1),
		block(x)
	};
	actLit = movePred2(x, b);
	rule = make_shared<ActionRule>(preconds, actLit, add, del, parents, false);

	// Matches constants in rule
	preconds = {
		clear(a),
		clear(b),
		on(a, f1),
		block(a)
	};
	actLit = movePred2(a, b);
	example = make_shared<ActionRule>(preconds, actLit, add, del, parents, false);
	EXPECT_TRUE(allEqNoOrder(toVec(rule->prematchingSubs(example)), {
		Substitution({ x }, { a })
	}));
	EXPECT_TRUE(rule->prematches(example));

	// Doesn't match constants in rule
	preconds = {
		clear(a),
		clear(c),
		on(a, f2),
		block(a)
	};
	actLit = movePred2(a, c);
	example = make_shared<ActionRule>(preconds, actLit, add, del, parents, false);
	EXPECT_TRUE(allEqNoOrder(toVec(rule->prematchingSubs(example)), {}));
	EXPECT_FALSE(rule->prematches(example));

	// Non-working example obtained during a run
	Predicate drop = Predicate("drop", 0);
	Predicate armempty = Predicate("armempty", 0);
	Predicate grabbable = Predicate("grabbable", 1);
	Predicate pushable = Predicate("pushable", 1);
	Predicate ispaint = Predicate("ispaint", 2);
	Predicate hascolor = Predicate("hascolor", 2);
	Predicate holding = Predicate("holding", 1);
	Predicate robotat = Predicate("robotat", 1);
	Predicate light = Predicate("light", 1);
	Predicate lit = Predicate("lit", 1);
	Predicate powered = Predicate("powered", 1);
	Predicate at = Predicate("at", 2);
	Predicate plugged = Predicate("plugged", 2);
	Predicate bigger = Predicate("bigger", 2);
	Predicate onfloor = Predicate("onfloor", 1);
	preconds = { clear(y), grabbable(x), holding(x), robotat(y) };
	add = { armempty(), at(x, y), clear(x), onfloor(x) };
	del = { -holding(x) };
	actLit = drop();
	rule = make_shared<ActionRule>(preconds, actLit, add, del, parents, 0.01f, true);

	Term Rbucket = Instance("RBucket");
	Term box = Instance("box");
	Term crate = Instance("crate");
	Term f = Instance("f");
	Term blue = Instance("blue");
	Term green = Instance("green");
	Term mug = Instance("mug");
	Term red = Instance("red");

	// Perfect match x/a, y/b, z/c
	preconds = {
		 at(e, b), at(f, c), at(Rbucket, b), at(box, a), at(d, c), at(mug, c), bigger(e, mug),
		 bigger(f, mug), bigger(crate, f), bigger(crate, mug), bigger(box, f), bigger(box, mug), bigger(d, f),
		 bigger(d, crate), bigger(d, mug), clear(e), clear(Rbucket), clear(c), clear(box), clear(d), clear(a), clear(mug),
		 grabbable(f), grabbable(crate), grabbable(d), grabbable(mug), hascolor(e, green), hascolor(box, red), holding(crate),
		 ispaint(Rbucket, blue), ispaint(crate, green), ispaint(d, red), light(e), light(d), lit(e), on(mug, f),
		 onfloor(e), onfloor(f), onfloor(Rbucket), onfloor(box), onfloor(d), plugged(e, b), powered(c), powered(a),
		 powered(b), pushable(e), pushable(f), pushable(Rbucket), pushable(box), pushable(d), pushable(mug), robotat(c)
	};
	actLit = drop();
	add = {
		armempty(), at(crate, c), clear(c), onfloor(crate)
	};
	del = {
		-holding(crate)
	};
	example = make_shared<ActionRule>(preconds, actLit, add, del, parents, 0.01f, true);

	EXPECT_TRUE(allEqNoOrder(toVec(rule->prematchingSubs(example)), {
		Substitution({ x, y }, { crate, c })
		}));
	EXPECT_TRUE(rule->prematches(example));
}

TEST_F(LearningAgentTest, PostmatchingTests) {
	set<Literal> preconds, add, del;
	Literal actLit;
	set<shared_ptr<ActionRule>> parents;

	Variable w = Variable("W");

	// Establishing basic rule effects with 4 variables, 2 being existentially defined (and interchangeable, supposedly fixed in preconditions)
	actLit = movePred2(x, y);
	add = {
		on(x, y),
		clear(z),
		clear(w)
	};
	del = {
		-on(x, z),
		-on(x, w),
		-clear(y)
	};
	shared_ptr<ActionRule> rule = make_shared<ActionRule>(preconds, actLit, add, del, parents, false);

	// Perfect match (2 possibilities)
	actLit = movePred2(a, b);
	add = {
		on(a, b),
		clear(d),
		clear(c)
	};
	del = {
		-on(a, d),
		-on(a, c),
		-clear(b)
	};
	shared_ptr<ActionRule> example = make_shared<ActionRule>(preconds, actLit, add, del, parents, false);
	EXPECT_TRUE(allEqNoOrder(toVec(rule->postmatchingSubs(example)), {
		Substitution({ x, y, z, w }, { a, b, c, d }),
		Substitution({ x, y, z, w }, { a, b, d, c })
		}));
	EXPECT_TRUE(rule->postmatches(example));

	// No match (missing add on(a, b))
	actLit = movePred2(a, b);
	add = {
		clear(d),
		clear(c)
	};
	del = {
		-on(a, d),
		-on(a, c),
		-clear(b)
	};
	example = make_shared<ActionRule>(preconds, actLit, add, del, parents, false);
	EXPECT_TRUE(allEqNoOrder(toVec(rule->postmatchingSubs(example)), {}));
	EXPECT_FALSE(rule->postmatches(example));

	// No match (missing del on(a, d))
	actLit = movePred2(a, b);
	add = {
		on(a, b),
		clear(d),
		clear(c)
	};
	del = {
		-on(a, c),
		-clear(b)
	};
	example = make_shared<ActionRule>(preconds, actLit, add, del, parents, false);
	EXPECT_TRUE(allEqNoOrder(toVec(rule->postmatchingSubs(example)), {}));
	EXPECT_FALSE(rule->postmatches(example));

	// Match would require non-injectivity
	actLit = movePred2(a, b);
	add = {
		on(a, b),
		clear(b),
		clear(c)
	};
	del = {
		-on(a, b),
		-on(a, c),
		-clear(b)
	};
	example = make_shared<ActionRule>(preconds, actLit, add, del, parents, false);
	EXPECT_TRUE(allEqNoOrder(toVec(rule->postmatchingSubs(example)), {}));
	EXPECT_FALSE(rule->postmatches(example));
	
	// Doesn't match because of supplementary add fact
	actLit = movePred2(a, b);
	add = {
		on(a, b),
		on(b, c),
		clear(d),
		clear(c)
	};
	del = {
		-on(a, d),
		-on(a, c),
		-clear(b)
	};
	example = make_shared<ActionRule>(preconds, actLit, add, del, parents, false);
	EXPECT_TRUE(allEqNoOrder(toVec(rule->postmatchingSubs(example)), {}));
	EXPECT_FALSE(rule->postmatches(example));

	// Doesn't match because of supplementary del fact
	actLit = movePred2(a, b);
	add = {
		on(a, b),
		clear(d),
		clear(c)
	};
	del = {
		-clear(a),
		-on(a, d),
		-on(a, c),
		-clear(b)
	};
	example = make_shared<ActionRule>(preconds, actLit, add, del, parents, false);
	EXPECT_TRUE(allEqNoOrder(toVec(rule->postmatchingSubs(example)), {}));
	EXPECT_FALSE(rule->postmatches(example));

	// Wrong action
	actLit = on(a, b);
	add = {
		on(a, b),
		clear(d),
		clear(c)
	};
	del = {
		-on(a, d),
		-on(a, c),
		-clear(b)
	};
	example = make_shared<ActionRule>(preconds, actLit, add, del, parents, false);
	EXPECT_TRUE(allEqNoOrder(toVec(rule->postmatchingSubs(example)), {}));
	EXPECT_FALSE(rule->postmatches(example));

	// Wrong action (different arity)
	actLit = block(a);
	add = {
		on(a, b),
		clear(d),
		clear(c)
	};
	del = {
		-on(a, d),
		-on(a, c),
		-clear(b)
	};
	example = make_shared<ActionRule>(preconds, actLit, add, del, parents, false);
	EXPECT_TRUE(allEqNoOrder(toVec(rule->postmatchingSubs(example)), {}));
	EXPECT_FALSE(rule->postmatches(example));

	// Wrong constants in action literal (but effects are correct, if action literal was okay)
	actLit = movePred2(b, b);
	add = {
		on(b, b),
		clear(d),
		clear(c)
	};
	del = {
		-on(b, d),
		-on(b, c),
		-clear(b)
	};
	example = make_shared<ActionRule>(preconds, actLit, add, del, parents, false);
	EXPECT_TRUE(allEqNoOrder(toVec(rule->postmatchingSubs(example)), {}));
	EXPECT_FALSE(rule->postmatches(example));

	// Presetting a substitution that matches the rule (and reduces the possibilities of postmatching to only one)
	Substitution preSub = Substitution({ z }, { d });
	actLit = movePred2(a, b);
	add = {
		on(a, b),
		clear(d),
		clear(c)
	};
	del = {
		-on(a, d),
		-on(a, c),
		-clear(b)
	};
	example = make_shared<ActionRule>(preconds, actLit, add, del, parents, false);
	EXPECT_TRUE(allEqNoOrder(toVec(rule->postmatchingSubs(example, preSub)), {
		Substitution({ x, y, z, w }, { a, b, d, c })
		}));
	EXPECT_TRUE(rule->postmatches(example, preSub));
	
	// Presetting a substitution that doesn't mach the rule
	actLit = movePred2(a, d);
	add = {
		on(a, d),
		clear(b),
		clear(c)
	};
	del = {
		-on(a, b),
		-on(a, c),
		-clear(d)
	};
	example = make_shared<ActionRule>(preconds, actLit, add, del, parents, false);
	EXPECT_TRUE(allEqNoOrder(toVec(rule->postmatchingSubs(example, preSub)), {}));
	EXPECT_FALSE(rule->postmatches(example, preSub));

	// Modifying the rule to have constants inside
	actLit = movePred2(x, b);
	add = {
		on(x, b),
		clear(z),
		clear(f1)
	};
	del = {
		-on(x, z),
		-on(x, f1),
		-clear(b)
	};
	rule = make_shared<ActionRule>(preconds, actLit, add, del, parents, false);

	// Matches constants in rule
	actLit = movePred2(a, b);
	add = {
		on(a, b),
		clear(f1),
		clear(c)
	};
	del = {
		-on(a, f1),
		-on(a, c),
		-clear(b)
	};
	example = make_shared<ActionRule>(preconds, actLit, add, del, parents, false);
	EXPECT_TRUE(allEqNoOrder(toVec(rule->postmatchingSubs(example)), {
		Substitution({ x, z }, { a, c })
		}));
	EXPECT_TRUE(rule->postmatches(example));

	// Doesn't match constants in rule
	actLit = movePred2(a, d);
	add = {
		on(a, d),
		clear(f2),
		clear(c)
	};
	del = {
		-on(a, f2),
		-on(a, c),
		-clear(d)
	};
	example = make_shared<ActionRule>(preconds, actLit, add, del, parents, false);
	EXPECT_TRUE(allEqNoOrder(toVec(rule->postmatchingSubs(example)), {}));
	EXPECT_FALSE(rule->postmatches(example));
}

TEST_F(LearningAgentTest, CoveringTests) {
	set<Literal> preconds, add, del;
	Literal actLit;
	set<shared_ptr<ActionRule>> parents;
	shared_ptr<ActionRule> rule, example;

	Variable w = Variable("W");

	Predicate white = Predicate("white", 1);
	Predicate black = Predicate("white", 1);
	Instance floor = Instance("floor");
	Instance f = Instance("f");
	Instance g = Instance("g");

	// A scenario that didn't work in a previous run
	preconds = { clear(x), clear(y), on(x, z), white(x), white(y) };
	actLit = movePred2(x, y);
	add = { clear(z), on(x, y) };
	del = { -clear(y), -on(x, z) };
	rule = make_shared<ActionRule>(preconds, actLit, add, del, parents, false);

	preconds = { black(c), black(d), black(f), black(g), clear(a), clear(b), clear(c), clear(d), on(a, f), on(b, floor),
				 on(c, e), on(d, floor), on(e, g), on(f, floor), on(g, floor), white(a), white(b), white(e) };
	actLit = movePred2(a, b);
	add = { clear(f), on(a, b) };
	del = { -clear(b), -on(a, f) };
	example = make_shared<ActionRule>(preconds, actLit, add, del, parents, false);

	EXPECT_TRUE(allEqNoOrder(toVec(rule->coveringSubs(example)), { Substitution({ x, y, z }, { a, b, f }) }));
	EXPECT_TRUE(rule->covers(example));

	// Perfect match
	preconds = {
		clear(x),
		clear(y),
		on(x, z)
	};
	actLit = movePred2(x, y);
	add = {
		on(x, y),
		clear(z)
	};
	del = {
		-on(x, z),
		-clear(y)
	};
	rule = make_shared<ActionRule>(preconds, actLit, add, del, parents, false);

	preconds = {
		clear(a),
		clear(b),
		on(a, c)
	};
	actLit = movePred2(a, b);
	add = {
		on(a, b),
		clear(c)
	};
	del = {
		-on(a, c),
		-clear(b)
	};
	example = make_shared<ActionRule>(preconds, actLit, add, del, parents, false);
	EXPECT_TRUE(allEqNoOrder(toVec(rule->coveringSubs(example)), {
		Substitution({ x, y, z }, { a, b, c })
	}));
	EXPECT_TRUE(rule->covers(example));

	// Not prematching
	preconds = {
		clear(a),
		on(a, c)
	};
	actLit = movePred2(a, b);
	add = {
		on(a, b),
		clear(c)
	};
	del = {
		-on(a, c),
		-clear(b)
	};
	example = make_shared<ActionRule>(preconds, actLit, add, del, parents, false);
	EXPECT_TRUE(allEqNoOrder(toVec(rule->coveringSubs(example)), {}));
	EXPECT_FALSE(rule->covers(example));

	// Not postmatching
	preconds = {
		clear(a),
		clear(b),
		on(a, c)
	};
	actLit = movePred2(a, b);
	add = {
		on(a, b),
		clear(c)
	};
	del = {
		-on(a, c),
		-clear(b),
		-clear(c)
	};
	example = make_shared<ActionRule>(preconds, actLit, add, del, parents, false);
	EXPECT_TRUE(allEqNoOrder(toVec(rule->coveringSubs(example)), {}));
	EXPECT_FALSE(rule->covers(example));

	// Several matches in preconds, ruled out by effects
	preconds = {
		clear(x),
		clear(y),
		on(x, w),
		on(x, z)
	};
	actLit = movePred2(x, y);
	add = {
		on(x, y),
		clear(w),
		clear(z)
	};
	del = {
		-on(x, w),
		-on(x, z),
		-clear(y),
		-block(w)
	};
	rule = make_shared<ActionRule>(preconds, actLit, add, del, parents, false);

	preconds = {
		clear(a),
		clear(b),
		on(a, c),
		on(a, d)
	};
	actLit = movePred2(a, b);
	add = {
		on(a, b),
		clear(c),
		clear(d)
	};
	del = {
		-on(a, c),
		-on(a, d),
		-clear(b),
		-block(c)
	};
	example = make_shared<ActionRule>(preconds, actLit, add, del, parents, false);
	EXPECT_TRUE(allEqNoOrder(toVec(rule->coveringSubs(example)), {
		Substitution({ x, y, w, z }, { a, b, c, d })
	}));
	EXPECT_TRUE(rule->covers(example));

	// Several matches in effects, ruled out by preconds
	preconds = {
		clear(x),
		clear(y),
		on(x, w),
		on(x, z),
		block(w)
	};
	actLit = movePred2(x, y);
	add = {
		on(x, y),
		clear(w),
		clear(z)
	};
	del = {
		-on(x, w),
		-on(x, z),
		-clear(y)
	};
	rule = make_shared<ActionRule>(preconds, actLit, add, del, parents, false);

	preconds = {
		clear(a),
		clear(b),
		on(a, c),
		on(a, d),
		block(c)
	};
	actLit = movePred2(a, b);
	add = {
		on(a, b),
		clear(c),
		clear(d)
	};
	del = {
		-on(a, c),
		-on(a, d),
		-clear(b)
	};
	example = make_shared<ActionRule>(preconds, actLit, add, del, parents, false);
	EXPECT_TRUE(allEqNoOrder(toVec(rule->coveringSubs(example)), {
		Substitution({ x, y, w, z }, { a, b, c, d })
	}));
	EXPECT_TRUE(rule->covers(example));

	// Several matches
	preconds = {
		clear(x),
		clear(y),
		on(x, w),
		on(x, z)
	};
	actLit = movePred2(x, y);
	add = {
		on(x, y),
		clear(w),
		clear(z)
	};
	del = {
		-on(x, w),
		-on(x, z),
		-clear(y)
	};
	rule = make_shared<ActionRule>(preconds, actLit, add, del, parents, false);

	preconds = {
		clear(a),
		clear(b),
		on(a, c),
		on(a, d)
	};
	actLit = movePred2(a, b);
	add = {
		on(a, b),
		clear(c),
		clear(d)
	};
	del = {
		-on(a, c),
		-on(a, d),
		-clear(b)
	};
	example = make_shared<ActionRule>(preconds, actLit, add, del, parents, false);
	EXPECT_TRUE(allEqNoOrder(toVec(rule->coveringSubs(example)), {
		Substitution({ x, y, w, z }, { a, b, c, d }),
		Substitution({ x, y, w, z }, { a, b, d, c })
	}));
	EXPECT_TRUE(rule->covers(example));
}

TEST_F(LearningAgentTest, SelectionAlgorithm) {
	// Algorithm is random so we perform each test a bunch of consecutive times
	int TRIALS = 50;

	set<Literal> preconds, add, del;
	Literal actLit;
	set<shared_ptr<ActionRule>> parents;
	shared_ptr<ActionRule> rule, example;

	Instance f = Instance("f");

	// Creating a rule and an example
	preconds = {
		on(a, c),
		on(b, d),
		movePred2(b, x)
	};
	actLit = movePred2(a, b);
	add = {
		on(a, b)
	};
	del = {
		-movePred2(b, x)
	};
	rule = make_shared<ActionRule>(preconds, actLit, add, del, parents, false);

	preconds = {
		on(a, e),
		on(b, f),
		movePred2(b, c)
	};
	actLit = movePred2(a, b);
	add = {
		on(a, b)
	};
	del = {
		-movePred2(b, c)
	};
	example = make_shared<ActionRule>(preconds, actLit, add, del, parents, false);
	
	// Performing random tests
	for (int i = 0; i < TRIALS; i++) {
		// Setting up the beginning of generalization algorithm by hand
		set<Term> genVars;
		set<Literal> genLits;
		Literal chosenLr, chosenLx;

		Substitution subr = Substitution({ }, { });
		Substitution subx = Substitution({ x }, { c });
		EXPECT_TRUE(rule->postGeneralizes(example, subr, subx, genVars));

		set<Literal> lr = subr.inverse().apply(rule->preconditions);
		set<Literal> lx = subx.inverse().apply(example->preconditions);

		// If we perform the Selection algorithm three times in a row,
		// it should have generalized the preconditions completely (regardless of the random order)
		EXPECT_TRUE(rule->selection(lr, lx, example, subr, subx, genVars, genLits, chosenLr, chosenLx));
		lr.erase(chosenLr);
		lx.erase(chosenLx);
		EXPECT_TRUE(rule->selection(lr, lx, example, subr, subx, genVars, genLits, chosenLr, chosenLx));
		lr.erase(chosenLr);
		lx.erase(chosenLx);
		EXPECT_TRUE(rule->selection(lr, lx, example, subr, subx, genVars, genLits, chosenLr, chosenLx));
		lr.erase(chosenLr);
		lx.erase(chosenLx);
		
		EXPECT_TRUE(lr.size() == 0);
		EXPECT_TRUE(lx.size() == 0);
		EXPECT_TRUE(subr.getInverse(c).obj == subx.getInverse(e).obj && subr.getInverse(c).there);
		EXPECT_TRUE(subr.getInverse(d).obj == subx.getInverse(f).obj && subr.getInverse(d).there);
		EXPECT_TRUE(subx.get(x).obj == c);

		auto mappingR = subr.getMapping();
		foreach(pair, mappingR) {
			if (subx.get(pair->first).obj == pair->second) {
				genLits = Substitution({ pair->first }, { pair->second }).apply(genLits);
				subr.remove(pair->first);
				subx.remove(pair->first);
			}
			if (pair->first == pair->second)
				subr.remove(pair->first);
		}

		EXPECT_TRUE(allEqNoOrder(toVec(genLits), {
			on({ a, Variable::anyVar() }),
			on({ b, Variable::anyVar() }),
			movePred2(b, x)
		}));

		// Performing the exact same test but with automatic algorithm "anyGeneralization"
		subr = Substitution({ }, { });
		subx = Substitution({ x }, { c });
		genVars = set<Term>();
		EXPECT_TRUE(rule->postGeneralizes(example, subr, subx, genVars));
		genLits = rule->anyGeneralization(example, subr, subx, genVars);

		EXPECT_TRUE(subr.getInverse(c).obj == subx.getInverse(e).obj && subr.getInverse(c).there);
		EXPECT_TRUE(subr.getInverse(d).obj == subx.getInverse(f).obj && subr.getInverse(d).there);
		EXPECT_TRUE(subx.get(x).obj == c);

		mappingR = subr.getMapping();
		foreach(pair, mappingR) {
			if (subx.get(pair->first).obj == pair->second) {
				genLits = Substitution({ pair->first }, { pair->second }).apply(genLits);
				subr.remove(pair->first);
				subx.remove(pair->first);
			}
			if (pair->first == pair->second)
				subr.remove(pair->first);
		}

		EXPECT_TRUE(allEqNoOrder(toVec(genLits), {
			on({ a, Variable::anyVar() }),
			on({ b, Variable::anyVar() }),
			movePred2(b, x)
		}));
	}

	// Case where literals should be deleted
	preconds = {
		on(a, c),
		on(b, d),
		on(c, d),
		movePred2(b, x)
	};
	actLit = movePred2(a, b);
	add = {
		on(a, b)
	};
	del = {
		-movePred2(b, x)
	};
	rule = make_shared<ActionRule>(preconds, actLit, add, del, parents, false);

	preconds = {
		on(a, e),
		on(b, f),
		block(b),
		movePred2(b, c)
	};
	actLit = movePred2(a, b);
	add = {
		on(a, b)
	};
	del = {
		-movePred2(b, c)
	};
	example = make_shared<ActionRule>(preconds, actLit, add, del, parents, false);

	// Performing random tests
	for (int i = 0; i < TRIALS; i++) {
		Substitution subr = Substitution({ }, { });
		Substitution subx = Substitution({ x }, { c });
		set<Term> genVars;
		set<Literal> genLits;

		EXPECT_TRUE(rule->postGeneralizes(example, subr, subx, genVars));
		genLits = rule->anyGeneralization(example, subr, subx, genVars);

		auto mappingR = subr.getMapping();
		foreach(pair, mappingR) {
			if (subx.get(pair->first).obj == pair->second) {
				genLits = Substitution({ pair->first }, { pair->second }).apply(genLits);
				subr.remove(pair->first);
				subx.remove(pair->first);
			}
			if (pair->first == pair->second)
				subr.remove(pair->first);
		}

		EXPECT_TRUE(subr.getInverse(c).obj == subx.getInverse(e).obj && subr.getInverse(c).there);
		EXPECT_TRUE(subr.getInverse(d).obj == subx.getInverse(f).obj && subr.getInverse(d).there);
		EXPECT_TRUE(subx.get(x).obj == c);

		EXPECT_TRUE(allEqNoOrder(toVec(genLits), {
			on({ a, Variable::anyVar() }),
			on({ b, Variable::anyVar() }),
			movePred2(b, x)
		}));
	}

	// Case where generalization is empty
	preconds = {
		on(a, c),
		on(b, d),
		movePred2(b, x)
	};
	actLit = movePred2(a, b);
	add = {
		on(a, b)
	};
	del = {
		-movePred2(b, x)
	};
	rule = make_shared<ActionRule>(preconds, actLit, add, del, parents, false);

	preconds = {
		block(b),
		movePred2(d, b)
	};
	actLit = movePred2(a, b);
	add = {
		on(a, b)
	};
	del = {
		-movePred2(b, c)
	};
	example = make_shared<ActionRule>(preconds, actLit, add, del, parents, false);

	// Performing random tests
	for (int i = 0; i < TRIALS; i++) {
		Substitution subr = Substitution({ }, { });
		Substitution subx = Substitution({ x }, { c });
		set<Term> genVars;
		set<Literal> genLits;

		EXPECT_TRUE(rule->postGeneralizes(example, subr, subx, genVars));
		genLits = rule->anyGeneralization(example, subr, subx, genVars);

		auto mappingR = subr.getMapping();
		foreach(pair, mappingR) {
			if (subx.get(pair->first).obj == pair->second) {
				genLits = Substitution({ pair->first }, { pair->second }).apply(genLits);
				subr.remove(pair->first);
				subx.remove(pair->first);
			}
			if (pair->first == pair->second)
				subr.remove(pair->first);
		}

		EXPECT_TRUE(subr == Substitution({ }, { }));
		EXPECT_TRUE(subx == Substitution({ x }, { c }));

		EXPECT_TRUE(genLits.size() == 0);
	}
}

TEST_F(LearningAgentTest, PostGeneralizationAlgorithm) {
	set<Literal> preconds, add, del;
	Literal actLit;
	set<shared_ptr<ActionRule>> parents;
	shared_ptr<ActionRule> rule, example;

	// Correct post-generalization, without post-matching
	actLit = movePred2(x, c);
	add = {
		on(x, c)
	};
	del = {
		-on(x, f1),
		-clear(c)
	};
	rule = make_shared<ActionRule>(preconds, actLit, add, del, parents, false);

	actLit = movePred2(a, b);
	add = {
		on(a, b)
	};
	del = {
		-on(a, f1),
		-clear(b)
	};
	example = make_shared<ActionRule>(preconds, actLit, add, del, parents, false);

	Substitution subr, subx;
	set<Term> genVars;
	EXPECT_FALSE(rule->postmatches(example));
	EXPECT_TRUE(rule->postGeneralizes(example, subr, subx, genVars));

	auto mappingR = subr.getMapping();
	foreach(pair, mappingR) {
		if (subx.get(pair->first).obj == pair->second) {
			subr.remove(pair->first);
			subx.remove(pair->first);
		}
		if (pair->first == pair->second)
			subr.remove(pair->first);
	}

	EXPECT_TRUE(subx.get(x).obj == a);
	EXPECT_TRUE(subx.getInverse(b).obj == Variable::anyVar());
	EXPECT_TRUE(subx.getMapping().size() == 2);

	EXPECT_TRUE(subr.getInverse(c).obj == Variable::anyVar());
	EXPECT_TRUE(subr.getMapping().size() == 1);

	// No post-generalization (easy reason, because of effect numbers are not the same)
	actLit = movePred2(x, c);
	add = {
		on(x, c)
	};
	del = {
		-on(x, f1),
		-clear(c)
	};
	rule = make_shared<ActionRule>(preconds, actLit, add, del, parents, false);

	actLit = movePred2(a, b);
	add = {
		on(a, b),
		-clear(b)
	};
	del = {
		-on(a, f1)
	};
	example = make_shared<ActionRule>(preconds, actLit, add, del, parents, false);

	subr = Substitution();
	subx = Substitution();
	EXPECT_FALSE(rule->postmatches(example));
	EXPECT_FALSE(rule->postGeneralizes(example, subr, subx, genVars));

	// No post-generalization (tricky reason, because of non-injectivity)
	actLit = movePred2(x, c);
	add = {
		on(x, c),
		on(c, d)
	};
	del = {
		-on(x, f1),
		-clear(c)
	};
	rule = make_shared<ActionRule>(preconds, actLit, add, del, parents, false);

	actLit = movePred2(a, b);
	add = {
		on(a, b),
		on(b, a)
	};
	del = {
		-on(a, f1),
		-clear(b)
	};
	example = make_shared<ActionRule>(preconds, actLit, add, del, parents, false);

	subr = Substitution();
	subx = Substitution();
	EXPECT_FALSE(rule->postmatches(example));
	EXPECT_FALSE(rule->postGeneralizes(example, subr, subx, genVars));

	// Wrong action literal
	add = {
	};
	del = {
	};
	actLit = movePred2(x, c);
	rule = make_shared<ActionRule>(preconds, actLit, add, del, parents, false);
	
	actLit = clear(a);
	example = make_shared<ActionRule>(preconds, actLit, add, del, parents, false);

	subr = Substitution();
	subx = Substitution();
	EXPECT_FALSE(rule->postmatches(example));
	EXPECT_FALSE(rule->postGeneralizes(example, subr, subx, genVars));

	// Correct post-generalization, with post-matching too
	actLit = movePred2(x, c);
	add = {
		on(x, c)
	};
	del = {
		-on(x, f1),
		-clear(c)
	};
	rule = make_shared<ActionRule>(preconds, actLit, add, del, parents, false);

	actLit = movePred2(a, c);
	add = {
		on(a, c)
	};
	del = {
		-on(a, f1),
		-clear(c)
	};
	example = make_shared<ActionRule>(preconds, actLit, add, del, parents, false);

	subr = Substitution();
	subx = Substitution();
	EXPECT_TRUE(rule->postmatches(example));
	EXPECT_TRUE(rule->postGeneralizes(example, subr, subx, genVars));

	mappingR = subr.getMapping();
	foreach(pair, mappingR) {
		if (subx.get(pair->first).obj == pair->second) {
			subr.remove(pair->first);
			subx.remove(pair->first);
		}
		if (pair->first == pair->second)
			subr.remove(pair->first);
	}

	EXPECT_TRUE(subx.get(x).obj == a);
	EXPECT_TRUE(subx.getMapping().size() == 1);
	EXPECT_TRUE(subr.getMapping().size() == 0);

	// Correct post-generalization but two ways are possible (performing several times to make sure both ways are correct)
	actLit = movePred2(x, c);
	add = {
		on(x, c)
	};
	del = {
		-on(x, f1),
		-on(x, f2),
		-clear(c)
	};
	rule = make_shared<ActionRule>(preconds, actLit, add, del, parents, false);

	actLit = movePred2(a, b);
	add = {
		on(a, b)
	};
	del = {
		-on(a, d),
		-on(a, e),
		-clear(b)
	};
	example = make_shared<ActionRule>(preconds, actLit, add, del, parents, false);
	EXPECT_FALSE(rule->postmatches(example));

	for (int i = 0; i < 50; i++) {
		subr = Substitution();
		subx = Substitution();

		EXPECT_TRUE(rule->postGeneralizes(example, subr, subx, genVars));

		mappingR = subr.getMapping();
		foreach(pair, mappingR) {
			if (subx.get(pair->first).obj == pair->second) {
				subr.remove(pair->first);
				subx.remove(pair->first);
			}
			if (pair->first == pair->second)
				subr.remove(pair->first);
		}

		EXPECT_TRUE(subx.get(x).obj == a);
		EXPECT_TRUE(subx.getInverse(b).obj == Variable::anyVar());
		EXPECT_TRUE(subx.getInverse(d).obj == Variable::anyVar());
		EXPECT_TRUE(subx.getInverse(e).obj == Variable::anyVar());
		EXPECT_TRUE(subx.getMapping().size() == 4);

		EXPECT_TRUE(subr.getInverse(c).obj == Variable::anyVar());
		EXPECT_TRUE(subr.getInverse(f1).obj == Variable::anyVar());
		EXPECT_TRUE(subr.getInverse(f2).obj == Variable::anyVar());
		EXPECT_TRUE(subr.getMapping().size() == 3);
	}
}

TEST_F(LearningAgentTest, WellFormation) {
	set<Literal> preconds;
	set<Literal> add;
	set<Literal> del;
	set<shared_ptr<ActionRule>> nullParents;

	// Constructing Rule 1
	preconds = {
		on(x, z),
		clear(y)
	};
	add = {
		on(x, y)
	};
	del = {
		-on(x, z)
	};
	shared_ptr<ActionRule> rule1 = make_shared<ActionRule>(preconds, movePred2(x, y), add, del, nullParents, 0.01f, false);
	EXPECT_TRUE(rule1->wellFormed());

	// Constructing Rule 2
	preconds = {
		block(z),
	};
	add = {
		on(x, y),
		on(x, z)
	};
	del = {
		-block(z)
	};
	shared_ptr<ActionRule> rule2 = make_shared<ActionRule>(preconds, movePred2(x, y), add, del, nullParents, 0.01f, false);
	EXPECT_FALSE(rule2->wellFormed()); // Variables x and y are not referenced in preconditions

	// Constructing Rule 3
	preconds = {
		block(x),
		block(y),
		block(z)
	};
	add = {
		on(x, y),
		on(y, z)
	};
	del = {
		-block(z)
	};
	shared_ptr<ActionRule> rule3 = make_shared<ActionRule>(preconds, movePred2(x, y), add, del, nullParents, 0.01f, false);
	//EXPECT_FALSE(rule3->wellFormed()); // Variable z is not connected to x nor y in preconditions (but in effects, which shouldn't be enough)

	// Constructing Rule 4
	preconds = {
		block(x),
		block(y)
	};
	add = {
		on(x, y),
		on(x, z)
	};
	del = {
		-block(z)
	};
	shared_ptr<ActionRule> rule4 = make_shared<ActionRule>(preconds, movePred2(x, y), add, del, nullParents, 0.01f, false);
	EXPECT_FALSE(rule4->wellFormed()); // block(z) is deleted but was not in preconditions

	// Constructing Rule 5
	preconds = {
		block(x),
		block(y),
		block(z)
	};
	add = {
		on(x, y),
		on(x, z),
		block(z)
	};
	del = {
	};
	shared_ptr<ActionRule> rule5 = make_shared<ActionRule>(preconds, movePred2(x, y), add, del, nullParents, 0.01f, false);
	EXPECT_FALSE(rule5->wellFormed()); // block(z) is added but was already in preconditions
}

TEST_F(LearningAgentTest, VariablesTests) {
	vector<Literal> preconds;
	vector<Literal> add;
	vector<Literal> del;
	set<shared_ptr<ActionRule>> nullParents;

	// Constructing rule with several constants
	preconds = {
		on(a, c),
		clear(a),
		clear(b),
		on(c, f1),
		on(b, f2)
	};
	add = {
		on(a, b),
		clear(c)
	};
	del = {
		-clear(b),
		-on(a, c)
	};
	shared_ptr<ActionRule> rule1 = make_shared<ActionRule>(toSet(preconds), movePred2(a, b), toSet(add), toSet(del), nullParents, 0.01f);
	EXPECT_TRUE(rule1->wellFormed());
	
	// Specificity should be: 5 preconditions + 8 constant occurences = 13
	EXPECT_TRUE(rule1->specificity() == 13);

	// Transforming two constants (a and f1) into variables
	Variable aVar = rule1->makeNewVar(set<Term>{}, a);
	Variable f1Var = rule1->makeNewVar(set<Term>{ aVar }, f1);
	Substitution genSub = Substitution({ a, f1 }, { aVar, f1Var });

	foreachindex(i, preconds)
		preconds[i] = genSub.apply(preconds[i]);
	foreachindex(i, add)
		add[i] = genSub.apply(add[i]);
	foreachindex(i, del)
		del[i] = genSub.apply(del[i]);

	shared_ptr<ActionRule> rule2 = make_shared<ActionRule>(toSet(preconds), movePred2(aVar, b), toSet(add), toSet(del), set<shared_ptr<ActionRule>>{ rule1 }, 0.01f);
	EXPECT_TRUE(rule2->wellFormed());
	EXPECT_TRUE(allEqNoOrder(rule2->parameters, { b, c, f2, Variable("_V1"), Variable("_V2") }));

	// Specificity should be: 5 preconditions + 5 constant occurences = 10
	EXPECT_TRUE(rule2->specificity() == 10);

	// Replacing all remaining constants by variables
	shared_ptr<ActionRule> rule3 = rule2->makeUseOfVariables();
	EXPECT_TRUE(rule3->wellFormed());
	EXPECT_TRUE(allEqNoOrder(rule3->parameters, { Variable("_V1"), Variable("_V2"), Variable("_V3"), Variable("_V4"), Variable("_V5") }));

	// Specificity should be: 5 preconditions + 0 variables = 5
	EXPECT_TRUE(rule3->specificity() == 5);
}

TEST_F(LearningAgentTest, ProbabilitiesTests) {

	// Containers
	set<Literal> preconds;
	set<Literal> add;
	set<Literal> del;
	set<shared_ptr<ActionRule>> nullParents;
	float startPu = 0.01f;

	// 1. Creation of a rule with different startPu: check initialization of probabilies over normal preconds, constants and preconds in del effects
	startPu = 0.5f;
	float expectedProb = 1.0f - powf(startPu, 1.0f / 6.0f);
	preconds = {
		on(x, z),
		clear(y),
		clear(z),
		block(a),
		block(c)
	};
	add = {
		on(x, y),
		block(z)
	};
	del = {
		-on(x, z)
	};
	shared_ptr<ActionRule> rule = make_shared<ActionRule>(preconds, movePred2(x, y), add, del, nullParents, startPu, false);
	EXPECT_TRUE(rule->precondsNecessities[on(x, z)] == 1.0f);
	EXPECT_TRUE(rule->precondsNecessities[clear(y)] == expectedProb);
	EXPECT_TRUE(rule->precondsNecessities[clear(z)] == expectedProb);
	EXPECT_TRUE(rule->precondsNecessities[block(a)] == expectedProb);
	EXPECT_TRUE(rule->precondsNecessities[block(c)] == expectedProb);
	EXPECT_TRUE(rule->constsNecessities[a] == expectedProb);
	EXPECT_TRUE(rule->constsNecessities[c] == expectedProb);

	expectedProb = 1.0f - powf(startPu, 1.0f / 6.0f);
	preconds = {
		on(a, c),
		clear(b),
		block(a),
		block(c)
	};
	add = {
		on(a, b),
		block(c)
	};
	del = {
		-on(a, c)
	};
	State nextState = State(preconds);
	nextState.addFacts(add);
	nextState.removeFacts(del);
	shared_ptr<ActionRule> rule2 = make_shared<ActionRule>(Trace(preconds, movePred2(a, b), true, nextState), startPu, false);
	EXPECT_TRUE(rule2->precondsNecessities[on(a, c)] == 1.0f);
	EXPECT_TRUE(rule2->precondsNecessities[clear(b)] == expectedProb);
	EXPECT_TRUE(rule2->precondsNecessities[block(a)] == expectedProb);
	EXPECT_TRUE(rule2->precondsNecessities[block(c)] == expectedProb);
	EXPECT_TRUE(rule2->constsNecessities[a] == expectedProb);
	EXPECT_TRUE(rule2->constsNecessities[b] == expectedProb);
	EXPECT_TRUE(rule2->constsNecessities[c] == expectedProb);

	// 2. Check fulfilment probabilities over simple cases
	expectedProb = 1.0f - powf(startPu, 1.0f / 6.0f);

	// 2.a. Case with absolutely nothing in common
	State state = State();
	bool prematches;
	set<Substitution> subs;
	EXPECT_TRUE(rule->fulfilmentProbability(state, movePred2(a, b), instances, prematches, subs) == 0.0f);
	EXPECT_FALSE(prematches);
	/*EXPECT_TRUE(rule->cacheState == state);
	EXPECT_TRUE(rule->cacheProbs.size() == 0);*/

	map<Literal, float> updatedPrecondNecs = rule->precondsNecessities;
	map<Term, float> updatedConstNecs = rule->constsNecessities;
	vector<Substitution> notCovSubs = {
		Substitution({ x, y }, { a, b }, false)
	};
	/*EXPECT_TRUE(rule->fulfilmentAndUpdatedNecsNotCov(updatedPrecondNecs, updatedConstNecs, state, notCovSubs) == 0.0f);
	foreach(pair, updatedPrecondNecs)
		EXPECT_TRUE(rule->precondsNecessities[pair->first] == pair->second);
	foreach(pair, updatedConstNecs)
		EXPECT_TRUE(rule->constsNecessities[pair->first] == pair->second);*/

	// 2.b. Case with only one sub prematching
	state = State({
		on(b, e),	// Substitutions like "X/c, Y/d, Z/f1" have a probability of 0 because on(x, z) is in delete effects!
		clear(e),
		clear(d),
		block(a),
		block(c)
	});
	subs = {};
	EXPECT_TRUE(rule->fulfilmentProbability(state, movePred2(b, d), instances, prematches, subs) == 1.0f);
	EXPECT_TRUE(prematches);
	/*EXPECT_TRUE(rule->cacheState == state);
	EXPECT_TRUE(rule->cacheProbs.size() == 1);
	EXPECT_TRUE(rule->cacheProbs.begin()->first == Substitution({ x, y, z }, { b, d, e }, false));*/

	updatedPrecondNecs = rule->precondsNecessities;
	updatedConstNecs = rule->constsNecessities;
	notCovSubs = {
		Substitution({ x, y, z }, { b, d, e }, false)
	};
	/*EXPECT_TRUE(rule->fulfilmentAndUpdatedNecsNotCov(updatedPrecondNecs, updatedConstNecs, state, notCovSubs) == 1.0f);
	foreach(pair, updatedPrecondNecs)
		EXPECT_TRUE(rule->precondsNecessities[pair->first] == pair->second);
	foreach(pair, updatedConstNecs)
		EXPECT_TRUE(rule->constsNecessities[pair->first] == pair->second);*/

	// 2.c. Case with absolute prematching (all subs) - so prematching is false
	state = State({
		on(b, e),
		on(b, f1),
		on(b, f2),
		on(b, f3),
		clear(e),
		clear(f1),
		clear(f2),
		clear(f3),
		clear(d),
		block(a),
		block(c)
	});
	subs = {};
	EXPECT_TRUE(rule->fulfilmentProbability(state, movePred2(b, d), instances, prematches, subs) == 1.0f);
	//EXPECT_FALSE(prematches);
	/*EXPECT_TRUE(rule->cacheState == state);
	EXPECT_TRUE(rule->cacheProbs.size() == 4);
	EXPECT_TRUE(in(rule->cacheProbs, Substitution({ x, y, z }, { b, d, e }, false)));
	EXPECT_TRUE(in(rule->cacheProbs, Substitution({ x, y, z }, { b, d, f1 }, false)));
	EXPECT_TRUE(in(rule->cacheProbs, Substitution({ x, y, z }, { b, d, f2 }, false)));
	EXPECT_TRUE(in(rule->cacheProbs, Substitution({ x, y, z }, { b, d, f3 }, false)));*/

	updatedPrecondNecs = rule->precondsNecessities;
	updatedConstNecs = rule->constsNecessities;
	notCovSubs = {
		Substitution({ x, y, z }, { b, d, e }, false),
		Substitution({ x, y, z }, { b, d, f1 }, false),
		Substitution({ x, y, z }, { b, d, f2 }, false)
	};
	/*EXPECT_TRUE(rule->fulfilmentAndUpdatedNecsNotCov(updatedPrecondNecs, updatedConstNecs, state, notCovSubs) == 1.0f);
	foreach(pair, updatedPrecondNecs)
		EXPECT_TRUE(rule->precondsNecessities[pair->first] == pair->second);
	foreach(pair, updatedConstNecs)
		EXPECT_TRUE(rule->constsNecessities[pair->first] == pair->second);*/

	// 2.d. Case with only one sub prematching except for one normal precondition
	state = State({
		on(b, e),	// Substitutions like "X/c, Y/d, Z/f1" have a probability of 0 because on(x, z) is in delete effects!
		clear(e),
		block(a),
		block(c)
	});
	subs = {};
	EXPECT_TRUE(rule->fulfilmentProbability(state, movePred2(b, d), instances, prematches, subs) == 1.0f - expectedProb);
	EXPECT_FALSE(prematches);
	/*EXPECT_TRUE(rule->cacheState == state);
	EXPECT_TRUE(rule->cacheProbs.size() == 1);
	EXPECT_TRUE(rule->cacheProbs.begin()->first == Substitution({ x, y, z }, { b, d, e }, false));*/

	updatedPrecondNecs = rule->precondsNecessities;
	updatedConstNecs = rule->constsNecessities;
	notCovSubs = {
		Substitution({ x, y, z }, { b, d, e }, false)
	};
	/*EXPECT_TRUE(rule->fulfilmentAndUpdatedNecsNotCov(updatedPrecondNecs, updatedConstNecs, state, notCovSubs) == 1.0f - expectedProb);
	EXPECT_TRUE(updatedPrecondNecs[on(x, z)] == 1.0f);
	EXPECT_TRUE(updatedPrecondNecs[clear(y)] == 1.0f);
	EXPECT_TRUE(updatedPrecondNecs[clear(z)] == expectedProb);
	EXPECT_TRUE(updatedPrecondNecs[block(a)] == expectedProb);
	EXPECT_TRUE(updatedPrecondNecs[block(c)] == expectedProb);
	EXPECT_TRUE(updatedConstNecs[a] == expectedProb);
	EXPECT_TRUE(updatedConstNecs[c] == expectedProb);*/

	// 2.e. Case with only one sub prematching except for one del effect precondition
	state = State({
		clear(d),
		clear(e),
		block(a),
		block(c)
	});
	subs = {};
	EXPECT_TRUE(rule->fulfilmentProbability(state, movePred2(b, d), instances, prematches, subs) == 0.0f);
	EXPECT_FALSE(prematches);
	/*EXPECT_TRUE(rule->cacheState == state);
	EXPECT_TRUE(rule->cacheProbs.size() == 0);*/

	updatedPrecondNecs = rule->precondsNecessities;
	updatedConstNecs = rule->constsNecessities;
	notCovSubs = {
		Substitution({ x, y, z }, { b, d, e }, false)
	};
	/*EXPECT_TRUE(rule->fulfilmentAndUpdatedNecsNotCov(updatedPrecondNecs, updatedConstNecs, state, notCovSubs) == 0.0f);
	EXPECT_TRUE(updatedPrecondNecs[on(x, z)] == 1.0f);
	EXPECT_TRUE(updatedPrecondNecs[clear(y)] == expectedProb);
	EXPECT_TRUE(updatedPrecondNecs[clear(z)] == expectedProb);
	EXPECT_TRUE(updatedPrecondNecs[block(a)] == expectedProb);
	EXPECT_TRUE(updatedPrecondNecs[block(c)] == expectedProb);
	EXPECT_TRUE(updatedConstNecs[a] == expectedProb);
	EXPECT_TRUE(updatedConstNecs[c] == expectedProb);*/

	// 2.f. Case where a variable assignement makes a constant in the rule impossible (breaking injectivity)
	state = State({
		on(c, e),	// if X is c, then c cannot be a constant in rule and all linked preconds are false
		clear(e),
		clear(d),
		block(a),
		block(c)
	});
	float pSub = 1 - expectedProb;
	subs = {};
	EXPECT_TRUE(rule->fulfilmentProbability(state, movePred2(c, d), instances, prematches, subs) == pSub);
	EXPECT_FALSE(prematches);
	/*EXPECT_TRUE(rule->cacheState == state);
	EXPECT_TRUE(rule->cacheProbs.size() == 1);
	EXPECT_TRUE(in(rule->cacheProbs, Substitution({ x, y, z }, { c, d, e }, false)));*/

	updatedPrecondNecs = rule->precondsNecessities;
	updatedConstNecs = rule->constsNecessities;
	notCovSubs = {
		Substitution({ x, y, z }, { c, d, e }, false)
	};
	/*EXPECT_TRUE(rule->fulfilmentAndUpdatedNecsNotCov(updatedPrecondNecs, updatedConstNecs, state, notCovSubs) == pSub);
	EXPECT_TRUE(updatedPrecondNecs[on(x, z)] == 1.0f);
	EXPECT_TRUE(updatedPrecondNecs[clear(y)] == expectedProb);
	EXPECT_TRUE(updatedPrecondNecs[clear(z)] == expectedProb);
	EXPECT_TRUE(updatedPrecondNecs[block(a)] == expectedProb);
	EXPECT_TRUE(updatedPrecondNecs[block(c)] == expectedProb / (1.0f - pSub));
	EXPECT_TRUE(updatedConstNecs[a] == expectedProb);
	EXPECT_TRUE(updatedConstNecs[c] == expectedProb / (1.0f - pSub));*/

	// 2.g. Case of prematching but without verifying inherited removed preconditions
	set<Literal> removedPreconds = {
		on(z, y),
		block(z),
		clear(a)
	};
	rule->removedPreconditions = removedPreconds;
	foreach(rem, removedPreconds)
		rule->precondsNecessities[*rem] = 0.05f;

	state = State({
		on(b, e),
		clear(e),
		clear(d),
		block(a),
		block(c),
		block(e)	// Missing removed preconditions: on(z, y), clear(a)
	});
	pSub = 1.0f - (0.05f * 2.0f - 0.05f * 0.05f);
	subs = {};
	EXPECT_TRUE(abs(rule->fulfilmentProbability(state, movePred2(b, d), instances, prematches, subs) - pSub) < 0.0001f);
	EXPECT_TRUE(prematches);
	/*EXPECT_TRUE(rule->cacheState == state);
	EXPECT_TRUE(rule->cacheProbs.size() == 1);
	EXPECT_TRUE(rule->cacheProbs.begin()->first == Substitution({ x, y, z }, { b, d, e }, false));*/

	updatedPrecondNecs = rule->precondsNecessities;
	updatedConstNecs = rule->constsNecessities;
	notCovSubs = {
		Substitution({ x, y, z }, { b, d, e }, false)
	};
	/*EXPECT_TRUE(rule->fulfilmentAndUpdatedNecsNotCov(updatedPrecondNecs, updatedConstNecs, state, notCovSubs) == pSub);
	EXPECT_TRUE(updatedPrecondNecs[on(x, z)] == 1.0f);
	EXPECT_TRUE(updatedPrecondNecs[clear(y)] == expectedProb);
	EXPECT_TRUE(updatedPrecondNecs[clear(z)] == expectedProb);
	EXPECT_TRUE(updatedPrecondNecs[block(a)] == expectedProb);
	EXPECT_TRUE(updatedPrecondNecs[block(c)] == expectedProb);
	EXPECT_TRUE(updatedConstNecs[a] == expectedProb);
	EXPECT_TRUE(updatedConstNecs[c] == expectedProb);

	EXPECT_TRUE(updatedPrecondNecs[on(z, y)] == 0.05f / (1.0f - pSub));
	EXPECT_TRUE(updatedPrecondNecs[block(z)] == 0.05f);
	EXPECT_TRUE(updatedPrecondNecs[clear(a)] == 0.05f / (1.0f - pSub));*/

	// 2.h. Case of prematching with under-PRECISION level probabilities (optional)
	foreach(rem, removedPreconds)
		rule->precondsNecessities[*rem] = 0.99999f;

	state = State({
		on(b, e),
		clear(e),
		clear(d),
		block(a),
		block(c),
		block(e)	// Missing removed preconditions: on(z, y), clear(a), but probabilies are very high
	});
	subs = {};
	EXPECT_TRUE(rule->fulfilmentProbability(state, movePred2(b, d), instances, prematches, subs) == 0.0f);
	EXPECT_TRUE(prematches);
	/*EXPECT_TRUE(rule->cacheState == state);
	EXPECT_TRUE(rule->cacheProbs.size() == 0);*/

	updatedPrecondNecs = rule->precondsNecessities;
	updatedConstNecs = rule->constsNecessities;
	notCovSubs = {
		Substitution({ x, y, z }, { b, d, e }, false)
	};
	// This second algorithm doesn't have the precision limit as it is linear in time
	pSub = powf(1.0f - 0.99999f, 2.0f);
	/*EXPECT_TRUE(rule->fulfilmentAndUpdatedNecsNotCov(updatedPrecondNecs, updatedConstNecs, state, notCovSubs) == pSub);
	EXPECT_TRUE(updatedPrecondNecs[on(x, z)] == 1.0f);
	EXPECT_TRUE(updatedPrecondNecs[clear(y)] == expectedProb);
	EXPECT_TRUE(updatedPrecondNecs[clear(z)] == expectedProb);
	EXPECT_TRUE(updatedPrecondNecs[block(a)] == expectedProb);
	EXPECT_TRUE(updatedPrecondNecs[block(c)] == expectedProb);
	EXPECT_TRUE(updatedConstNecs[a] == expectedProb);
	EXPECT_TRUE(updatedConstNecs[c] == expectedProb);

	EXPECT_TRUE(updatedPrecondNecs[on(z, y)] == 0.99999f / (1.0f - pSub));
	EXPECT_TRUE(updatedPrecondNecs[block(z)] == 0.99999f);
	EXPECT_TRUE(updatedPrecondNecs[clear(a)] == 0.99999f / (1.0f - pSub));*/

	rule->removedPreconditions = {};

	// 2.i. Case of two substitutions with non-negligeable probabilities (therefore conditionals are computed)
	expectedProb = 1.0f - powf(startPu, 1.0f / 7.0f);
	preconds = {
		on(x, z),
		clear(y),
		clear(z),
		on(z, y),
		block(a),
		block(c)
	};
	add = {
		on(x, y),
		block(z)
	};
	del = {
		-on(x, z)
	};
	rule = make_shared<ActionRule>(preconds, movePred2(x, y), add, del, nullParents, startPu, false);

	state = State({
		on(b, e),
		on(b, f1),
		on(e, d),		// Because this precond is here, sub { X/b, Y/d, Z/e } is prefered to sub { X/b, Y/d, Z/f1 }
		clear(d),
		block(c)
	});
	pSub = 1.0f - (2 * expectedProb - expectedProb * expectedProb);
	EXPECT_TRUE(abs(rule->fulfilmentProbability(state, movePred2(b, d), instances, prematches, subs) - pSub) < 0.0001f);
	EXPECT_FALSE(prematches);
	/*EXPECT_TRUE(rule->cacheState == state);
	EXPECT_TRUE(rule->cacheProbs.size() == 2);
	EXPECT_TRUE(rule->cacheProbs[Substitution({ x, y, z }, { b, d, e }, false)] == pSub1);
	EXPECT_TRUE(rule->cacheProbs[Substitution({ x, y, z }, { b, d, f1 }, false)] == pSub2);*/

	updatedPrecondNecs = rule->precondsNecessities;
	updatedConstNecs = rule->constsNecessities;
	notCovSubs = {
		Substitution({ x, y, z }, { b, d, f1 }, false),
		Substitution({ x, y, z }, { b, d, e }, false)
	};
	/*EXPECT_TRUE(rule->fulfilmentAndUpdatedNecsNotCov(updatedPrecondNecs, updatedConstNecs, state, notCovSubs) == pSub1 + (1 - pSub1) * pSub2);
	EXPECT_TRUE(updatedPrecondNecs[on(x, z)] == 1.0f);
	EXPECT_TRUE(updatedPrecondNecs[clear(y)] == expectedProb);
	EXPECT_TRUE(updatedPrecondNecs[clear(z)] == expectedProb / (1.0f - pSub1) / (1.0f - pSub2));
	EXPECT_TRUE(updatedPrecondNecs[on(z, y)] == expectedProb / (1.0f - pSub2));
	EXPECT_TRUE(updatedPrecondNecs[block(a)] == expectedProb / (1.0f - pSub1) / (1.0f - pSub2));
	EXPECT_TRUE(updatedPrecondNecs[block(c)] == expectedProb);
	EXPECT_TRUE(updatedConstNecs[a] == expectedProb);
	EXPECT_TRUE(updatedConstNecs[c] == expectedProb);*/

	// This second algorithm should not depend on the order of application of the substitutions
	updatedPrecondNecs = rule->precondsNecessities;
	updatedConstNecs = rule->constsNecessities;
	notCovSubs = {
		Substitution({ x, y, z }, { b, d, e }, false),
		Substitution({ x, y, z }, { b, d, f1 }, false)
	};
	/*EXPECT_TRUE(rule->fulfilmentAndUpdatedNecsNotCov(updatedPrecondNecs, updatedConstNecs, state, notCovSubs) == pSub1 + (1 - pSub1) * pSub2);
	EXPECT_TRUE(updatedPrecondNecs[on(x, z)] == 1.0f);
	EXPECT_TRUE(updatedPrecondNecs[clear(y)] == expectedProb);
	EXPECT_TRUE(updatedPrecondNecs[clear(z)] == expectedProb / (1.0f - pSub1) / (1.0f - pSub2));
	EXPECT_TRUE(updatedPrecondNecs[on(z, y)] == expectedProb / (1.0f - pSub2));
	EXPECT_TRUE(updatedPrecondNecs[block(a)] == expectedProb / (1.0f - pSub1) / (1.0f - pSub2));
	EXPECT_TRUE(updatedPrecondNecs[block(c)] == expectedProb);
	EXPECT_TRUE(updatedConstNecs[a] == expectedProb);
	EXPECT_TRUE(updatedConstNecs[c] == expectedProb);*/
}

TEST_F(LearningAgentTest, CdProbTests) {
	shared_ptr<ActionRule> rule = make_shared<ActionRule>(set<Literal>{}, movePred2(x, y), set<Literal>{}, set<Literal>{}, set<shared_ptr<ActionRule>>{}, 0.05f, false);

	map<Literal, float> precondNecs;
	map<Term, float> constNecs;
	vector<pair<vector<Literal>, vector<Term>>> cds;

	EXPECT_TRUE(rule->cdProb(precondNecs, constNecs, cds) == 1.0f);

	precondNecs = {
		{ block(a), 0.2f },
		{ block(b), 0.4f },
		{ block(c), 0.6f },
		{ block(d), 0.8f }
	};

	constNecs = {
		{ a, 0.1f },
		{ b, 0.3f },
		{ c, 0.5f },
		{ d, 0.7f }
	};

	cds = {
		{{ block(a) }, { a }},
		{{ block(b) }, { b }},
		{{ block(c) }, { c }}
	};

	float expectedProb = (0.2f + 0.1f - 0.2f * 0.1f) * (0.4f + 0.3f - 0.4f * 0.3f) * (0.6f + 0.5f - 0.6f * 0.5f);
	EXPECT_TRUE(rule->cdProb(precondNecs, constNecs, cds) == expectedProb);

	cds = {
		{{ block(a), block(d) }, { a, d }},
		{{ block(b), block(d) }, { b, d }},
		{{ block(c), block(d) }, { c, d }}
	};
	EXPECT_TRUE(abs(rule->cdProb(precondNecs, constNecs, cds) - 0.947795f) < 0.0001f);

	cds = {
		{{ block(a), block(d) }, { a, d }}
	};
	EXPECT_TRUE(abs(rule->cdProb(precondNecs, constNecs, cds) - 0.956800f) < 0.0001f);

	cds = {
		{{ block(a), block(b) }, { a, b }},
		{{ block(b), block(c) }, { b, c }},
		{{ block(c), block(d) }, { c, d }},
		{{ block(d), block(a) }, { d, a }}
	};
	EXPECT_TRUE(abs(rule->cdProb(precondNecs, constNecs, cds) - 0.647075f) < 0.0001f);
}

TEST_F(LearningAgentTest, GeneralizationAndTreeTests) {
	//// Containers
	//vector<Literal> preconds;
	//vector<Literal> add;
	//vector<Literal> del;
	//set<shared_ptr<ActionRule>> nullParents;

	//// Setting initial state

	////	a		e
	////	c	b	d
	////	--	--	--
	////	f1	f2	f3

	//State state0 = State({
	//	on(c, f1),
	//	on(a, c),
	//	clear(a),
	//	
	//	on(b, f2),
	//	clear(b),

	//	on(d, f3),
	//	on(e, d),
	//	clear(e),

	//	block(a),
	//	block(b),
	//	block(c),
	//	block(d),
	//	block(e)
	//});

	//// Performing a few actions
	//Literal action1 = movePred3(a, b, c);
	//Literal simpleAction1 = movePred2(a, b);

	//State state1 = domain->tryAction(state0, instances, action1).obj;
	////	->	a	e
	////	c	b	d
	////	--	--	--
	////	f1	f2	f3

	//Literal action2 = movePred3(e, c, d);
	//Literal simpleAction2 = movePred2(e, c);

	//State state2 = domain->tryAction(state1, instances, action2).obj;
	////	e	a	<-
	////	c	b	d
	////	--	--	--
	////	f1	f2	f3

	//Literal action3 = movePred3(d, a, f3);
	//Literal simpleAction3 = movePred2(d, a);

	//State state3 = domain->tryAction(state2, instances, action3).obj;
	////		d
	////	e	a	/\
	////	c	b	||
	////	--	--	--
	////	f1	f2	f3

	//Literal action4 = movePred3(e, f3, c);
	//Literal simpleAction4 = movePred2(e, f3);

	//State state4 = domain->tryAction(state3, instances, action4).obj;
	////		d
	////	->	a	
	////	c	b	e
	////	--	--	--
	////	f1	f2	f3

	//EXPECT_TRUE(state4 == State({
	//	on(c, f1),
	//	clear(c),

	//	on(b, f2),
	//	on(a, b),
	//	on(d, a),
	//	clear(d),

	//	on(e, f3),
	//	clear(e),
	//	
	//	block(a),
	//	block(b),
	//	block(c),
	//	block(d),
	//	block(e)
	//}));

	//// Creating traces
	//Trace trace01 = Trace(state0, simpleAction1, true, state1);
	//Trace trace12 = Trace(state1, simpleAction2, true, state2);
	//Trace trace23 = Trace(state2, simpleAction3, true, state3);
	//Trace trace34 = Trace(state3, simpleAction4, true, state4);

	//// Feeding the traces to the agent
	//agent->updateKnowledge(trace01);
	//agent->updateKnowledge(trace12);
	//agent->updateKnowledge(trace23);
	//agent->updateKnowledge(trace34);

	//shared_ptr<ActionRule> ex1, ex2, ex3, ex4, rule1, rule2, rule3;

	//set<shared_ptr<ActionRule>> activeRules = agent->rules;
	//set<shared_ptr<ActionRule>> counterExamples = agent->counterExamples;
	//
	//// Reading and checking tree structure:
	//
	////	Ex1		Ex2
	////	  \		/
	////	   Rule1	 Ex3
	////		   \     /
	////			Rule2	 Ex4
	////				\	 /
	////				 Rule3

	//EXPECT_TRUE(counterExamples.size() == 4);
	//EXPECT_TRUE(activeRules.size() == 1);

	//rule3 = *activeRules.begin();
	//EXPECT_TRUE(rule3->generalityLevel() == 3);
	//EXPECT_TRUE(rule3->parents.size() == 2);

	//foreach(par, rule3->parents)
	//	if ((*par)->generalityLevel() == 2)
	//		rule2 = *par;
	//	else
	//		ex4 = *par;

	//EXPECT_TRUE(ex4->generalityLevel() == 0);
	//EXPECT_TRUE(ex4->parents.size() == 0);

	//EXPECT_TRUE(rule2->generalityLevel() == 2);
	//EXPECT_TRUE(rule2->parents.size() == 2);

	//foreach(par, rule2->parents)
	//	if ((*par)->generalityLevel() == 1)
	//		rule1 = *par;
	//	else
	//		ex3 = *par;

	//EXPECT_TRUE(ex3->generalityLevel() == 0);
	//EXPECT_TRUE(ex3->parents.size() == 0);

	//EXPECT_TRUE(rule1->generalityLevel() == 1);
	//EXPECT_TRUE(rule1->parents.size() == 2);

	//foreach(par, rule1->parents)
	//	if ((*par)->actionLiteral == action1)
	//		ex1 = *par;
	//	else
	//		ex2 = *par;

	//EXPECT_TRUE(ex1->generalityLevel() == 0);
	//EXPECT_TRUE(ex1->parents.size() == 0);

	//EXPECT_TRUE(ex2->generalityLevel() == 0);
	//EXPECT_TRUE(ex2->parents.size() == 0);

	//// Checking preconditions of example rules
	//EXPECT_TRUE(allEqNoOrder(toVec(ex1->preconditions), {
	//	on(c, f1),
	//	on(a, c),
	//	on(b, f2),
	//	clear(a),
	//	clear(b),
	//	block(a),
	//	block(b),
	//	block(c)
	//}));
	//EXPECT_TRUE(allEqNoOrder(toVec(ex2->preconditions), {
	//	on(c, f1),
	//	on(e, d),
	//	on(d, f3),
	//	clear(c),
	//	clear(e),
	//	block(e),
	//	block(c),
	//	block(d)
	//}));
	//EXPECT_TRUE(allEqNoOrder(toVec(ex3->preconditions), {
	//	on(d, f3),
	//	on(a, b),
	//	on(b, f2),
	//	clear(a),
	//	clear(d),
	//	block(a),
	//	block(b),
	//	block(d)
	//}));
	//EXPECT_TRUE(allEqNoOrder(toVec(ex4->preconditions), {
	//	on(e, c),
	//	on(c, f1),
	//	clear({f3}),
	//	clear(e),
	//	block(e),
	//	block(c)
	//}));

	//// Checking variable usage of generalized rules
	//EXPECT_TRUE(rule1->parameters.size() == 5);
	//EXPECT_TRUE(rule2->parameters.size() == 4);
	//EXPECT_TRUE(rule3->parameters.size() == 3);

	//Variable any = Variable::anyVar();

	//// Checking preconditions of generalized rules
	///*EXPECT_TRUE(allEqNoOrder(toVec(rule1->preconditions), {
	//	on(any, any),
	//	on(any, any),
	//	on(any, any),
	//	clear(any),
	//	clear(any),
	//	block(any),
	//	block(any),
	//	block(any)
	//}));
	//EXPECT_TRUE(rule1->preconditions.size() == 8);

	//EXPECT_TRUE(allEqNoOrder(toVec(rule2->preconditions), {
	//	on(any, any),
	//	on(any, any),
	//	clear(any),
	//	clear(any),
	//	block(any),
	//	block(any)
	//	}));
	//EXPECT_TRUE(rule2->preconditions.size() == 6);

	//EXPECT_TRUE(allEqNoOrder(toVec(rule3->preconditions), {
	//	on(any, any),
	//	clear(any),
	//	clear(any),
	//	block(any)
	//	}));
	//EXPECT_TRUE(rule3->preconditions.size() == 4);*/

	//EXPECT_TRUE(rule1->countLeaves() == 2);
	//EXPECT_TRUE(rule2->countLeaves() == 3);
	//EXPECT_TRUE(rule3->countLeaves() == 4);

	//Predicate black = Predicate("black", 1), white = Predicate("white", 1);
	//Instance floor = Instance("floor"), g = Instance("g"), f = Instance("f");

	//state0 = State({
	//	black(c),
	//	clear(a), clear(b), clear(e), clear(g),
	//	on(a, d), on(b, floor), on(c, f), on(d, floor), on(e, c), on(f, floor), on(g, floor),
	//	white(a), white(b), white(d), white(e), white(f), white(g)
	//});
	//action1 = movePred2(g, a);
	//state1 = State({
	//	black(c),
	//	clear(b), clear(e), clear(g),
	//	on(a, d), on(b, floor), on(c, f), on(d, floor), on(e, c), on(f, floor), on(g, a),
	//	white(a), white(b), white(d), white(e), white(f), white(g)
	//});
	//trace01 = Trace(state0, action1, true, state1);

	//action2 = movePred2(b, e);
	//state2 = State({
	//	black(c),
	//	clear(b), clear(g),
	//	on(a, d), on(b, e), on(c, f), on(d, floor), on(e, c), on(f, floor), on(g, a),
	//	white(a), white(b), white(d), white(e), white(f), white(g)
	//});
	//trace12 = Trace(state1, action2, true, state2);

	//for (int i = 0; i < 20; i++) {
	//	agent->init(domain, { a, b, c, d, e, f, g, floor }, Goal(), trace);
	//	agent->updateKnowledge(trace01);
	//	agent->updateKnowledge(trace12);

	//	EXPECT_TRUE(agent->rules.size() == 1);
	//}
}
