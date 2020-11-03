/**
 * @author Thomas Lamson
 * Contact: thomas.lamson@cea.fr
 */

//#pragma once
//
//#include <string>
//#include <iostream>
//#include <sstream>
//#include <fstream>
//
//#include "Logic/Domain.h"
//
//using namespace std;
//
//class PDDLDomainParser {
//public:
//	PDDLDomainParser() { }
//
//	Domain* parse(string path, string domainName) {
//		domain = new Domain();
//
//		file = new ifstream();
//		file->open(path + "/" + domainName + ".pddl");
//
//		char ch;
//		string content = "";
//		while (*file >> noskipws >> ch) {
//			content += ch;
//		}
//
//		content = getThisLevel(content);
//		defineDomain(content);
//
//		file->close();
//
//		return domain;
//	}
//
//private:
//	Domain* domain;
//	ifstream* file;
//	string current;
//
//	void defineDomain(string content) throw (string) {
//		for(char& ch : content) {
//			if (ch == '(') {
//				parseInstruction();
//				current = "";
//			}
//			else if (ch == ' ' || ch == '\n' || ch == '\t') {
//				parseInstruction();
//				current = "";
//			}
//			else if (ch == ')') {
//				current = "";
//			}
//			else {
//				current += ch;
//			}
//		}
//	}
//
//	void parseInstruction() throw (string) {
//		if (current == "define" || current == ":requirements") {
//			skipNext();
//		}
//		else if (current == ":predicates") {
//			parsePredicates();
//		}
//		else if (current == ":action") {
//			parseAction();
//		}
//	}
//
//	void parsePredicates() throw (string) {
//		while (*file >> noskipws >> ch) {
//			if (ch == '(') {
//				parsePredicate();
//			}
//			else if (ch == ')') {
//				return;
//			}
//		}
//	}
//
//	void parsePredicate() throw (string) {
//		current = "";
//		bool gotPredName = false;
//		string predName;
//		vector<string> parameters;
//
//		while (*file >> noskipws >> ch) {
//			if (ch == ' ' || ch == '\n' || ch == '\t') {
//				if (current != "") {
//					if (!gotPredName) {
//						predName = current;
//					}
//					else {
//						parameters.push_back(current);
//					}
//				}
//				current = "";
//			}
//			else if (ch == '?') {
//			}
//			else if (ch == ')') {
//				if (current != "") {
//					if (!gotPredName) {
//						predName = current;
//					}
//					else {
//						parameters.push_back(current);
//					}
//				}
//				current = "";
//
//				Predicate pred = Predicate(predName, parameters.size());
//				domain->addPredicate(pred);
//				return;
//			}
//			else {
//				current += ch;
//			}
//		}
//	}
//	
//	void parseAction() throw (string) {
//		current = "";
//		bool gotActionName;
//		string actionName;
//		vector<Term> parameters;
//		vector<Literal> truePrecond;
//		vector<Literal> falsePrecond;
//		vector<Literal> add;
//		vector<Literal> del;
//
//		while (*file >> noskipws >> ch) {
//			if (ch == ' ' || ch == '\n' || ch == '\t') {
//				if (current == ":parameters") {
//					parseParameters(&parameters);
//				}
//				else if (current == ":precondition") {
//					parsePreconditions(&truePrecond, &falsePrecond);
//				}
//				else if (current == ":effect") {
//					parseEffect(&add, &del);
//				}
//				else if (!gotActionName && current != "") {
//					actionName == current;
//				}
//				current = "";
//			}
//			else if (ch == ')') {
//				Predicate actionPredicate = Predicate(actionName, parameters.size());
//				Literal actionLiteral = Literal(actionPredicate, parameters);
//				Action action = Action(actionLiteral, truePrecond, falsePrecond, add, del);
//				domain->addAction(action);
//				return;
//			}
//			else {
//				current += ch;
//			}
//		}
//	}
//
//	void parseParameters(vector<Term>* parameters) throw (string) {
//		while (*file >> noskipws >> ch) {
//			if (ch == ' ' || ch == '\n' || ch == '\t') {
//				if (current != "") {
//					Term atom = Term(current, true);
//					parameters->push_back(atom);
//				}
//				current = "";
//			}
//			else if (ch == '(' || ch == '?') {
//				current = "";
//			}
//			else if (ch == ')') {
//				if (current != "") {
//					Term atom = Term(current, true);
//					parameters->push_back(atom);
//				}
//				return;
//			}
//			else {
//				current += ch;
//			}
//		}
//	}
//
//	void parsePreconditions(vector<Literal>* truePrecond, vector<Literal>* falsePrecond) throw (string) {
//		while (*file >> noskipws >> ch) {
//			if (ch == ' ' || ch == '\n' || ch == '\t') {
//				if (current == "not") {
//
//				}
//				else if (current != "" && current != "and") {
//					
//				}
//				current = "";
//			}
//			else if (ch == '(' || ch == '?') {
//				current = "";
//			}
//			else if (ch == ')') {
//				if (current != "") {
//					
//				}
//				return;
//			}
//			else {
//				current += ch;
//			}
//		}
//	}
//
//	void parseEffect(vector<Literal>* add, vector<Literal>* del) throw (string) {
//
//	}
//
//	Literal parseLiteral() throw (string) {
//		current = "";
//		bool gotPredName = false;
//		string predName;
//		vector<Term> parameters;
//
//		while (*file >> noskipws >> ch) {
//			if (ch == ' ' || ch == '\n' || ch == '\t') {
//				if (current != "") {
//					if (!gotPredName) {
//						predName = current;
//					}
//					else {
//						Term atom = Term(current, true);
//						parameters.push_back(atom);
//					}
//				}
//				current = "";
//			}
//			else if (ch == '?') {
//			}
//			else if (ch == ')') {
//				if (current != "") {
//					if (!gotPredName) {
//						predName = current;
//					}
//					else {
//						Term atom = Term(current, true);
//						parameters.push_back(atom);
//					}
//				}
//				current = "";
//
//				Predicate pred = Predicate(predName, parameters.size());
//				Literal lit = Literal(pred, parameters);
//				return lit;
//			}
//			else {
//				current += ch;
//			}
//		}
//	}
//
//	void skipNext() throw (string) {
//		int level = 0;
//		while (*file >> noskipws >> ch) {
//			if (ch == '(') {
//				level++;
//			}
//			else if (ch == ')') {
//				level--;
//				if (level <= 0) return;
//			}
//		}
//	}
//
//	string getThisLevel(string content) {
//		int level = 0;
//		current = "";
//		for (char& ch : content) {
//			current += ch;
//			if (ch == '(') {
//				if (level == 0) {
//					current = "(";
//				}
//				level++;
//			}
//			else if (ch == ')') {
//				level--;
//				if (level <= 0) return current;
//			}
//		}
//	}
//};
//
