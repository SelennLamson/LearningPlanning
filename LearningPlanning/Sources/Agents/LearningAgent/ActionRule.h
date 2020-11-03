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

#pragma once

#include <memory>
#include <ctime>
#include <chrono>

#include "Logic/Domain.h"

typedef pair<vector<Literal>, vector<Term>> Unverified;

struct Experiment {
	State state;
	Literal action;

	Experiment(State inState, Literal inAction) : state(inState), action(inAction) { }

	bool operator==(const Experiment& other) const {
		return state == other.state && action == other.action;
	}

	bool operator<(const Experiment& other) const {
		if (action != other.action) return action < other.action;
		if (state != other.state) return state < other.state;
		return false;
	}
};

struct SigmaTheta {
	Substitution sigma, theta;
	Substitution st;

	SigmaTheta(Substitution inSigma, Substitution inTheta) : sigma(inSigma), theta(inTheta) {
		st = sigma.merge(theta);
	}
};

struct RoSigmaTheta {
	Substitution ro, sigma, theta;
	Substitution rst;

	RoSigmaTheta() : RoSigmaTheta(Substitution(false), Substitution(), Substitution()) { }
	RoSigmaTheta(Substitution inRo, Substitution inSigma, Substitution inTheta) : ro(inRo), sigma(inSigma), theta(inTheta) {
		rst = ro.merge(sigma).merge(theta);
	}
};

struct ActionRule : enable_shared_from_this<ActionRule> {
	float startPu;
	//int estimatedPreconditions;
	vector<Term> parameters;

	set<Literal> preconditions;

	set<Literal> removedPreconditions;

	Literal actionLiteral;
	set<Literal> add;
	set<Literal> del;

	set<shared_ptr<ActionRule>> parents;

	State cacheState;
	map<Substitution, float> cacheProbs;

	map<Term, float> constsNecessities;
	map<Literal, float> precondsNecessities;

	ActionRule(set<Literal> inPreconditions, Literal inActionLiteral, set<Literal> inAdd, set<Literal> inDel, set<shared_ptr<ActionRule>> inParents, float startPu, bool filter = true);
	ActionRule(Trace trace, float startPu, bool filter = true);
	
	// Prematching
	set<Substitution> prematchingSubs(shared_ptr<ActionRule> x, Substitution sub = Substitution());
	bool prematches(shared_ptr<ActionRule> x, Substitution sub = Substitution());

	// Postmatching
	set<Substitution> postmatchingSubs(shared_ptr<ActionRule> x, Substitution sub = Substitution());
	bool postmatches(shared_ptr<ActionRule> x, Substitution sub = Substitution());

	// Coverage
	set<Substitution> coveringSubs(shared_ptr<ActionRule> x, Substitution sub = Substitution());
	bool covers(shared_ptr<ActionRule> x, Substitution sub = Substitution());

	// Algorithm 2: LIT-GEN-OI
	Opt<Literal> generalizeLiteralsOI(Literal l1, Literal l2, /*r*/ set<Term>& genVars, /*r*/ Substitution& theta1, /*r*/ Substitution& theta2);

	// Algorithm 10: SELECTION
	bool selection(set<Literal>lr, set<Literal> lx, shared_ptr<ActionRule> x,
				   /*r*/ Substitution& subr, /*r*/ Substitution& subx,
				   /*r*/ set<Term>& genVars, /*r*/ set<Literal>& genLits,
				   /*r*/ Literal& chosenLr, /*r*/ Literal& chosenLx);

	// Algorithm 9: UNE-GEN-OI
	set<Literal> anyGeneralization(set<Literal> lr, set<Literal> lx, shared_ptr<ActionRule> x,
								   /*r*/ Substitution& subr, /*r*/ Substitution& subx, /*r*/ set<Term>& genVars);
	set<Literal> anyGeneralization(shared_ptr<ActionRule> x, /*r*/ Substitution& subr, /*r*/ Substitution& subx, /*r*/ set<Term>& genVars);

	// Non-official algorithm: exact generalization (UNE-GEN with random exhaustive exploration of the tree of generalizations and no fact deletion)
	bool exactGeneralizationLxChoice(Literal chosenLr, set<Literal> lr, set<Literal> lx,
		/*r*/ Substitution& subr, /*r*/ Substitution& subx, /*r*/ set<Term>& genVars, /*r*/ set<Literal>& genLits);
	bool exactGeneralizationLrChoice(set<Literal> lr, set<Literal> lx,
		/*r*/ Substitution& subr, /*r*/ Substitution& subx, /*r*/ set<Term>& genVars, /*r*/ set<Literal>& genLits);

	// Algorithm 7: POST-GENERALIZATION
	bool postGeneralizes(shared_ptr<ActionRule> x, /*r*/ Substitution& subr, /*r*/ Substitution& subx, /*r*/ set<Term>& genVars);



	shared_ptr<ActionRule> makeUseOfVariables();

	int getFreeParameterId(set<Term> blackList) const;
	Variable makeNewVar(set<Term>& genVars, Term param) const;

	bool contradicts(shared_ptr<ActionRule> x);

	void insertParent(shared_ptr<ActionRule> parent);
	void removeParentRecursive(shared_ptr<ActionRule> parent);
	bool wellFormed();
	int generalityLevel() const;
	int countLeaves();
	float maxLeafSimilarity(State state);
	shared_ptr<ActionRule> getLeastGeneralRuleCovering(shared_ptr<ActionRule> example);

	vector<SigmaTheta> applies(State state, vector<Term> instances, Literal inActionLiteral, bool onlyFirst);
	State apply(State state, SigmaTheta sigmaTheta);

	int specificity() const;

	void setRemovedPreconditions(set<Literal> remPreconds);
	
	// Belief-related algorithms
	float fulfilmentProbability(State state, Literal action, vector<Term> instances, /*r*/ bool& prematches, /*r*/ set<Substitution>& subs);
	void generateRandomSubs(State state, Literal action, vector<Term> instances, Substitution rho, Substitution sigma, size_t maxRandomSubs, set<Substitution>& subs);
	
	float computeCdProb(State state, Literal action, vector<Substitution> subs);
	float cdProb(map<Literal, float>& precondNecs, map<Term, float>& constNecs, vector<Unverified> cds);
	float dgcdProb(map<Literal, float>& precondNecs, map<Term, float>& constNecs, Unverified disj, vector<Unverified> conditionalCds);
	void processEffects(/*r*/ set<Unverified>& sigmaPos, /*r*/ set<Unverified>& sigmaNeg,
		State state, Literal action, State effects, vector<Term> instances);


	string toString() const;
	ostream& operator<<(ostream& os) {
		os << toString();
		return os;
	}

private:
	void extractParameters();
};

ostream& operator<<(ostream& os, ActionRule &rule);
