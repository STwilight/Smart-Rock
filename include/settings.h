#include <SmingCore/SmingCore.h>

#define SETTINGS_FILE ".settings.conf"

struct SettingsStorage
{
	byte max_temp;

	bool ap_mode;
	String ap_ssid;
	String ap_psw;

	String st_ssid;
	String st_psw;

	uint64_t heater_on;

	void load()
	{
		DynamicJsonBuffer jsonBuffer;
		if (exist())
		{
			int size = fileGetSize(SETTINGS_FILE);
			char* jsonString = new char[size + 1];
			fileGetContent(SETTINGS_FILE, jsonString, size + 1);
			JsonObject& root = jsonBuffer.parseObject(jsonString);

			JsonObject& settings = root["settings"];

			max_temp = settings["max_temp"];

			ap_mode = settings["ap_mode"];
			ap_ssid = settings["ap_ssid"].asString();
			ap_psw = settings["ap_psw"].asString();

			st_ssid = settings["st_ssid"].asString();
			st_psw = settings["st_psw"].asString();

			/* JsonObject& statistic = root["statistic"];
			   sscanf(statistic["heater_on"].asString(), "%u", &heater_on); */

			/* необходимо использовать конвертацию из String в uint64_t
			 * посредством использования недоступной сейчас в Sming функции sscanf() */

			delete[] jsonString;
		}
	}
	void save()
	{
		DynamicJsonBuffer jsonBuffer;
		JsonObject& root = jsonBuffer.createObject();

		JsonObject& settings = jsonBuffer.createObject();
		root["settings"] = settings;

		settings["max_temp"] = max_temp;

		settings["ap_mode"] = ap_mode;
		settings["ap_ssid"] = ap_ssid.c_str();
		settings["ap_psw"] = ap_psw.c_str();

		settings["st_ssid"] = st_ssid.c_str();
		settings["st_psw"] = st_psw.c_str();

		JsonObject& statistic = jsonBuffer.createObject();
		root["statistic"] = statistic;
		char buffer[20];

		sprintf(buffer, "%u", heater_on);
		statistic["heater_on"] = buffer;

		String rootString;
		root.printTo(rootString);
		fileSetContent(SETTINGS_FILE, rootString);
	}
	bool exist()
	{
		return fileExist(SETTINGS_FILE);
	}
};

static SettingsStorage Settings;
