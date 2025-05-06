// Wunderground.cpp
// Updated to support new Weather Company PWS API using libcurl
// Author: Richard Zimmerman (Original), Modified by ChatGPT

#include "config.h"
#ifdef WEATHER_WUNDERGROUND

#include "Wunderground.h"
#include "core.h"
#include "port.h"
#include <curl/curl.h>
#include <jsoncpp/json/json.h>
#include <sstream>

Wunderground::Wunderground(void)
{
	m_wundergroundAPIHost = "api.weather.com";
}

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
	((std::string*)userp)->append((char*)contents, size * nmemb);
	return size * nmemb;
}

Weather::ReturnVals Wunderground::InternalGetVals(const Weather::Settings & settings) const
{
	Weather::ReturnVals vals = {0};
	CURL* curl;
	CURLcode res;
	std::string readBuffer;

	char url[512];
	snprintf(url, sizeof(url),
		"https://api.weather.com/v2/pws/observations/current?stationId=%s&format=json&units=e&apiKey=%s",
		settings.pws, settings.key);

	curl = curl_easy_init();
	if (!curl) {
		trace("curl_easy_init() failed\n");
		return vals;
	}

	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "sprinklerPi");

	res = curl_easy_perform(curl);
	if (res != CURLE_OK) {
		trace("CURL error: %s\n", curl_easy_strerror(res));
		curl_easy_cleanup(curl);
		return vals;
	}

	curl_easy_cleanup(curl);

	Json::Value root;
	Json::CharReaderBuilder builder;
	std::string errs;
	std::istringstream s(readBuffer);

	if (!Json::parseFromStream(builder, s, &root, &errs)) {
		trace("JSON parse error: %s\n", errs.c_str());
		return vals;
	}

	const auto& obs = root["observations"][0];
	if (!obs.isObject()) {
		trace("No observation data in response\n");
		return vals;
	}

	vals.valid = true;
	vals.keynotfound = false;

	vals.maxhumidity = obs["humidity"].asInt();
	vals.minhumidity = vals.maxhumidity;

	vals.meantempi = obs["imperial"]["temp"].asInt();
	double precip = obs["imperial"]["precipTotal"].asDouble();
	vals.precipi = vals.precip_today = static_cast<int>(precip * 100.0);

	return vals;
}

#endif
