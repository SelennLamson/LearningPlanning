/**
 * @author Thomas Lamson
 * Contact: thomas.lamson@cea.fr
 */

#include "Utils.h"
#include <map>
#include <set>
#include <chrono>

std::mt19937 globalRandomDevice = std::mt19937{ unsigned(std::chrono::high_resolution_clock::now().time_since_epoch().count()) };;

string varName(int i) {
	return "_V" + to_string(i);
}

template<>
string join(string separator, std::vector<string> const& args) {
	string result = "";
	bool first = true;
	foreachindex(i, args) {
		if (first) first = false;
		else result += separator;
		result += args[i];
	}
	return result;
}

string padString(size_t level) {
	string res = "";
	for (size_t i = 0; i < level && i < 10; i++)
		res += "\t";
	return res;
}

bool isInfinite(const float& value) {
	constexpr float max_value = numeric_limits<float>::max();
	constexpr float min_value = -max_value;

	return !(min_value <= value && value <= max_value);
}

bool isNan(const float& value) {
	return value != value;
}

bool isValid(const float& value) {
	return !isInfinite(value) && !isNan(value);
}

bool isProb(const float& value) {
	return isValid(value) && value >= -0.0001f && value <= 1.0001f;
}

void assertMsg(const bool& value, const std::string& msg) {
	if (!value) {
		std::cout << "Assertion error: " << msg << std::endl;
		throw std::logic_error("Assertion error: " + msg);
	}
}

float clamp(float val, float minv, float maxv) {
	return max(minv, min(maxv, val));
}

string formatPercent(const float value) {
	return to_string((int)(value * 100)) + "%";
}
