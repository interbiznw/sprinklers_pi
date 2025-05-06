// Wunderground.cpp (Updated for IBM Weather Company API - Daily Summary)
// Retrieves historical PWS data for weather-based irrigation adjustment

#include "config.h"
#ifdef WEATHER_WUNDERGROUND

#include "Wunderground.h"
#include "core.h"
#include "port.h"
#include <string>
#include <sstream>
#include <curl/curl.h>
#include <json/json.h>

Wunderground::Wunderground(void)
{
	m_wundergroundAPIHost = "api.weather.com";
}

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp)
{
	((std::string*)userp)->append((char*)contents, size * nmemb);
	return size * nmemb;
}

Weather::ReturnVals Wunderground::InternalGetVals(const Weather::Settings & settings) const
{
	Weather::ReturnVals ret = {0};
	std::string readBuffer;
	CURL* curl = curl_easy_init();

	if (!curl)
	{
		trace("CURL init failed\n");
		return ret;
	}

	std::stringstream url;
	url << "https://" << m_wundergroundAPIHost
	<< "/v2/pws/observations/all/1day?stationId=" << settings.pws
	<< "&format=json&units=e&apiKey=" << settings.key;

	curl_easy_setopt(curl, CURLOPT_URL, url.str().c_str());
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");

	CURLcode res = curl_easy_perform(curl);
	if (res != CURLE_OK) {
		trace("curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
		curl_easy_cleanup(curl);
		return ret;
	}
	curl_easy_cleanup(curl);

	Json::CharReaderBuilder builder;
	Json::Value json;
	std::string errs;
	std::istringstream s(readBuffer);
	if (!Json::parseFromStream(builder, s, &json, &errs)) {
		trace("JSON parse error: %s\n", errs.c_str());
		return ret;
	}

	try {
		const Json::Value& obs = json["observations"][0];
		ret.valid = true;
		ret.keynotfound = false;
		ret.meantempi = obs["imperial"]["tempAvg"].asInt();
		ret.minhumidity = obs["humidityMin"].asInt();
		ret.maxhumidity = obs["humidityMax"].asInt();
		ret.precipi = obs["imperial"]["precipTotal"].asFloat() * 100.0f;
		ret.windmph = obs["imperial"]["windSpeedAvg"].asFloat() * 10.0f;
	} catch (const std::exception& e) {
		trace("Error extracting fields: %s\n", e.what());
		ret.valid = false;
	}

	ret.resolvedIP = "(null)";
	return ret;
}

#endif
