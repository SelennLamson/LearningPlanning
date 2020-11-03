/**
 * @author Thomas Lamson
 * Contact: thomas.lamson@cea.fr
 */

#include "ConfigReader.h"

ConfigReader* ConfigReader::fromFile(string filePath) {
	string line, text;
	ifstream in(filePath);
	while (getline(in, line)) text += line + "\n";
	in.close();
	const char* data = text.c_str();

	ConfigReader* reader = new ConfigReader();
	reader->doc = cJSON_Parse(data);
	return reader;
}

ConfigReader* ConfigReader::fromObject(cJSON* obj) {
	ConfigReader* reader = new ConfigReader();
	reader->doc = obj;
	return reader;
}

ConfigReader* ConfigReader::getSubconfig(string key) {
	if (!cJSON_HasObjectItem(doc, key.c_str())) return fromObject(cJSON_CreateObject());
	cJSON* val = cJSON_GetObjectItemCaseSensitive(doc, key.c_str());
	if (cJSON_IsObject(val)) return fromObject(val);
	return fromObject(cJSON_CreateObject());
}

int ConfigReader::getInt(string key) {
	if (!cJSON_HasObjectItem(doc, key.c_str())) return 0;
	cJSON* val = cJSON_GetObjectItemCaseSensitive(doc, key.c_str());
	if (cJSON_IsNumber(val)) return val->valueint;
	return 0;
}

unsigned ConfigReader::getUint(string key) {
	if (!cJSON_HasObjectItem(doc, key.c_str())) return 0;
	cJSON* val = cJSON_GetObjectItemCaseSensitive(doc, key.c_str());
	if (cJSON_IsNumber(val) && val->valueint >= 0) return val->valueint;
	return 0;
}

double ConfigReader::getDouble(string key) {
	if (!cJSON_HasObjectItem(doc, key.c_str())) return 0;
	cJSON* val = cJSON_GetObjectItemCaseSensitive(doc, key.c_str());
	if (cJSON_IsNumber(val)) return val->valuedouble;
	return 0;
}

float ConfigReader::getFloat(string key) {
	if (!cJSON_HasObjectItem(doc, key.c_str())) return 0;
	cJSON* val = cJSON_GetObjectItemCaseSensitive(doc, key.c_str());
	if (cJSON_IsNumber(val)) return (float)val->valuedouble;
	return 0;
}

string ConfigReader::getString(string key) {
	if (!cJSON_HasObjectItem(doc, key.c_str())) return "";
	cJSON* val = cJSON_GetObjectItemCaseSensitive(doc, key.c_str());
	if (cJSON_IsString(val)) return val->valuestring;
	return "";
}

bool ConfigReader::getBool(string key) {
	if (!cJSON_HasObjectItem(doc, key.c_str())) return false;
	cJSON* val = cJSON_GetObjectItemCaseSensitive(doc, key.c_str());
	return cJSON_IsTrue(val);
}

cJSON* ConfigReader::getArray(string key) {
	if (!cJSON_HasObjectItem(doc, key.c_str())) return cJSON_CreateArray();
	cJSON* val = cJSON_GetObjectItemCaseSensitive(doc, key.c_str());
	if (cJSON_IsArray(val)) return val;
	return cJSON_CreateArray();
}
