// Logging.cpp
// This manages the Logging to SQL for the Sprinkling System.
// Author: Richard Zimmerman
// Copyright (c) 2013 Richard Zimmerman
//

#define __STDC_FORMAT_MACROS
#include "Logging.h"
#include "port.h"
#include "settings.h"
#include <sqlite3.h>
#include <string.h>
#include <cstdlib>
#include "Weather.h"  // <== Make sure this is at the top of Logging.cpp

Logging::Logging()
		: m_db(0)
{

}

Logging::~Logging()
{
	Close();
}

static int GetDBVersion(sqlite3 * db)
{
	int ret_val = 0;
	sqlite3_stmt * statement;
	// Now determine if we're running V1.0 of the schema.
	int res = sqlite3_prepare_v2(db, "SELECT MAX(version) FROM versions;", -1, &statement, NULL);
	if (res)
	{
		trace("Prepare Failure (%s)\n", sqlite3_errmsg(db));
		return 0;
	}

	res = sqlite3_step(statement);
	if (res == SQLITE_ROW)
	{
		ret_val = sqlite3_column_int(statement, 0);
	}

	sqlite3_finalize(statement);
	return ret_val;
}

static bool CreateSchema(sqlite3 * db)
{
	char * zErrMsg = 0;
	if (sqlite3_exec(db,
			"DROP TABLE IF EXISTS versions; DROP TABLE IF EXISTS zonelog; CREATE TABLE versions (version INT);"
			"INSERT INTO versions VALUES (2);CREATE TABLE zonelog(date INTEGER, zone INTEGER, duration INTEGER, schedule INTEGER,"
			"seasonal INTEGER, wunderground INTEGER);",
			NULL, NULL, &zErrMsg) != SQLITE_OK)
	{
		trace("SQL Error (%s)\n", zErrMsg);
		sqlite3_free(zErrMsg);
		return false;
	}
	return true;
}

static bool UpdateV1toV2(sqlite3 * db)
{
	char * zErrMsg = 0;
	if (sqlite3_exec(db,
			"begin;DROP TABLE IF EXISTS tmp_zonelog; CREATE TABLE tmp_zonelog(date INTEGER, zone INTEGER, duration INTEGER, schedule INTEGER,"
			"seasonal INTEGER, wunderground INTEGER);INSERT INTO tmp_zonelog SELECT date, zone, duration, schedule, -1, -1 from zonelog;"
			"DROP TABLE zonelog; ALTER TABLE tmp_zonelog rename to zonelog;INSERT INTO versions VALUES (2);commit;",
			NULL, NULL, &zErrMsg) != SQLITE_OK)
	{
		trace("SQL Error (%s)\n", zErrMsg);
		sqlite3_free(zErrMsg);
		return false;
	}
	return true;
}

bool Logging::Init()
{
	int rc = sqlite3_open("db.sql", &m_db);
	if (rc)
	{
		trace("Can't open Database (%s)\n", sqlite3_errmsg(m_db));
		Close();
		return false;
	}

	int version = GetDBVersion(m_db);
	if (version == 0)
		CreateSchema(m_db);
	else if (version == 1)
		UpdateV1toV2(m_db);
	else if (version != 2)
		CreateSchema(m_db);

	return true;
}

void Logging::Close()
{
	if (m_db)
	{
		sqlite3_close(m_db);
		m_db = 0;
	}
}

bool Logging::LogZoneEvent(time_t start, int zone, int duration, int schedule, int sadj, int wunderground)
{
	// Insert into SQLite log table
	char* zErrMsg = 0;
	char sSQL[100];
	snprintf(sSQL, sizeof(sSQL),
		"INSERT INTO zonelog VALUES(%ld, %d, %d, %d, %d, %d);",
		start, zone, duration, schedule, sadj, wunderground);
	sSQL[sizeof(sSQL) - 1] = '\0';

	if (sqlite3_exec(m_db, sSQL, NULL, NULL, &zErrMsg) != SQLITE_OK) {
		trace("SQL Error (%s)\n", zErrMsg);
		sqlite3_free(zErrMsg);
		return false;
	}

	// Log to NTFY
	trace(F("Logging Zone Event: zone %d, schedule %d, duration %d\n"), zone, schedule, duration);

	// Load Zone name
	const char* zoneName = "Unknown";
	if (zone > 0 && zone <= NUM_ZONES) {
		FullZone z;
		LoadZone(zone - 1, &z);
		zoneName = z.name;
		trace(F("Loaded Zone name: %s\n"), zoneName);
	} else {
		trace(F("Zone index %d out of range. Using fallback name.\n"), zone);
	}

	// Get schedule name
	Schedule sched;
	char scheduleName[sizeof(sched.name)] = "Unnamed Schedule";

	if (schedule <= 0 || schedule > MAX_SCHEDULES) {
			trace(F("Schedule index %d out of range.\n"), schedule);
			strcpy(scheduleName, "Invalid Schedule");
	} else {
			LoadSchedule(schedule - 1, &sched);
			strncpy(scheduleName, sched.name, sizeof(sched.name));
			trace(F("Loaded Schedule name: %s\n"), scheduleName);
	}

	// Send to NTFY
	char cmd[512];
	snprintf(cmd, sizeof(cmd),
		"curl -H \"Authorization: Bearer tk_jjldqavvt53ik5zm5pg86c928olen\" "
		"-H \"Title: Zone %s ended\" "
		"-d \"Zone %s (%d sec, Schedule %d: %s)\" "
		"https://ntfy.interbiznw.dev/Irrigation",
		zoneName, zoneName, duration, schedule + 1, schedName);  // schedule + 1 is to match user-facing numbering

	system(cmd);

	return true;
}




static void DumpData(FILE * stream_file, uint32_t bin_data[], uint32_t bins, uint32_t bin_scale, uint32_t bin_offset, bool bMil)
{
	for (uint32_t i=0; i<bins; i++)
		fprintf(stream_file, "%s[%" PRIu32 "%s, %" PRIu32 "]", (i==0)?"":",", bin_offset + i*bin_scale, bMil?"000":"", bin_data[i]);
}

bool Logging::GraphZone(FILE* stream_file, time_t start, time_t end, GROUPING grouping)
{
	if (start == 0)
		start = nntpTimeServer.LocalNow();
	end = spi_max(start,end) + 24*3600;  // add 1 day to end time.

	grouping = spi_max(NONE, spi_min(grouping, MONTHLY));
	char sSQL[200];
	uint16_t bins = 0;
	uint32_t bin_offset = 0;
	uint32_t bin_scale = 1;
	switch (grouping)
	{
	case HOURLY:
		snprintf(sSQL, sizeof(sSQL),
				"SELECT zone, strftime('%%H', date, 'unixepoch') as hour, SUM(duration)"
				" FROM zonelog WHERE date BETWEEN %lu AND %lu"
				" GROUP BY zone,hour ORDER BY zone,hour",
				start, end);
		bins = 24;
		break;
	case DAILY:
		snprintf(sSQL, sizeof(sSQL),
				"SELECT zone, strftime('%%w', date, 'unixepoch') as bucket, SUM(duration)"
				" FROM zonelog WHERE date BETWEEN %lu AND %lu"
				" GROUP BY zone,bucket ORDER BY zone,bucket",
				start, end);
		bins = 7;
		break;
	case MONTHLY:
		snprintf(sSQL, sizeof(sSQL),
				"SELECT zone, strftime('%%m', date, 'unixepoch') as bucket, SUM(duration)"
				" FROM zonelog WHERE date BETWEEN %lu AND %lu"
				" GROUP BY zone,bucket ORDER BY zone,bucket",
				start, end);
		bins = 12;
		break;
	case NONE:
		bins = 100;
		bin_offset = start;
		bin_scale = (end-start)/bins;
		snprintf(sSQL, sizeof(sSQL),
				"SELECT zone, ((date-%lu)/%" PRIu32 ") as bucket, SUM(duration)"
				" FROM zonelog WHERE date BETWEEN %lu AND %lu"
				" GROUP BY zone,bucket ORDER BY zone,bucket",
				start, bin_scale, start, end);
		break;
	}

	// Make sure we've zero terminated this string.
	sSQL[sizeof(sSQL)-1] = 0;

	trace("%s\n", sSQL);

	sqlite3_stmt * statement;
	// Now determine if we're running V1.0 of the schema.
	int res = sqlite3_prepare_v2(m_db, sSQL, -1, &statement, NULL);
	if (res)
	{
		trace("Prepare Failure (%s)\n", sqlite3_errmsg(m_db));
		return false;
	}

	int current_zone = -1;
	bool bFirstZone = true;
	uint32_t bin_data[bins];
	memset(bin_data, 0, bins*sizeof(uint32_t));
	while (sqlite3_step(statement) == SQLITE_ROW)
	{
		int zone = sqlite3_column_int(statement, 0);
		if (current_zone != zone)
		{
			current_zone = zone;
			if (!bFirstZone)
				DumpData(stream_file, bin_data, bins, bin_scale, bin_offset, grouping == NONE);
			fprintf(stream_file, "%s\t\"%d\" : [", bFirstZone?"":"],\n", zone);
			memset(bin_data, 0, bins*sizeof(uint32_t));
			bFirstZone = false;
		}
		uint32_t bin = sqlite3_column_int(statement, 1);
		uint32_t value = sqlite3_column_int(statement, 2);
		bin_data[bin] = value;
	}
	if (!bFirstZone)
	{
		DumpData(stream_file, bin_data, bins, bin_scale, bin_offset, grouping == NONE);
		fprintf(stream_file, "]\n");
	}

	sqlite3_finalize(statement);

	return true;
}

bool Logging::TableZone(FILE* stream_file, time_t start, time_t end)
{
	if (start == 0)
		start = nntpTimeServer.LocalNow();
	end = spi_max(start,end) + 24*3600;  // add 1 day to end time.
	char sSQL[200];
	snprintf(sSQL, sizeof(sSQL),
			"SELECT zone, date, duration, schedule, seasonal, wunderground"
			" FROM zonelog WHERE date BETWEEN %lu AND %lu"
			" ORDER BY zone,date",
			start, end);
	// Make sure we've zero terminated this string.
	sSQL[sizeof(sSQL)-1] = 0;

	sqlite3_stmt * statement;
	// Now determine if we're running V1.0 of the schema.
	int res = sqlite3_prepare_v2(m_db, sSQL, -1, &statement, NULL);
	if (res)
	{
		trace("Prepare Failure (%s)\n", sqlite3_errmsg(m_db));
		return false;
	}
	int current_zone=-1;
	bool bFirstRow = false;
	while (sqlite3_step(statement) == SQLITE_ROW)
	{
		int zone = sqlite3_column_int(statement, 0);
		if (current_zone != zone)
		{
			fprintf(stream_file, "%s\t\t{\n\t\t\t\"zone\" : %d,\n\t\t\t\"entries\" : [", current_zone==-1?"":"\n\t\t\t]\n\t\t},\n", zone);
			current_zone = zone;
			bFirstRow = true;
		}
		fprintf(stream_file, "%s\n\t\t\t\t{ \"date\":%ld, \"duration\":%d, \"schedule\":%d, \"seasonal\":%d, \"wunderground\":%d}",
				bFirstRow ? "":",",
				(long)sqlite3_column_int(statement, 1), sqlite3_column_int(statement, 2), sqlite3_column_int(statement, 3),
				sqlite3_column_int(statement, 4), sqlite3_column_int(statement, 5));
		bFirstRow = false;
	}
	if (current_zone!=-1)
	{
		fprintf(stream_file, "\n\t\t\t]\n\t\t}\n");
	}

	sqlite3_finalize(statement);

	return true;
}


