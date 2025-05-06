// Weather.cpp
// This file manages the retrieval of Weather related information and adjustment of durations
// Author: Richard Zimmerman
// Copyright (c) 2013 Richard Zimmerman
//

#include "Weather.h"
#include "core.h"
#include "port.h"
#include <string.h>
#include <stdlib.h>

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

	const int avgHumidity = (vals.maxhumidity + vals.minhumidity) / 2;

	// Pacific NW tuning:
	// - Less penalty for humidity >60%
	// - Temperature baseline 70°F, mild effect ±1.5%/°F
	// - Rain under 0.05" is ignored, above that is -1.5% per 0.01"

	int humid_factor = 0;
	if (avgHumidity < 45)
		humid_factor = (58 - avgHumidity);  // +1% per % below 58
	else if (avgHumidity > 65)
		humid_factor = (58 - avgHumidity) / 2; // -0.5% per % above 58

	const int temp_factor = (vals.meantempi - 70) * 1.5;  // mild impact

	float totalPrecipInches = (vals.precipi + vals.precip_today) / 100.0f;
	int rain_factor = 0;
	if (totalPrecipInches > 0.05f)
		rain_factor = (int)(totalPrecipInches * -150.0f);  // -1.5% per 0.01" above 0.05"

	const int adj = spi_min(spi_max(0, 100 + humid_factor + temp_factor + rain_factor), 200);

	trace(F("PNW Adjust: H(%d) T(%d) R(%d) => %d%%\n"), humid_factor, temp_factor, rain_factor, adj);
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