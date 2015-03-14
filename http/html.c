#include <stdio.h>
#include <sys/types.h> 
#include <string.h>
#include <errno.h>

#include <sys/stat.h>   //File date

#include "http.h"
#include "../global.h"
#include "../utc/utc.h"
#include "../utc/leaps.h"
#include "../lib/log.h"
#include "../lib/rtc.h"
#include "../lib/datetime.h"
#include "../lib/file.h"
#include "../sysclk/sysclk.h"
#include "../fan/fan.h"
#include "../fan/pid.h"
#include "../fan/curve.h"

#define DELIMITER '^'

static char *pb; //Buffer begin
static char *p;  //Current position
static char *pe; //Buffer end - don't actually write to this position: it's just for comparison and arithmetic

//Utilities for handling files
static void makeFullPath(char *filename, char *fullPath) { //Converts a relative file name to a full path by prepending the www folder
	
	 //prepend with 'www' and leave our local copy of fullPath pointing at the end of the string
	fullPath = stpcpy(fullPath, "/srv/rtc");
	
	//add a folder delimiter if not already there
	if (filename[0] != '/') *fullPath++ = '/';
	
	//add the filename
	stpcpy(fullPath, filename);
}

//Utilities for writing into non text area of buffer. On error: set error type and return failure; server will respond with a 4xx or 5xx response
static int  addHttpDate     (time_t t       )              {
	char sDate[30];
	struct tm tm;
	if (DateTimeTtoTm(t, &tm))
	{
		HttpErrorType = 500; //Internal Server Error
		sprintf(HttpErrorMsg, "ERROR Could not convert time; see log");
		return -1;
	}
	int length = strftime(sDate, 30, "%a, %e %b %Y %H:%M:%S GMT", &tm);
	if (length < 0)
	{
		HttpErrorType = 500; //Internal Server Error
		sprintf(HttpErrorMsg, "ERROR Could not format time");
		return -1;
	}
	p = stpcpy(p, sDate);
	return 0;
}
static int  addBinaryFile   ( char *filename)              {
	FILE *fp = fopen(filename, "r");
	if (fp == NULL)
	{
		HttpErrorType = 500; //Internal Server Error
		sprintf(HttpErrorMsg, "ERROR Could not open binary file %s", filename);
		return -1;
	}
	
	while (1)
	{
		if (p >= pe - 1000)
		{
			p = stpcpy(p, "ERROR Buffer is full in addBinaryFile ");
			p = stpcpy(p, filename);
			return 0;
		}
		int c = fgetc(fp);
		if (c == EOF) break;
		*p++ = (char)c;
	}
		
	fclose(fp);
	return 0;
}

//Utilities for writing into buffer text part; any error is printed directly into the text so server will still respond with a 200 success response
static void addTabs         (   int tabs                   ) { //Adds tabs
	for(int i = 0; i < tabs; i++) *p++ = '\t';
}
static void addAsciiFile    (                  char *file  ) { //Adds the content of the file
	FILE *fp = fopen(file, "r");
	if (fp == NULL)
	{
		p = stpcpy(p, "ERROR Could not open for read ");
		p = stpcpy(p, file);
		p = stpcpy(p, " ");
		p = stpcpy(p, strerror(errno));
		return;
	}
	while (1)
	{
		if (p >= pe - 1000)
		{
			p = stpcpy(p, "ERROR Buffer is full in addAsciiFile ");
			p = stpcpy(p, file);
			return;
		}
		int c = fgetc(fp);
		if (c == EOF) break;
        *p++ = (char)c;
	}

	fclose(fp);
}
static void addAsciiFileRows(                  char *file  ) { //Adds a character for the number of rows needed to edit an ascii file
	FILE *fp = fopen(file, "r");
	if (fp == NULL)
	{
		*p++ = '1'; //Just return '1' if cannot determine length
		return;
	}
	int lines = 0;
	while (1)
	{
		int c = fgetc(fp);
		if (c == EOF) break;
        if (c == '\n') lines++;
	}
	fclose(fp);
	sprintf(p, "%d", lines + 1);
	while(*p != 0) p++; //Advance p to the string terminator
	return;
}
static void addString       (                  char *value ) { //Adds a string
	p = stpcpy(p, value);
}
static void addTime         ( char *format,   time_t value ) { //Adds a formatted date
	struct tm tm;
	if (DateTimeTtoTm(value, &tm))
	{
		p = stpcpy(p, "ERROR Could not convert date");
		return;
	}
	int length = strftime(p, 100, format, &tm);
	if (length < 0)
	{
		p = stpcpy(p, "ERROR Could not print date");
	}
	else
	{
		p += length;
	}	
}
static void addBoolean      ( char *format,     char value ) { //Adds a boolean displayed as truetext or falsetext from a format of truetext/falsetext eg 'Y/N' or 'checked'
	char *i = format;
	char isFalseText = 0;
	while (1)
	{
		if (*i == 0) break;
		if (!isFalseText && *i == '/')
		{
			if (value) break;
			isFalseText = 1;
			i++;
			continue;
		}
		if ( value && !isFalseText) *p++ = *i;
		if (!value &&  isFalseText) *p++ = *i;
		i++;
	}
}
static void addFloat        ( char *format,    float value ) { //Adds a formatted float
	int length = sprintf(p, format, value);
	if (length < 0)
	{
		p = stpcpy(p, "ERROR Could not print float");
	}
	else
	{
		p += length;
	}
}
static void addDouble       ( char *format,   double value ) { //Adds a formatted double
	int length = sprintf(p, format, value);
	if (length < 0)
	{
		p = stpcpy(p, "ERROR Could not print double");
	}
	else
	{
		p += length;
	}
}
static void addSint16       ( char *format,  int16_t value ) { //Adds a formatted signed integer
	int length = sprintf(p, format, value);
	if (length < 0)
	{
		p = stpcpy(p, "ERROR Could not print integer ");
	}
	else
	{
		p += length;
	}
}
static void addUint16       ( char *format, uint16_t value ) { //Adds a formatted unsigned integer
	int length = sprintf(p, format, value);
	if (length < 0)
	{
		p = stpcpy(p, "ERROR Could not print integer ");
	}
	else
	{
		p += length;
	}
}
static void addSint32       ( char *format,  int32_t value ) { //Adds a formatted signed long
	int length = sprintf(p, format, value);
	if (length < 0)
	{
		p = stpcpy(p, "ERROR Could not print integer ");
	}
	else
	{
		p += length;
	}
}
static void addUint32       ( char *format, uint32_t value ) { //Adds a formatted unsigned long
	int length = sprintf(p, format, value);
	if (length < 0)
	{
		p = stpcpy(p, "ERROR Could not print integer ");
	}
	else
	{
		p += length;
	}
}
static void addSint64       ( char *format,  int64_t value ) { //Adds a formatted signed long long
	int length = sprintf(p, format, value);
	if (length < 0)
	{
		p = stpcpy(p, "ERROR Could not print integer ");
	}
	else
	{
		p += length;
	}
}
static void addUint64       ( char *format, uint64_t value ) { //Adds a formatted unsigned long long
	int length = sprintf(p, format, value);
	if (length < 0)
	{
		p = stpcpy(p, "ERROR Could not print integer ");
	}
	else
	{
		p += length;
	}
}

//Include value content in html. Returns 1 if found, 0 if not
static int includeValueString (char * item, char * compare, int(*getValueFunction)(int, char *), char * format) {
	if (strcmp(item, compare) != 0) return 0;
	char value[128];
	if (getValueFunction(128, value))
	{
		p = stpcpy(p, "ERROR Could not read ");
		p = stpcpy(p, item);
		return 1;
	}
	addString(value);
	return 1;
}
static int includeValueTime   (char * item, char * compare, int(*getValueFunction)(   time_t *), char * format) {
	if (strcmp(item, compare) != 0) return 0;
	time_t value;
	if (getValueFunction(&value))
	{
		p = stpcpy(p, "ERROR Could not read ");
		p = stpcpy(p, item);
		return 1;
	}
	addTime(format, value);
	return 1;
}
static int includeValueBoolean(char * item, char * compare, int(*getValueFunction)(     char *), char * format) {
	if (strcmp(item, compare) != 0) return 0;
	char value;
	if (getValueFunction(&value))
	{
		p = stpcpy(p, "ERROR Could not read ");
		p = stpcpy(p, item);
		return 1;
	}
	addBoolean(format, value);
	return 1;
}
static int includeValueFloat  (char * item, char * compare, int(*getValueFunction)(    float *), char * format) {
	if (strcmp(item, compare) != 0) return 0;
	float value;
	if (getValueFunction(&value))
	{
		p = stpcpy(p, "ERROR Could not read ");
		p = stpcpy(p, item);
		return 1;
	}
	addFloat(format, value);
	return 1;
}
static int includeValueDouble (char * item, char * compare, int(*getValueFunction)(   double *), char * format) {
	if (strcmp(item, compare) != 0) return 0;
	double value;
	if (getValueFunction(&value))
	{
		p = stpcpy(p, "ERROR Could not read ");
		p = stpcpy(p, item);
		return 1;
	}
	addDouble(format, value);
	return 1;
}
static int includeValueSint16 (char * item, char * compare, int(*getValueFunction)(  int16_t *), char * format) {
	if (strcmp(item, compare) != 0) return 0;
	int16_t value;
	if (getValueFunction(&value))
	{
		p = stpcpy(p, "ERROR Could not read ");
		p = stpcpy(p, item);
		return 1;
	}
	addSint16(format, value);
	return 1;
}
static int includeValueUint16 (char * item, char * compare, int(*getValueFunction)( uint16_t *), char * format) {
	if (strcmp(item, compare) != 0) return 0;
	uint16_t value;
	if (getValueFunction(&value))
	{
		p = stpcpy(p, "ERROR Could not read ");
		p = stpcpy(p, item);
		return 1;
	}
	addUint16(format, value);
	return 1;
}
static int includeValueSint32 (char * item, char * compare, int(*getValueFunction)(  int32_t *), char * format) {
	if (strcmp(item, compare) != 0) return 0;
	int32_t value;
	if (getValueFunction(&value))
	{
		p = stpcpy(p, "ERROR Could not read ");
		p = stpcpy(p, item);
		return 1;
	}
	addSint32(format, value);
	return 1;
}
static int includeValueUint32 (char * item, char * compare, int(*getValueFunction)( uint32_t *), char * format) {
	if (strcmp(item, compare) != 0) return 0;
	uint32_t value;
	if (getValueFunction(&value))
	{
		p = stpcpy(p, "ERROR Could not read ");
		p = stpcpy(p, item);
		return 1;
	}
	addUint32(format, value);
	return 1;
}
static int includeValueSint64 (char * item, char * compare, int(*getValueFunction)(  int64_t *), char * format) {
	if (strcmp(item, compare) != 0) return 0;
	int64_t value;
	if (getValueFunction(&value))
	{
		p = stpcpy(p, "ERROR Could not read ");
		p = stpcpy(p, item);
		return 1;
	}
	addSint64(format, value);
	return 1;
}
static int includeValueUint64 (char * item, char * compare, int(*getValueFunction)( uint64_t *), char * format) {
	if (strcmp(item, compare) != 0) return 0;
	uint64_t value;
	if (getValueFunction(&value))
	{
		p = stpcpy(p, "ERROR Could not read ");
		p = stpcpy(p, item);
		return 1;
	}
	addUint64(format, value);
	return 1;
}
static int includeValueClock  (char * item, char * compare, int(*getValueFunction)( uint64_t *), char * format) {
	if (strcmp(item, compare) != 0) return 0;
	uint64_t rtc;
	if (getValueFunction(&rtc))
	{
		p = stpcpy(p, "ERROR Could not read ");
		p = stpcpy(p, item);
		return 1;
	}
	uint64_t tai;
	if (RtcToTicks(rtc, UtcEpoch, UtcTicksPerSec, &tai)) return -1;
	uint64_t utc;
	char isLeap;
	if (TaiToUtc(tai, &utc, &isLeap)) return -1;
	
	char time[128];
	if (UtcLeapToF(format, utc, isLeap, 128, time))
	{
		p = stpcpy(p, "ERROR Could not convert UtcLeapToF ");
		p = stpcpy(p, item);
		return 1;
	}
	
	addString(time);
	return 1;
}

//Include file content in html
static  int include                  (int tabs, char *item );  //Declaration required to allow recursive sheet addition
static void includeRtcNowUtcMs1970   (                     ) { // ~utcMs1970~               Adds the unix time
	uint64_t tai;
	if (RtcGetNowTicks(UtcEpoch, UtcTicksPerSec, &tai))
	{
		p = stpcpy(p, "ERROR includeUtcMs1970 - Could not read time");
		return;
	}
	uint64_t utc;
	char isLeap;
	if (TaiToUtc(tai, &utc, &isLeap))
	{
		p = stpcpy(p, "ERROR includeUtcMs1970 - Could not convert tai to utc");
		return;
	}
	uint64_t ms1970;
	if (UtcToTicks(utc, 1970, 1000, &ms1970))
	{
		p = stpcpy(p, "ERROR includeUtcMs1970 - Could not convert utc to ms 1970");
		return;
	}
	
	int length = sprintf(p, "%llu", ms1970);
	if (length < 0)
	{
		p = stpcpy(p, "ERROR includeUtcMs1970 - Could not print integer ");
		return;
	}
	p += length;
}
static void includeLeapsTop1970      (                     ) { // ~nextleap~                Adds the top leap
	uint64_t topLeapUtc;
	uint32_t topLeapCount;
	if (LeapsFetchTop(&topLeapUtc, &topLeapCount))
	{
		p = stpcpy(p, "ERROR includeTopLeapUtc - Could not read top leap");
		return;
	}
	uint64_t topLeapUtc1970 = topLeapUtc - SECONDS_1900_1970;
	int length = sprintf(p, "%llu", topLeapUtc1970);
	if (length < 0)
	{
		p = stpcpy(p, "ERROR includeTopLeapUtc - Could not print integer ");
		return;
	}
	p += length;
}
static void includeLeapsDisplayLast  (                     ) { // ~lastleap~                Adds the 5 seconds before and after the last leap in leapseconds
	p += LeapsDisplayLast(p);
}
static void includeErrorFlag         (          char *format) {
	uint32_t rtcError;
	if (RtcGetError(&rtcError))
	{
		p = stpcpy(p, "ERROR Could not read RtcGetError");
		return;
	}
	char flag = rtcError > 0;
	addBoolean(format, flag);
}
static void includeAmbientDiff       (          char *format) {
	float ambient;
	float actual;
	if (RtcGetAmbient(&ambient))
	{
		p = stpcpy(p, "ERROR Could not read RtcGetAmbient");
		return;
	}
	if (RtcGetTemperature(&actual))
	{
		p = stpcpy(p, "ERROR Could not read RtcGetTemperature");
		return;
	}
	addFloat(format, actual - ambient);
}
static void includeSheet             (          char *value) { //    ~sheet^sheetname~      Adds the current sheet, for example 'RTC'
	p = stpcpy(p, HttpSheet);
}
static void includeEdit              (int tabs, char *value) { //     ~edit^filename^title~ Adds the file wrapped in a textarea in form ready to POST
	char *filename = value;
	char *title;
	char *i = value;
	while (1)
	{
		if (*i == 0)
		{
			p = stpcpy(p, "ERROR Could not parse value ");
			p = stpcpy(p, value);
			return;
		}
		if (*i == DELIMITER)
		{
			*i = 0;
			title = i + 1;
			break;
		}
		i++;
	}
					p = stpcpy(p, "<h1>");
					p = stpcpy(p, title);
					p = stpcpy(p, "</h1>\r\n");
					
	addTabs(tabs);	p = stpcpy(p, "<form action='/' method='post'>\r\n");
	
	addTabs(tabs);	p = stpcpy(p, "\t<input type='submit' value='Submit'>\r\n");
	addTabs(tabs);	p = stpcpy(p, "\t<input type='button' value='Clear' onclick='this.form.content.value=\"\"'><br/>\r\n");

	addTabs(tabs);	p = stpcpy(p, "\t<input type='hidden' name='edit' value='");
					p = stpcpy(p, filename); p = stpcpy(p, "'>\n\r");
										
	addTabs(tabs);	p = stpcpy(p, "\t<textarea name='content' style='width:100%' rows='");
					addAsciiFileRows(filename);
					p = stpcpy(p, "'>");
					addAsciiFile(filename);
					p = stpcpy(p, "</textarea>\r\n");
	
	addTabs(tabs);	p = stpcpy(p, "</form>");
}
static  int includeBody              (int tabs, char *file ) { //     ~body^filename~       Adds the file or sets a failure response. Parses file for includes.

	//Work out the root for the file
	char fullPath[20];
	makeFullPath(file, fullPath);
	
	FILE *fp = fopen(fullPath, "r");
	if (fp == NULL)
	{
		HttpErrorType = 500; //Internal Server Error
		sprintf(HttpErrorMsg, "Could not include file %s", fullPath);
		return -1;
	}
	
	int newtabs = 0;
	char name[100];
	int  n = 0;
	char isName = 0;
	while (1)
	{
		int c = fgetc(fp);
		if (c == EOF) break;
		if (c ==   0) continue;
		if (c == '\n') newtabs = 0;
		if (c == '\t') newtabs++;
		if (c == '~')
		{
			if (isName)
			{
				name[n] = 0; //Terminate name string
				n = 0;       //Reset the start of name
				include(newtabs + tabs, name);
			}
			isName = !isName;
			continue;
		}
		if (isName)  name[n++] = (char)c;
		else              *p++ = (char)c;
	}

	fclose(fp);
	return 0;
}
static  int include                  (int tabs, char *item ) { // Called by includeFile
	char *name  = item;
	char *value = item;
	while (1)
	{
		if (*value == DELIMITER) { *value = 0; value++; break; } //Has value
		if (*value ==        0 ) {  value = NULL      ; break; } //No value attached
		value++;
	}

	//Empty
	if (strcmp             (name, ""                   ) == 0) { p = stpcpy(p, "~"); return 0; } //An empty include '~~' returns a single '~'
	
	//Recursive
	if (strcmp             (name, "body"               ) == 0) return includeBody(tabs, HttpSheet); //Recursively add content
	
	//General
	if (strcmp             (name, "edit"               ) == 0) { includeEdit               (tabs, value); return 0; }
	if (strcmp             (name, "sheet"              ) == 0) { includeSheet              (      value); return 0; }
	if (strcmp             (name, "file"               ) == 0) { addAsciiFile              (      value); return 0; }
	if (strcmp             (name, "filerows"           ) == 0) { addAsciiFileRows          (      value); return 0; }
	
	//Leaps
	if (includeValueTime   (name, "LeapsCacheFileTime"         , &LeapsGetCacheFileTime         , value)) return 0;
	if (strcmp             (name, "LeapsDisplayLast"   ) == 0) { includeLeapsDisplayLast              (); return 0; }
	if (strcmp             (name, "LeapsTopSecond1970" ) == 0) { includeLeapsTop1970                  (); return 0; }
	
	//Pi local
	if (strcmp             (name, "SysClkOffsetSeconds") == 0) { addDouble  (value, SysClkOffsetSeconds); return 0; }
	if (strcmp             (name, "SysClkFrequencyPpm" ) == 0) { addDouble  (value, SysClkFrequencyPpm ); return 0; }
	if (includeValueFloat  (name, "FanTarget"                  , &PidGetTarget                  , value)) return 0;
	if (includeValueFloat  (name, "FanPidIn"                   , &PidGetInput                   , value)) return 0;
	if (includeValueFloat  (name, "FanPidOut"                  , &PidGetOutput                  , value)) return 0;
	if (includeValueFloat  (name, "FanHeating"                 , &PidGetHeating                 , value)) return 0;
	if (includeValueBoolean(name, "FanAuto"                    , &PidGetAuto                    , value)) return 0;
	if (includeValueBoolean(name, "FanStall"                   , &PidGetWarning                 , value)) return 0;
	if (includeValueBoolean(name, "FanLocked"                  , &PidGetLocked                  , value)) return 0;
	if (includeValueFloat  (name, "FanStepAmount"              , &PidGetStepAmount              , value)) return 0;
	if (includeValueBoolean(name, "FanStepDirection"           , &PidGetStepDirection           , value)) return 0;
	if (includeValueBoolean(name, "FanStepAuto"                , &PidGetStepAuto                , value)) return 0;

	if (includeValueUint16 (name, "FanOutput"                  , &FanGetOutput                  , value)) return 0; 
	if (includeValueUint16 (name, "FanDensity"                 , &FanGetDensity                 , value)) return 0;
	if (includeValueSint32 (name, "FanOnTime"                  , &FanGetOnTime                  , value)) return 0;
	if (includeValueSint32 (name, "FanOffTime"                 , &FanGetOffTime                 , value)) return 0;
	if (includeValueSint32 (name, "FanPulse"                   , &FanGetPulse                   , value)) return 0;
	
	if (strcmp             (name, "FanCurvePoints"     ) == 0) { CurveAddPointsForHtmlChart         (&p); return 0; }
	
	//General
	if (includeValueUint32 (name, "RtcError"                   , &RtcGetError                   , value)) return 0;
	if (strcmp             (name, "RtcFlag"            ) == 0) { includeErrorFlag                (value); return 0; }
	if (includeValueString (name, "RtcBuildInfo"               , &RtcGetBuildInfo               , value)) return 0;
	if (includeValueString (name, "RtcModelInfo"               , &RtcGetModelInfo               , value)) return 0;
	if (includeValueString (name, "RtcMakerInfo"               , &RtcGetMakerInfo               , value)) return 0;
	if (includeValueFloat  (name, "RtcTemperature"             , &RtcGetTemperature             , value)) return 0;
	if (includeValueFloat  (name, "RtcAmbient"                 , &RtcGetAmbient                 , value)) return 0;
	if (strcmp             (name, "RtcAmbientDiff"     ) == 0) { includeAmbientDiff              (value); return 0; }
	if (includeValueFloat  (name, "RtcVoltage"                 , &RtcGetVoltage                 , value)) return 0;
	if (includeValueBoolean(name, "RtcLED"                     , &RtcGetLED                     , value)) return 0;
	if (includeValueSint64 (name, "RtcDebug0"                  , &RtcGetDebug0                  , value)) return 0;
	if (includeValueSint64 (name, "RtcDebug1"                  , &RtcGetDebug1                  , value)) return 0;
	if (includeValueSint64 (name, "RtcDebug2"                  , &RtcGetDebug2                  , value)) return 0;
	if (includeValueSint64 (name, "RtcDebug3"                  , &RtcGetDebug3                  , value)) return 0;
	if (includeValueString (name, "RtcComment"                 , &RtcGetComment                 , value)) return 0;
	if (includeValueUint32 (name, "RtcPollPeriod"              , &RtcGetPollPeriod              , value)) return 0;
	
	//Aging
	if (includeValueSint32 (name, "RtcAgingParamFo"            , &RtcGetAgingParamFo            , value)) return 0;
	if (includeValueSint32 (name, "RtcAgingParamAo"            , &RtcGetAgingParamAo            , value)) return 0;
	
	//Regulator
	if (includeValueFloat  (name, "RtcRegulatorParamTo"        , &RtcGetRegulatorParamTo        , value)) return 0;
	if (includeValueFloat  (name, "RtcRegulatorParamKo"        , &RtcGetRegulatorParamKo        , value)) return 0;
	if (includeValueSint32 (name, "RtcRegulatorOutputPpb"      , &RtcGetRegulatorOutputPpb      , value)) return 0;
	
	//Slew
	if (includeValueSint32 (name, "RtcSlewAmountNs"            , &RtcGetSlewAmountNs            , value)) return 0;
	if (includeValueSint32 (name, "RtcSlewRateNsPerS"          , &RtcGetSlewRateNsPerS          , value)) return 0;

	//Clock
	if (includeValueBoolean(name, "RtcClockIsSet"              , &RtcGetClockIsSet              , value)) return 0;
	if (includeValueUint16 (name, "RtcClockEpoch"              , &RtcGetClockEpoch              , value)) return 0;
	if (includeValueUint64 (name, "RtcClockNowTicNs"           , &RtcGetClockNowTicNs           , value)) return 0;
	if (includeValueUint64 (name, "RtcClockNowIntNs"           , &RtcGetClockNowIntNs           , value)) return 0;
	if (includeValueUint64 (name, "RtcClockNowAbsNs"           , &RtcGetClockNowAbsNs           , value)) return 0;

	//Sample
	if (includeValueUint64 (name, "RtcSampleThisTicNs"         , &RtcGetSampleThisTicNs         , value)) return 0;
	if (includeValueUint64 (name, "RtcSampleThisIntNs"         , &RtcGetSampleThisIntNs         , value)) return 0;
	if (includeValueUint64 (name, "RtcSampleThisAbsNs"         , &RtcGetSampleThisAbsNs         , value)) return 0;
	if (includeValueUint64 (name, "RtcSampleThisExtNs"         , &RtcGetSampleThisExtNs         , value)) return 0;
	if (includeValueUint64 (name, "RtcSampleNextTicNs"         , &RtcGetSampleLastTicNs         , value)) return 0;
	if (includeValueUint64 (name, "RtcSampleNextIntNs"         , &RtcGetSampleLastIntNs         , value)) return 0;
	if (includeValueUint64 (name, "RtcSampleNextAbsNs"         , &RtcGetSampleLastAbsNs         , value)) return 0;
	if (includeValueUint64 (name, "RtcSampleNextExtNs"         , &RtcGetSampleLastExtNs         , value)) return 0;
	if (includeValueBoolean(name, "RtcSampleTreated"           , &RtcGetSampleTreated           , value)) return 0;
	if (includeValueBoolean(name, "RtcSampleControlOn"         , &RtcGetSampleControlOn         , value)) return 0;

	//Rate loop
	if (includeValueSint16 (name, "RtcRateLoopParamKr"         , &RtcGetRateLoopParamKr         , value)) return 0;
	if (includeValueSint32 (name, "RtcRateLoopParamMax"        , &RtcGetRateLoopParamMax        , value)) return 0;
	if (includeValueSint64 (name, "RtcRateLoopInputDiffPpb"    , &RtcGetRateLoopInputDiffPpb    , value)) return 0;
	if (includeValueUint64 (name, "RtcRateLoopInputPeriodNs"   , &RtcGetRateLoopInputPeriodNs   , value)) return 0;
	if (includeValueSint32 (name, "RtcRateLoopOutputPpb"       , &RtcGetRateLoopOutputPpb       , value)) return 0;

	//Time loop
	if (includeValueSint16 (name, "RtcTimeLoopParamKa"         , &RtcGetTimeLoopParamKa         , value)) return 0;
	if (includeValueSint32 (name, "RtcTimeLoopParamMax"        , &RtcGetTimeLoopParamMax        , value)) return 0;
	if (includeValueSint32 (name, "RtcTimeLoopParamSpreadSecs" , &RtcGetTimeLoopParamSpreadSecs , value)) return 0;
	if (includeValueSint64 (name, "RtcTimeLoopInputAbsDiffNs"  , &RtcGetTimeLoopInputAbsDiffNs  , value)) return 0;
	if (includeValueSint32 (name, "RtcTimeLoopOutputAmountNs"  , &RtcGetTimeLoopOutputAmountNs  , value)) return 0;
	if (includeValueSint32 (name, "RtcTimeLoopOutputRateNsPerS", &RtcGetTimeLoopOutputRateNsPerS, value)) return 0;
	
	//String time
	if (includeValueClock  (name, "RtcNowF"                    , &RtcGetClockNowAbsNs           , value)) return 0;
	if (includeValueClock  (name, "RtcSampleF"                 , &RtcGetSampleThisExtNs         , value)) return 0;
	if (strcmp             (name, "RtcNowUtcMs1970"    ) == 0) { includeRtcNowUtcMs1970               (); return 0; }
	
	
	//Handle name which could not be found
	p = stpcpy(p, "ERROR Could not find ");
	p = stpcpy(p, name);
	return 0;
}

//Add response headers
static int addResponseHeaderDate        (void) {
	p = stpcpy(p,"Date: ");
	time_t t;
	if (DateTimeNowT(&t)) return -1;
	if (addHttpDate(t)) return -1;
	p = stpcpy(p, "\r\n");
	return 0;
}
static int addResponseHeaderLastModified(time_t t) {
	p = stpcpy(p,"Last-Modified: ");
	if (addHttpDate(t)) return -1;
	p = stpcpy(p, "\r\n");
	return 0;
}

//Send response
static int sendAjax    (char *pStart, int bufferSize, char *query, char isHead) {
	pb = pStart;
	p = pb;

	p = stpcpy(p, "HTTP/1.1 200 OK\r\n");
	if (addResponseHeaderDate()) return -1;
	p = stpcpy(p, "Connection: close\r\n");
	p = stpcpy(p, "Content-Type: text\r\n");
	p = stpcpy(p, "\r\n");
	*p = 0;
	if (isHead) return p - pb; //Don't add the body if this is a HEAD request

	char name[100];
	int  n = 0;
	char isName = 0;
	while (1)
	{
		char c = *query++;
		if (c == 0 || c == '~')
		{
			if (isName)
			{
				name[n] = 0; //Terminate name string
				n = 0;       //Reset the start of name
				include(0, name);
			}
			isName = !isName;
			if (c == 0 ) break;
			continue;
		}
		if (isName)  name[n++] = c;
		else              *p++ = c;
	}

	return p - pb;

}
static int sendFile    (char *pStart, int bufferSize, char *resource, char isHead) {

	pb = pStart;
	p = pb;
	
	//Work out the root for the file
	char fullPath[50];
	makeFullPath(resource, fullPath);
	
	//Add the header
	p = stpcpy(p, "HTTP/1.1 200 OK\r\n");
	if (addResponseHeaderDate()) return -1;
	p = stpcpy(p, "Connection: close\r\n");
	
	//Add the content type
	p = stpcpy(p, "Content-Type: ");
  	char *ext = PathExt(resource); //This is never null but it could be an empty string.
	     if (strcmp(ext, "ico" ) == 0) p = stpcpy(p, "image/x-icon"          );
	else if (strcmp(ext, "jpg" ) == 0) p = stpcpy(p, "image/jpeg"            );
	else if (strcmp(ext, "gif" ) == 0) p = stpcpy(p, "image/gif"             );
	else if (strcmp(ext, "png" ) == 0) p = stpcpy(p, "image/png"             );
	else if (strcmp(ext, "html") == 0) p = stpcpy(p, "text/html"             );
	else if (strcmp(ext, "css" ) == 0) p = stpcpy(p, "text/css"              );
	else if (strcmp(ext, "js"  ) == 0) p = stpcpy(p, "application/javascript");
	else if (strcmp(ext, "txt" ) == 0) p = stpcpy(p, "text/plain"            );
	else
	{
		HttpErrorType = 500; // Internal Server Error
		sprintf(HttpErrorMsg, "Could not fetch '%s' as its content type is not supported.", resource);
		return -1;
	}
	p = stpcpy(p, "\r\n");
	
	struct stat filestat;
	if (stat(fullPath, &filestat))
	{
		HttpErrorType = 404; // Not Found
		sprintf(HttpErrorMsg, "Could not find requested resource '%s'", resource);
		return -1;
	}
	if (addResponseHeaderLastModified(filestat.st_mtime)) return -1;
	p = stpcpy(p, "\r\n");
	*p = 0;
	if (!isHead) //Don't add the body if this is a HEAD request
	{
		if (addBinaryFile(fullPath)) return -1;
	}
	return p - pb;
}
static int sendIndex   (char *pStart, int bufferSize, char *resource, char isHead) {			
	pb = pStart;
	p = pb;

	p = stpcpy(p, "HTTP/1.1 200 OK\r\n");
	if (addResponseHeaderDate()) return -1;
	p = stpcpy(p, "Connection: close\r\n");
	p = stpcpy(p, "Content-Type: text/html\r\n");
	p = stpcpy(p, "\r\n");
	*p = 0;
	if (!isHead) //Dont add the body if this is a HEAD request
	{
		if (includeBody(0, resource)) return -1;
	}
	return p - pb;
}
int HtmlSend           (char *pStart, int bufferSize, char *resource, char isHead) {

	//Extract the query part
	char *query = resource;
	while ( *query && *query != '?') query++;
	if (*query)
	{
		*query = 0; //End the resource with a null
		query++;    //Put the query to the start
	}
	
	if (strcmp(resource, "/"    ) == 0) return sendIndex(pStart, bufferSize, "index" , isHead);
	if (strcmp(resource, "/ajax") == 0) return sendAjax (pStart, bufferSize, query   , isHead);
	                                    return sendFile (pStart, bufferSize, resource, isHead);
}
int HtmlSendError      (char *pStart, int bufferSize) {
	//Build the buffer
	pb = pStart;
	p = pb;
	p = stpcpy(p, "HTTP/1.1 ");
	switch (HttpErrorType)
	{
		case 400:
			p = stpcpy(p, "400 Bad Request");
			break;
		case 404:
			p = stpcpy(p, "404 Not Found");
			break;
		case 500:
			p = stpcpy(p, "500 Internal Server Error");
			break;
		case 501:
			p = stpcpy(p, "501 Not Implemented");
			break;
		default:
			Log ('e', "Asked to send an unknown error response %d", HttpErrorType);
			return -1;
	}
	p = stpcpy(p, "\r\n");
	if (addResponseHeaderDate()) return -1;
	p = stpcpy(p, "Connection: close\r\n");
	p = stpcpy(p, "Content-Type: text/plain\r\n");
	p = stpcpy(p, "\r\n");
	p = stpcpy(p, HttpErrorMsg);
	*p = 0;
	return p - pb;
}
