/**
 * @author Thomas Lamson
 * Contact: thomas.lamson@cea.fr
 */

#pragma once

#include <string>
#include <iostream>
#include <sstream>
#include <fstream>

#include "Logic/Domain.h"
#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"

using namespace rapidjson;

class JsonParser {
public:
	JsonParser() { }
	JsonParser(shared_ptr<Domain> inDomain) : domain(inDomain) { }

	shared_ptr<Domain> parseDomain(string path) {
		domain = make_shared<Domain>();

		std::string line, text;
		std::ifstream in(path);
		while (std::getline(in, line)) text += line + "\n";
		in.close();
		const char* data = text.c_str();

		Document d;
		d.Parse(data);

		if (d.HasMember("types"))
			parseTypes(d["types"]);
		if (d.HasMember("constants"))
			parseConstants(d["constants"]);
		parsePredicates(d["predicates"]);
		parseActions(d["actions"]);

		return domain;
	}

	Literal parseActionLiteral(string command) {
		string tmp = "";
		Predicate action;
		vector<Term> params;
		bool first = true;
		char c = ' ';

		vector<Term> allInsts = toVec(domain->getConstants());
		foreach(inst, problem->instances)
			allInsts.push_back(*inst);

		for (unsigned int i = 0; i < command.length() + 1; i++) {
			if (i < command.length())
				c = command[i];

			if (i == command.length() || c == ' ' || c == '(' || c == ' ' || c == ')' || c == ',') {
				if (tmp != "") {
					if (first) {
						first = false;
						action = domain->getActionPredByName(tmp);
						if (action == Predicate()) {
							cout << "Didn't recognized action \"" << tmp << "\"." << endl;
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
							cout << "Didn't recognized object \"" << tmp << "\"." << endl;
							return Literal();
						}
					}

					tmp = "";
				}
			}
			else {
				tmp += c;
			}
		}

		if (params.size() != action.arity) {
			cout << "Action " << action.name << " requires " << action.arity << " arguments." << endl;
			return Literal();
		}

		return Literal(action, params);
	}

	shared_ptr<Problem> parseProblem(string path, string headstartPath) {
		problem = make_shared<Problem>();
		problem->domain = domain;

		std::string line, text;
		std::ifstream in(path);
		while (std::getline(in, line)) text += line + "\n";
		in.close();
		const char* data = text.c_str();

		Document d;
		d.Parse(data);

		parseInstances(d["objects"]);
		parseInitState(d["init"]);
		parseGoal(d["goal"]);

		if (headstartPath != "") {
			vector<Literal> hsActions;
			in = std::ifstream(headstartPath);
			while (std::getline(in, line)) {
				Literal act = parseActionLiteral(line);
				if (act == Literal()) cout << "ERROR PARSING HEADSTART ACTION: " << line << endl;
				else hsActions.push_back(act);
			}
			in.close();

			problem->headstartActions = hsActions;
		}

		return problem;
	}

private:
	shared_ptr<Domain> domain;
	shared_ptr<Problem> problem;

	void recursiveAddTypes(map<string, vector<string>> typesMap, string beginType, shared_ptr<TermType> parentType) {
		shared_ptr<TermType> t = make_shared<TermType>(beginType, parentType);
		domain->addType(t);

		if (in(typesMap, beginType))
			foreach(child, typesMap[beginType])
				recursiveAddTypes(typesMap, *child, t);
	}

	void parseTypes(Value& jsonTypes) {
		auto arr = jsonTypes.GetArray();

		map<string, vector<string>> typesMap;
		vector<string> baseTypes;

		for (auto it = arr.Begin(); it != arr.End(); it++) {
			auto typeObj = (*it).GetObject();

			string typeName = typeObj["name"].GetString();
			
			if (!typeObj["parent"].IsNull()) {
				string parentName = typeObj["parent"].GetString();
				if (in(typesMap, parentName))
					typesMap[parentName].push_back(typeName);
				else
					typesMap[parentName] = { typeName };
			}
			else
				baseTypes.push_back(typeName);
		}

		foreach(tn, baseTypes) {
			recursiveAddTypes(typesMap, *tn, nullptr);
		}
	}

	void parsePredicates(Value& jsonPreds) {
		auto arr = jsonPreds.GetArray();
		for (auto it = arr.Begin(); it != arr.End(); it++) {
			auto pred = (*it).GetObject();

			std::string predName = pred["name"].GetString();
			int predArity = pred["arity"].GetInt();

			domain->addPredicate(Predicate(predName, predArity));
		}
	}

	void parseActions(Value& jsonActions) {
		auto arr = jsonActions.GetArray();

		map<string, Term> atoms;
		foreach(cst, domain->constants)
			atoms[cst->name] = *cst;

		map<string, Predicate> actionPreds;

		for (auto it = arr.Begin(); it != arr.End(); it++) {
			auto act = (*it).GetObject();

			string actionName = act["name"].GetString();

			map<string, Term> tmpAtoms = atoms;
			atoms.clear();
			foreach(pair, tmpAtoms)
				if (!pair->second.isVariable)
					atoms[pair->first] = pair->second;

			vector<Term> parameters;
			size_t pIndex = 0;

			for (auto pit = act["parameters"].GetArray().Begin(); pit != act["parameters"].GetArray().End(); pit++) {
				std::string pName = (*pit).GetString();

				shared_ptr<TermType> pType = nullptr;
				if (act.HasMember("paramtypes")) {
					bool nullType = act["paramtypes"].GetArray()[(rapidjson::SizeType) pIndex].IsNull();
					if (!nullType)
						pType = domain->getTypeByName(act["paramtypes"].GetArray()[(rapidjson::SizeType) pIndex].GetString());
					pIndex++;
				}

				Term param;
				
				if (in(atoms, pName)) param = atoms[pName];
				else {
					param = Variable(pName, pType);
					atoms[pName] = param;
				}

				parameters.push_back(param);
			}

			vector<Term> variables;
			if (act.HasMember("variables")) {
				pIndex = 0;
				for (auto pit = act["variables"].GetArray().Begin(); pit != act["variables"].GetArray().End(); pit++) {
					std::string vName = (*pit).GetString();

					shared_ptr<TermType> vType = nullptr;
					if (act.HasMember("vartypes"))
						vType = domain->getTypeByName(act["vartypes"].GetArray()[(rapidjson::SizeType) pIndex++].GetString());

					Term var;

					if (in(atoms, vName)) var = atoms[vName];
					else {
						var = Variable(vName, vType);
						atoms[vName] = var;
					}

					variables.push_back(var);
				}
			}

			vector<Literal> truePrecond;
			vector<Literal> falsePrecond;
			for (auto pit = act["precondition"].GetArray().Begin(); pit != act["precondition"].GetArray().End(); pit++) {
				bool first = true;
				bool isFalse = false;
				std::string predName;
				vector<Term> params;

				for (auto eit = (*pit).GetArray().Begin(); eit != (*pit).GetArray().End(); eit++) {
					if (first) {
						if (*eit == "!") isFalse = true;
						else {
							first = false;
							predName = (*eit).GetString();
						}
					}
					else {
						string pName = (*eit).GetString();
						Term param;

						if (in(atoms, pName)) param = atoms[pName];
						else {
							param = Variable(pName);
							atoms[pName] = param;
						}

						params.push_back(param);
					}
				}

				Literal precond = Literal(domain->getPredByName(predName), params);
				if (isFalse) falsePrecond.push_back(precond);
				else truePrecond.push_back(precond);
			}

			vector<Literal> addEffects;
			vector<Literal> delEffects;
			for (auto pit = act["add"].GetArray().Begin(); pit != act["add"].GetArray().End(); pit++) {
				bool first = true;
				std::string predName;
				vector<Term> params;

				for (auto eit = (*pit).GetArray().Begin(); eit != (*pit).GetArray().End(); eit++) {
					if (first) {
						first = false;
						predName = (*eit).GetString();
					}
					else {
						string pName = (*eit).GetString();
						Term param;

						if (in(atoms, pName)) param = atoms[pName];
						else {
							param = Term(pName, true);
							atoms[pName] = param;
						}

						params.push_back(param);
					}
				}

				Literal eff = Literal(domain->getPredByName(predName), params);
				addEffects.push_back(eff);
			}

			for (auto pit = act["del"].GetArray().Begin(); pit != act["del"].GetArray().End(); pit++) {
				bool first = true;
				std::string predName;
				vector<Term> params;

				for (auto eit = (*pit).GetArray().Begin(); eit != (*pit).GetArray().End(); eit++) {
					if (first) {
						first = false;
						predName = (*eit).GetString();
					}
					else {
						string pName = (*eit).GetString();
						Term param;

						if (in(atoms, pName)) param = atoms[pName];
						else {
							param = Term(pName, true);
							atoms[pName] = param;
						}

						params.push_back(param);
					}
				}

				Literal eff = Literal(domain->getPredByName(predName), params, false);
				delEffects.push_back(eff);
			}

			Predicate actionPred;
			if (in(actionPreds, actionName)) actionPred = actionPreds[actionName];
			else {
				actionPred = Predicate(actionName, parameters.size());
				actionPreds[actionName] = actionPred;
			}

			Literal actionLiteral = Literal(actionPred, parameters);
			Action action = Action(actionLiteral, truePrecond, falsePrecond, addEffects, delEffects);
			domain->addAction(action);
		}
	}

	void parseConstants(Value& jsonInstances) {
		auto arr = jsonInstances.GetArray();
		for (auto it = arr.Begin(); it != arr.End(); it++) {
			if ((*it).IsString()) {
				string name = (*it).GetString();
				domain->addConstant(Instance(name));
			}
			else {
				auto inst = (*it).GetObject();
				string name = inst["name"].GetString();
				shared_ptr<TermType> type = inst["type"].IsNull() ? nullptr : domain->getTypeByName(inst["type"].GetString());

				domain->addConstant(Instance(name, type));
			}
		}
	}

	void parseInstances(Value& jsonInstances) {
		auto arr = jsonInstances.GetArray();
		for (auto it = arr.Begin(); it != arr.End(); it++) {
			if ((*it).IsString()) {
				string name = (*it).GetString();

				if (problem->getInstByName(name) != Instance())
					continue;

				Instance instance = Instance(name);
				problem->instances.insert(instance);
			}
			else {
				auto inst = (*it).GetObject();
				string name = inst["name"].GetString();

				if (problem->getInstByName(name) != Instance())
					continue;

				shared_ptr<TermType> type = inst["type"].IsNull() ? nullptr : domain->getTypeByName(inst["type"].GetString());

				Instance instance = Instance(name, type);
				problem->instances.insert(instance);
			}
		}
	}

	void parseInitState(Value& jsonState) {
		auto arr = jsonState.GetArray();
		for (auto it = arr.Begin(); it != arr.End(); it++) {
			bool first = true;
			std::string predName;
			vector<Term> params;

			for (auto eit = (*it).GetArray().Begin(); eit != (*it).GetArray().End(); eit++) {
				if (first) {
					first = false;
					predName = (*eit).GetString();
				}
				else {
					params.push_back(problem->getInstByName((*eit).GetString()));
				}
			}

			Literal fact = Literal(domain->getPredByName(predName), params);
			problem->initialState.addFact(fact);
		}
	}

	void parseGoal(Value& jsonGoal) {
		auto arr = jsonGoal.GetArray();
		for (auto it = arr.Begin(); it != arr.End(); it++) {
			bool first = true;
			std::string predName;
			vector<Term> params;

			for (auto eit = (*it).GetArray().Begin(); eit != (*it).GetArray().End(); eit++) {
				if (first) {
					first = false;
					predName = (*eit).GetString();
				}
				else {
					params.push_back(problem->getInstByName((*eit).GetString()));
				}
			}

			Literal fact = Literal(domain->getPredByName(predName), params);
			problem->goal.trueFacts.push_back(fact);
		}
	}
};
