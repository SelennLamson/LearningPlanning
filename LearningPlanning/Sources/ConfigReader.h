/**
 * @author Thomas Lamson
 * Contact: thomas.lamson@cea.fr
 */

#pragma once

#include <string>
#include <sstream>
#include <fstream>
#include <iostream>

#include "cJSON.h"

using namespace std;

class ConfigReader {
public:
	static ConfigReader* fromFile(string filePath);
	static ConfigReader* fromObject(cJSON* obj);

	int getInt(string key);
	unsigned getUint(string key);
	double getDouble(string key);
	float getFloat(string key);
	string getString(string key);
	bool getBool(string key);

	cJSON* getArray(string key);

	ConfigReader* getSubconfig(string key);

private:
	cJSON* doc = nullptr;
};

extern ConfigReader* config;
