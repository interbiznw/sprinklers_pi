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

int16_t Weather::GetScale(const ReturnVals &vals) const
{
	// If the weather values are invalid, return a default adjustment of 100%
	if (!vals.valid)
		return 100;

	// Load biases from EEPROM (stored as 2-byte values)
	int temp_bias = (int16_t)(EEPROM.read(ADDR_TEMP_BIAS) | (EEPROM.read(ADDR_TEMP_BIAS + 1) << 8));
	int humid_bias = (int16_t)(EEPROM.read(ADDR_HUMIDITY_BIAS) | (EEPROM.read(ADDR_HUMIDITY_BIAS + 1) << 8));
	int rain_bias = (int16_t)(EEPROM.read(ADDR_RAIN_BIAS) | (EEPROM.read(ADDR_RAIN_BIAS + 1) << 8));

	// Calculate the average humidity between the maximum and minimum values
	const int avgHumidity = (vals.maxhumidity + vals.minhumidity) / 2;

	// 🌿 Humidity Adjustment:
	// Targeting a humidity of 55%. If the humidity is less than 55%, increase the adjustment.
	// If it's above 65%, reduce the adjustment.
	int humid_factor = 0;
	if (avgHumidity < 76)
		humid_factor = (76 - avgHumidity) * 1;  // Add +1% per % below 55
	else if (avgHumidity > 80)
		humid_factor = (76 - avgHumidity) / 2;  // Reduce penalty if above 65%
	humid_factor += humid_bias;  // Apply any EEPROM-stored humidity bias

	// 🌞 Temperature Adjustment:
	// Use a weighted average of the mean temperature and the high temperature.
	// Baseline is 70°F, with blended temperature affecting the adjustment.
	int blendedTemp = (int)((vals.meantempi * 0.7f) + (vals.tempHigh * 0.3f));  // 70% weight to mean, 30% to high
	int temp_factor = 0;
	if (blendedTemp > 60)
		temp_factor = (blendedTemp - 60) * 4;  // +5% for every degree above 70°F
	else
		temp_factor = (blendedTemp - 60) * 2;  // +2% for every degree below 70°F
	temp_factor += temp_bias;  // Apply any EEPROM-stored temperature bias

	// ☔ Rain Adjustment:
	// Only consider rainfall above 0.05 inches. For each 0.01" above 0.05", reduce the adjustment by 2.5%.
	float totalPrecipInches = (vals.precipi + vals.precip_today) / 100.0f;  // Convert precipitation to inches
	int rain_factor = 0;
	if (totalPrecipInches > 0.05f)
		rain_factor = (int)((totalPrecipInches - 0.05f) * -250.0f);  // Subtract 2.5% per 0.01" over 0.05"
	rain_factor += rain_bias;  // Apply any EEPROM-stored rainfall bias

	// Clamp the final adjustment between 0% and 200%
	const int adj = spi_min(spi_max(0, 100 + humid_factor + temp_factor + rain_factor), 200);

	// Log the adjusted values for debugging
	trace(F("PNW Adjusted: H(%d), T(%d), R(%d) => Final %d%%\n"),
		humid_factor, temp_factor, rain_factor, adj);

	// Return the final adjustment as a percentage
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
