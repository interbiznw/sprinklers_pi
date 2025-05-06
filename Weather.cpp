// Weather.cpp
// This file manages the retrieval of Weather related information and adjustment of durations
// Author: Richard Zimmerman
// Copyright (c) 2013 Richard Zimmerman

#include "Weather.h"
#include "core.h"
#include "port.h"
#include <string.h>
#include <stdlib.h>
#include "settings.h" // Include to read EEPROM bias values
#include <avr/eeprom.h>
Weather::Settings Weather::GetSettings(void) {
	Settings settings = {0};

	GetApiKey(settings.key);
	GetApiId(settings.apiId);
	GetApiSecret(settings.apiSecret);
	GetPWS(settings.pws);
	GetLoc(settings.location);

	settings.zip = GetZip();
	settings.usePws = GetUsePWS();

	return settings;
}

int16_t Weather::GetScale(void) const
{
	ReturnVals vals = this->GetVals();
	return this->GetScale(vals);
}

int16_t Weather::GetScale(const Weather::Settings & settings) const
{
	ReturnVals vals = this->GetVals(settings);
	return this->GetScale(vals);
}

int16_t Weather::GetScale(const ReturnVals & vals) const
{
	if (!vals.valid)
		return 100;

	// Load biases from EEPROM if available
	int temp_bias = (int16_t)eeprom_read_word((uint16_t*)1170); // ADDR_TEMP_BIAS
	int humid_bias = (int16_t)eeprom_read_word((uint16_t*)1172); // ADDR_HUMIDITY_BIAS
	int rain_bias = (int16_t)eeprom_read_word((uint16_t*)1174); // ADDR_RAIN_BIAS

	const int avgHumidity = (vals.maxhumidity + vals.minhumidity) / 2;

	// 🌿 Humidity: 55% target. If it's dry (<55), add up to +20%
	int humid_factor = 0;
	if (avgHumidity < 55)
		humid_factor = (55 - avgHumidity) * 1;  // +1% per % below 55
	else if (avgHumidity > 65)
		humid_factor = (55 - avgHumidity) / 2;  // soften penalty if it's humid
	humid_factor += humid_bias;

	// 🌞 Temp: 70°F baseline. Add 5% per °F above, subtract 2% below
	int temp_factor = 0;
	if (vals.meantempi > 70)
		temp_factor = (vals.meantempi - 70) * 5;
	else
		temp_factor = (vals.meantempi - 70) * 2;
	temp_factor += temp_bias;

	// ☔ Rain: first 0.05" is ignored. Subtract 2.5% per 0.01" above
	float totalPrecipInches = (vals.precipi + vals.precip_today) / 100.0f;
	int rain_factor = 0;
	if (totalPrecipInches > 0.05f)
		rain_factor = (int)((totalPrecipInches - 0.05f) * -250.0f);  // -2.5% per 0.01" over 0.05"
	rain_factor += rain_bias;

	// Clamp final adjustment between 0% and 200%
	const int adj = spi_min(spi_max(0, 100 + humid_factor + temp_factor + rain_factor), 200);

	trace(F("PNW Adjusted: H(%d), T(%d), R(%d) => Final %d%%\n"),
		humid_factor, temp_factor, rain_factor, adj);

	return adj;
}

Weather::ReturnVals Weather::GetVals(void) const
{
	Settings settings = this->GetSettings();
	return this->InternalGetVals(settings);
}

Weather::ReturnVals Weather::GetVals(const Settings & settings) const
{
	return this->InternalGetVals(settings);
}

Weather::ReturnVals Weather::InternalGetVals(const Settings & settings) const
{
	// You must override and implement this function
	trace("Warning: Placeholder weather provider called. No weather scaling used.\n");
	ReturnVals vals = {0};
	vals.valid = false;
	return vals;
}
