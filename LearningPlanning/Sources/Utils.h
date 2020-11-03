/**
 * @author Thomas Lamson
 * Contact: thomas.lamson@cea.fr
 */

#pragma once

#include <random>
#include <iterator>
#include <map>
#include <set>
#include <tuple>
#include <string>
#include <limits>
#include <iostream>

#ifndef foreach
#define foreach(iter, iterable) for(auto iter = iterable.begin(); iter != iterable.end(); iter++)
#endif

#ifndef foreachindex
#define foreachindex(i, iterable) for(size_t i = 0; i < iterable.size(); i++)
#endif

#ifndef debug
#define debug if(debugPrints) cout
#endif

#ifndef log
#define log if(verbose) cout
#endif

using namespace std;

extern std::mt19937 globalRandomDevice;

std::string varName(int i);

template<typename Iter>
Iter select_randomly(Iter start, Iter end, std::mt19937& g) {
	std::uniform_int_distribution<__int64> dis(0, std::distance(start, end) - 1);
	std::advance(start, dis(g));
	return start;
}

template<typename Iter>
Iter select_randomly(Iter start, Iter end) {
	return select_randomly(start, end, globalRandomDevice);
}

template<typename Iter>
Iter select_randomly_weighted(Iter start, vector<float> weights, std::mt19937& g) {
	std::uniform_real_distribution<float> dis(0.0f, 1.0f);
	float sample = dis(g);
	float cumulWeight = 0.0f;
	for (size_t i = 0; i < weights.size(); i++) {
		cumulWeight += weights[i];
		if (cumulWeight >= sample) {
			std::advance(start, i);
			return start;
		}
	}
	std::advance(start, weights.size() - 1);
	return start;
}

template<typename Iter>
Iter select_randomly_weighted(Iter start, vector<float> weights) {
	return select_randomly_weighted(start, weights, globalRandomDevice);
}

template<typename Iter, typename T>
bool in(Iter const& container, T const& elem) {
	for (auto it = container.begin(); it != container.end(); it++)
		if (*it == elem)
			return true;
	return false;
}

template<typename K, typename T>
bool in(std::map<K, T> const& container, K const& elem) {
	return container.find(elem) != container.end();
}

template<typename T>
void insertUnique(vector<T>* vec, T const& elem) {
	if (!in(*vec, elem))
		vec->push_back(elem);
}

template<typename T>
void removeOccurences(vector<T>* vec, T const& elem) {
	size_t i = 0;
	while (i < vec->size()) {
		if ((*vec)[i] == elem) vec->erase(i);
		else i++;
	}
}

template<typename T>
void removeFirst(vector<T>* vec, T const& elem) {
	for (size_t i = 0; i < vec->size(); i++)
		if ((*vec)[i] == elem) {
			vec->erase(vec->begin() + i);
			return;
		}
}

template<typename T>
bool allEq(std::vector<T> const& vec1, std::vector<T> const& vec2) {
	if (vec1.size() != vec2.size()) return false;
	for(int i = 0; i < vec1.size(); i++)
		if (vec1[i] != vec2[i])
			return false;
	return true;
}

template<typename T>
bool allEqNoOrder(std::vector<T> const& vec1, std::vector<T> const& vec2) {
	if (vec1.size() != vec2.size()) return false;
	for (int i = 0; i < vec1.size(); i++)
		if (!in(vec1, vec2[i]) || !in(vec2, vec1[i]))
			return false;
	return true;
}

template<typename T>
bool allEqNoOrder(std::set<T> const& set1, std::set<T> const& set2) {
	if (set1.size() != set2.size()) return false;
	foreach(elt, set1)
		if (!in(set2, *elt))
			return false;
	return true;
}

template<typename K, typename V>
std::set<K> keys(std::map<K, V> const& m) {
	std::set<K> r;
	foreach(pair, m)
		r.insert(pair->first);
	return r;
}

template<typename K, typename V>
std::set<V> values(std::map<K, V> const& m) {
	std::set<V> r;
	foreach(pair, m)
		r.insert(pair->second);
	return r;
}

template<typename T>
std::set<T> operator+(std::set<T> const& a, std::set<T> const& b) {
	std::set<T> r = a;
	foreach(it, b)
		r.insert(*it);
	return r;
}

template<typename T>
std::set<T> operator-(std::set<T> const& a, std::set<T> const& b) {
	std::set<T> r;
	foreach(it, a)
		if (!in(b, *it))
			r.insert(*it);
	return r;
}

template<typename T>
std::vector<T> operator+(std::vector<T> const& a, std::vector<T> const& b) {
	std::vector<T> r = a;
	foreach(it, b)
		r.push_back(*it);
	return r;
}

template<typename T>
std::vector<T> operator+(std::vector<T> const& a, std::set<T> const& b) {
	std::vector<T> r = a;
	foreach(it, b)
		r.push_back(*it);
	return r;
}

template<typename T>
std::set<T> operator+(std::set<T> const& a, std::vector<T> const& b) {
	std::set<T> r = a;
	foreach(it, b)
		r.insert(*it);
	return r;
}

template<typename T>
std::set<T> operator-(std::set<T> const& a, std::vector<T> const& b) {
	std::set<T> r;
	foreach(it, a)
		if (!in(b, *it))
			r.insert(*it);
	return r;
}

template<typename T>
std::vector<T> toVec(std::set<T> const& a) {
	std::vector<T> r;
	foreach(it, a)
		r.push_back(*it);
	return r;
}

template<typename T>
std::set<T> toSet(std::vector<T> const& a) {
	std::set<T> r;
	foreach(it, a)
		r.insert(*it);
	return r;
}

template<typename Iter>
string join(string separator, Iter const& args, size_t startIndex, size_t endIndex) {
	string result = "";
	bool first = true;
	size_t index = 0;
	foreach(it, args) {
		if (index < startIndex) {
			index++;
			continue;
		}
		if (index >= endIndex) break;

		if (first) first = false;
		else result += separator;
		result += it->toString();
	}
	return result;
}

template<typename Iter>
string join(string separator, Iter const& args, size_t startIndex) {
	return join(separator, args, startIndex, args.size());
}

template<typename Iter>
string join(string separator, Iter const& args) {
	return join(separator, args, 0, args.size());
}

template<typename Iter>
string join(Iter const& args) {
	return join(", ", args);
}

template<>
string join(string separator, std::vector<string> const& args);

template<typename K, typename V>
string joinmap(string separator, std::map<K, V> const& args) {
	string result;
	bool first = true;
	foreach(pair, args) {
		if (first) first = false;
		else result += separator;
		result += pair->first.toString() + ": " + pair->second.toString();
	}
	return result;
}

template<typename K>
string joinmap(string separator, std::map<K, float> const& args) {
	string result;
	bool first = true;
	foreach(pair, args) {
		if (first) first = false;
		else result += separator;
		result += pair->first.toString() + ": " + to_string(pair->second);
	}
	return result;
}

template<typename K, typename V>
string joinmap(std::map<K, V> const& args) {
	return joinmap(", ", args);
}

string padString(size_t level);

bool isInfinite(const float& value);
bool isNan(const float& value);
bool isValid(const float& value);
bool isProb(const float& value);
void assertMsg(const bool& value, const std::string& msg);

float clamp(float val, float minv, float maxv);

string formatPercent(const float value);

extern bool debugPrints;
extern std::map<std::string, std::tuple<std::uint8_t, std::uint8_t, std::uint8_t>> colorMap;
