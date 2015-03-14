#include <stdio.h>
#include <sys/types.h> 
#include <string.h>

#include "http.h"
#include "../global.h"
#include "../lib/log.h"
#include "../lib/cfg.h"
#include "../lib/rtc.h"
#include "../lib/datetime.h"
#include "../sample/sample.h"
#include "../fan/fan.h"
#include "../fan/pid.h"
#include "../fan/curve.h"

static char *p;  //Current position
static char *pl; //Last character to be handled (set after a POST from Content-Length)

//Handle saves
static int handleValueString(char * item, char * text, char * compare, int(*setValueFunction)(  char *)) {
	if (strcmp(item, compare) != 0) return 0;
	//Log('d', "Handling string %s %s", item, text);
	if (setValueFunction(text))
	{
		HttpErrorType = 400; //Bad Request
		sprintf(HttpErrorMsg, "Handle POST - values: could not set %s", item);
		return 1;
	}
	return 1;
}
static int handleValueSint64(char * item, char * text, char * compare, int(*setValueFunction)( int64_t)) {
	if (strcmp(item, compare) != 0) return 0;
	//Log('d', "Handling sint64 %s %s", item, text);
	int64_t value;
	if (sscanf(text, "%'lld",  &value) <= 0)
	{
		HttpErrorType = 400; //Bad Request
		sprintf(HttpErrorMsg, "Handle POST - values: could not parse '%s'", text);
		return 1;
	}
	if (setValueFunction(value))
	{
		HttpErrorType = 400; //Bad Request
		sprintf(HttpErrorMsg, "Handle POST - values: could not set %s", item);
		return 1;
	}
	return 1;
}
static int handleValueSint32(char * item, char * text, char * compare, int(*setValueFunction)( int32_t)) {
	if (strcmp(item, compare) != 0) return 0;
	//Log('d', "Handling sint32 %s %s", item, text);
	long int value;
	if (sscanf(text, "%'ld",  &value) <= 0)
	{
		HttpErrorType = 400; //Bad Request
		sprintf(HttpErrorMsg, "Handle POST - values: could not parse '%s'", text);
		return 1;
	}
	if (setValueFunction((int32_t)value))
	{
		HttpErrorType = 400; //Bad Request
		sprintf(HttpErrorMsg, "Handle POST - values: could not set %s", item);
		return 1;
	}
	return 1;
}
static int handleValueSint16(char * item, char * text, char * compare, int(*setValueFunction)( int16_t)) {
	if (strcmp(item, compare) != 0) return 0;
	//Log('d', "Handling sint16 %s %s", item, text);
	int value;
	if (sscanf(text, "%'d",  &value) <= 0)
	{
		HttpErrorType = 400; //Bad Request
		sprintf(HttpErrorMsg, "Handle POST - values: could not parse '%s'", text);
		return 1;
	}
	if (setValueFunction((int16_t)value))
	{
		HttpErrorType = 400; //Bad Request
		sprintf(HttpErrorMsg, "Handle POST - values: could not set %s", item);
		return 1;
	}
	return 1;
}
static int handleValueUint16(char * item, char * text, char * compare, int(*setValueFunction)(uint16_t)) {
	if (strcmp(item, compare) != 0) return 0;
	//Log('d', "Handling uint16 %s %s", item, text);
	unsigned int value;
	if (sscanf(text, "%'u",  &value) <= 0)
	{
		HttpErrorType = 400; //Bad Request
		sprintf(HttpErrorMsg, "Handle POST - values: could not parse '%s'", text);
		return 1;
	}
	if (setValueFunction((uint16_t)value))
	{
		HttpErrorType = 400; //Bad Request
		sprintf(HttpErrorMsg, "Handle POST - values: could not set %s", item);
		return 1;
	}
	return 1;
}
static int handleValueFloat (char * item, char * text, char * compare, int(*setValueFunction)(   float)) {
	if (strcmp(item, compare) != 0) return 0;
	//Log('d', "Handling float %s %s", item, text);
	float value;
	if (sscanf(text, "%f",  &value) <= 0)
	{
		HttpErrorType = 400; //Bad Request
		sprintf(HttpErrorMsg, "Handle POST - values: could not parse '%s'", text);
		return 1;
	}
	if (setValueFunction(value))
	{
		HttpErrorType = 400; //Bad Request
		sprintf(HttpErrorMsg, "Handle POST - values: could not set %s", item);
		return 1;
	}
	return 1;
}

//Handle posts
static int handlePostGetChar      (char *r)                   {
	char sHex[3];
	
	//Decode the character if need be
	switch (*p)
	{
		case '+' :
			*r  = ' ';
			break;
		case '%':
			//First hex digit
			p++;
			sHex[0] = *p;
			
			//Second hex digit
			p++;
			sHex[1] = *p;
			
			//Convert hex digits to ascii
			int i;
			if (sscanf(sHex, "%x", &i) < 1)
			{
				HttpErrorType = 400; //Bad Request
				sprintf(HttpErrorMsg, "Invalid URL encoded sequence '%%%2s'", sHex);
				return -1;
			}
			*r = (char)i;
			break;
		default:
			*r = *p;
			break;
	}
	
	//Advance the pointer
	p++;
	return 0;
}
static int handlePostReadName     (char *name )               { //Reads the name and leaves the pointer at the start of the value
	char c;
	
	//Read the name and the leave the pointer at the start of the value
	int n = 0;
	while (p < pl)
	{
		if (*p == '=')
		{
			p++; //Position the pointer after the '='
			break;
		}
		if (handlePostGetChar(&c)) return -1;
		name[n++] = c;
	}
	name[n] = 0;
	return 0;
}
static int handlePostReadValue    (char *value)               { //Reads the name and value and leave the pointer at the start of the next name value pair
	char c;

	//Read the value and leave the pointer at the start of the next name value pair
	int v = 0;
	
	while (p < pl)
	{
		if (*p == '&')
		{
			p++; //Position the pointer after the '&'
			break;
		}
		if (handlePostGetChar(&c)) return -1;
		value[v++] = c;
	}
	value[v] = 0;
	return 0;
}
static int handlePostPicValue     (char *pFlag, char *pLed)   {
	char name[50];
	char value[128];
	if (handlePostReadName (&name[0] )) return -1;
	if (handlePostReadValue(&value[0])) return -1;
	
	//Log('d', "Handling name '%s' : value '%s'", name, value);
	
	//General
	if (strcmp(name, "RtcFlag") == 0) { *pFlag = 1; return 0; }
	if (strcmp(name, "RtcLED" ) == 0) { *pLed  = 1; return 0; }
	if (handleValueSint64(name, value, "RtcDebug0" , RtcSetDebug0 )) return 0;
	if (handleValueSint64(name, value, "RtcDebug1" , RtcSetDebug1 )) return 0;
	if (handleValueSint64(name, value, "RtcDebug2" , RtcSetDebug2 )) return 0;
	if (handleValueSint64(name, value, "RtcDebug3" , RtcSetDebug3 )) return 0;
	if (handleValueString(name, value, "RtcComment", RtcSetComment)) return 0;
	
	//Deal with unknown value
	HttpErrorType = 400; //Bad Request
	sprintf(HttpErrorMsg, "Handle POST - values: did not recognise '%s'", name);
	
	return -1;
}
static int handlePostPicValues    (void)                      {
	char flag = 0;
	char led = 0;
	
	while (p < pl)
	{
		if (handlePostPicValue(&flag, &led)) return -1;
	}
	if (!flag)
	{
		if(RtcRstError()) return -1;
		Log('d', "Sent reset error");
	}
	if (RtcSetLED(led)) return -1;
	
	return 0;
}
static int handlePostClockValue   (char *pClockIsSet)         {
	char name[50];
	char value[128];
	if (handlePostReadName (&name[0] )) return -1;
	if (handlePostReadValue(&value[0])) return -1;
	
	//Log('d', "Handling name '%s' : value '%s'", name, value);
	
	//Clock
	if (strcmp(name, "RtcClockIsSet") == 0) { *pClockIsSet = 1 ; return 0; }
	if (strcmp(name, "RtcClockEpoch") == 0)
	{
		unsigned int parsedvalue;
		if (sscanf(value, "%'u",  &parsedvalue) <= 0)
		{
			HttpErrorType = 400; //Bad Request
			sprintf(HttpErrorMsg, "Handle POST - values: could not parse '%s'", value);
			return 0;
		}
		uint16_t newEpoch = (uint16_t)parsedvalue;
		uint16_t oldEpoch;
		if (RtcGetClockEpoch(&oldEpoch))
		{
			HttpErrorType = 400; //Bad Request
			sprintf(HttpErrorMsg, "Handle POST - values: could not get RtcGetClockEpoch");
			return 0;
		}
		if (RtcSetClockEpoch(newEpoch))
		{
			HttpErrorType = 400; //Bad Request
			sprintf(HttpErrorMsg, "Handle POST - values: could not set RtcSetClockEpoch");
			return 0;
		}
		if (!RtcIsSet()) return 0; //don't adjust the time if the clock is not already set
		
		int32_t daysBetweenEpochs = DateTimeDaysBetweenTwoEpochs(oldEpoch, newEpoch);
		int64_t nsBetweenEpochs = (int64_t)daysBetweenEpochs * 24 * 3600 * ONE_BILLION;
		if (RtcSetClockFromOffset(-nsBetweenEpochs))
		{
			HttpErrorType = 400; //Bad Request
			sprintf(HttpErrorMsg, "Handle POST - values: could not set RtcSetClockFromOffset");
			return 0;
		}
		return 0;
	}
	
	//Deal with unknown value
	HttpErrorType = 400; //Bad Request
	sprintf(HttpErrorMsg, "Handle POST - values: did not recognise '%s'", name);
	
	return -1;
}
static int handlePostClockValues  (void)                      {
	char clockIsSet = 0;
	while (p < pl)
	{
		if (handlePostClockValue(&clockIsSet)) return -1;
	}	
	if (!clockIsSet) if(RtcRstClock()) return -1;
	return 0;
}
static int handlePostSampleValue  (char *pSampleControlOn)    {
	char name[50];
	char value[128];
	if (handlePostReadName (&name[0] )) return -1;
	if (handlePostReadValue(&value[0])) return -1;
	
	//Log('d', "Handling name '%s' : value '%s'", name, value);
		
	//Sample
	if (strcmp(name, "RtcSampleControlOn"  ) == 0) { *pSampleControlOn = 1    ; return 0; }

	//Rate loop
	if (handleValueSint16(name, value, "RtcRateLoopParamKr", RtcSetRateLoopParamKr)) return 0;
	if (handleValueSint32(name, value, "RtcRateLoopParamMax", RtcSetRateLoopParamMax)) return 0;

	//Time loop
	if (handleValueSint16(name, value, "RtcTimeLoopParamKa", RtcSetTimeLoopParamKa)) return 0;
	if (handleValueSint32(name, value, "RtcTimeLoopParamMax", RtcSetTimeLoopParamMax)) return 0;
	if (handleValueSint32(name, value, "RtcTimeLoopParamSpreadSecs", RtcSetTimeLoopParamSpreadSecs)) return 0;

	
	//Deal with unknown value
	HttpErrorType = 400; //Bad Request
	sprintf(HttpErrorMsg, "Handle POST - values: did not recognise '%s'", name);
	
	return -1;
}
static int handlePostSampleValues (void)                      {
	char sampleControlOn = 0;
	
	while (p < pl)
	{
		if (handlePostSampleValue(&sampleControlOn)) return -1;
	}
	if (RtcSetSampleControlOn(sampleControlOn)) return -1;
	
	return 0;
}
static int handlePostTickValue    (void)                      {
	char name[50];
	char value[128];
	if (handlePostReadName (&name[0] )) return -1;
	if (handlePostReadValue(&value[0])) return -1;
	
	//Log('d', "Handling name '%s' : value '%s'", name, value);
		
	//Aging
	if (handleValueSint32(name, value, "RtcAgingParamFo", RtcSetAgingParamFo)) return 0;
	if (handleValueSint32(name, value, "RtcAgingParamAo", RtcSetAgingParamAo)) return 0;
	
	//Regulator
	if (handleValueFloat(name, value, "RtcRegulatorParamTo", RtcSetRegulatorParamTo)) return 0;
	if (handleValueFloat(name, value, "RtcRegulatorParamKo", RtcSetRegulatorParamKo)) return 0;

	//Slew
	if (handleValueSint32(name, value, "RtcSlewAmountNs", RtcSetSlewAmountNs)) return 0;
	if (handleValueSint32(name, value, "RtcSlewRateNsPerS", RtcSetSlewRateNsPerS)) return 0;
	
	//Deal with unknown value
	HttpErrorType = 400; //Bad Request
	sprintf(HttpErrorMsg, "Handle POST - values: did not recognise '%s'", name);
	
	return -1;
}
static int handlePostTickValues   (void)                      {
	while (p < pl)
	{
		if (handlePostTickValue()) return -1;
	}
	
	return 0;
}
static int handlePostStepValue    (char *pFanStepDirection, char *pFanStepAuto)    {
	char name[50];
	char value[128];
	if (handlePostReadName (&name[0] )) return -1;
	if (handlePostReadValue(&value[0])) return -1;
	
	if (strcmp(name, "FanStepDirection") == 0) { *pFanStepDirection = 1; return 0; }
	if (strcmp(name, "FanStepAuto"     ) == 0) { *pFanStepAuto      = 1; return 0; }
	
	if (handleValueFloat (name, value, "FanStepAmount", PidSetStepAmount)) return 0;
	
	//Deal with unknown value
	HttpErrorType = 400; //Bad Request
	sprintf(HttpErrorMsg, "Handle POST - values: did not recognise '%s'", name);
	
	return -1;
}
static int handlePostStepValues   (void)                      {
	char fanStepDirection = 0;
	char fanStepAuto = 0;
	while (p < pl)
	{
		if (handlePostStepValue(&fanStepDirection, &fanStepAuto)) return -1;
	}
	if (PidSetStepDirection(fanStepDirection)) return -1;
	if (PidSetStepAuto     (fanStepAuto)     ) return -1;
	
	return 0;
}
static int handlePostPidValue     (void)                      {
	char name[50];
	char value[128];
	if (handlePostReadName (&name[0] )) return -1;
	if (handlePostReadValue(&value[0])) return -1;
	
	if (handleValueFloat (name, value, "FanTarget", PidSetTarget)) return 0;
	
	//Deal with unknown value
	HttpErrorType = 400; //Bad Request
	sprintf(HttpErrorMsg, "Handle POST - values: did not recognise '%s'", name);
	
	return -1;
}
static int handlePostPidValues    (void)                      {
	while (p < pl)
	{
		if (handlePostPidValue()) return -1;
	}
	
	return 0;
}
static int handlePostAmbientValue (char *pFanAuto)            {
	char name[50];
	char value[128];
	if (handlePostReadName (&name[0] )) return -1;
	if (handlePostReadValue(&value[0])) return -1;
	
	if (strcmp(name, "FanAuto"        ) == 0) { *pFanAuto         = 1; return 0; }

	//Deal with unknown value
	HttpErrorType = 400; //Bad Request
	sprintf(HttpErrorMsg, "Handle POST - values: did not recognise '%s'", name);
	
	return -1;
}
static int handlePostAmbientValues(void)                      {
	char fanAuto = 0;
	while (p < pl)
	{
		if (handlePostAmbientValue(&fanAuto)) return -1;
	}
	if (PidSetAuto(fanAuto)) return -1;
	
	return 0;
}
static int handlePostFanValue     (void)                      {
	char name[50];
	char value[128];
	if (handlePostReadName (&name[0] )) return -1;
	if (handlePostReadValue(&value[0])) return -1;

	if (handleValueUint16(name, value, "FanOutput", FanSetOutput)) return 0;
	
	//Deal with unknown value
	HttpErrorType = 400; //Bad Request
	sprintf(HttpErrorMsg, "Handle POST - values: did not recognise '%s'", name);
	
	return -1;
}
static int handlePostFanValues    (void)                      {
	while (p < pl)
	{
		if (handlePostFanValue()) return -1;
	}
	
	return 0;
}
static int handlePostSheet        (char *sheetname)           {
	strcpy(HttpSheet, sheetname);
	return 0;
}
static int handlePostEditSave     (char *filename)            {
	Log('i' ,"Saving to file %s", filename);
	FILE *fp = fopen(filename, "w");
	if (fp == NULL)
	{
		HttpErrorType = 400; //Bad Request
		sprintf(HttpErrorMsg, "Could not open file to save POST %s", filename);
		return -1;
	}
	char c;
	while (p < pl)
	{
		if (handlePostGetChar(&c)) return -1;
		if (c != '\r')
		{
			if (fputc(c, fp) == EOF)
			{
				HttpErrorType = 500; //Internal Server Error
				sprintf(HttpErrorMsg, "ERROR writing to file to save POST %s", filename);
				fclose(fp);
				return -1;
			}
		}
	}
	if (fflush(fp))
	{
		HttpErrorType = 500; //Internal Server Error
		sprintf(HttpErrorMsg, "ERROR flushing file to save POST %s", filename);
		fclose(fp);
		return -1;
	}
	if (fclose(fp))
	{
		HttpErrorType = 500; //Internal Server Error
		sprintf(HttpErrorMsg, "Could not close file to save POST %s", filename);
		fclose(fp);
		return -1;
	}
	
	//Reload the config file if that is what it is.
	CfgLock();
		char * curveFile = strdupa(CfgFanCurveFileName);
	CfgUnlock();
	if (strcmp(filename,   CfgFile) == 0)   CfgLoad();
	if (strcmp(filename, curveFile) == 0) CurveLoad();
	
	return 0;
}
static int handlePostEdit         (char *filename)            {

	//Read the name
	char name[20];
	if (handlePostReadName(&name[0])) return -1;
	
	if (strcmp(name, "content") == 0) return handlePostEditSave(filename);
		
	HttpErrorType = 400; //Bad Request
	sprintf(HttpErrorMsg, "Handle POST name 'edit': expected 'content' but had '%s'", name);
	return -1;
}
static int handlePost             () {
	//Read the name
	char  name[20];
	char value[20];
	if (handlePostReadName (&name[0] )) return -1;
	if (handlePostReadValue(&value[0])) return -1;
	
	//Handle the name
	if (strcmp(name, "edit"          ) == 0) return handlePostEdit    (value);
	if (strcmp(name, "sheet"         ) == 0) return handlePostSheet   (value);
	if (strcmp(name, "pic_values"    ) == 0) return handlePostPicValues    ();
	if (strcmp(name, "clock_values"  ) == 0) return handlePostClockValues  ();
	if (strcmp(name, "sample_values" ) == 0) return handlePostSampleValues ();
	if (strcmp(name, "tick_values"   ) == 0) return handlePostTickValues   ();
	if (strcmp(name, "step_values"   ) == 0) return handlePostStepValues   ();
	if (strcmp(name, "pid_values"    ) == 0) return handlePostPidValues    ();
	if (strcmp(name, "ambient_values") == 0) return handlePostAmbientValues();
	if (strcmp(name, "fan_values"    ) == 0) return handlePostFanValues    ();
	if (strcmp(name, "upload_samples") == 0) return SampleUpload           ();
	
	//Deal with unknown POST
	HttpErrorType = 400; //Bad Request
	sprintf(HttpErrorMsg, "POST: did not recognise '%s'", name);
	return -1;
}
int Post         (char * pStart, int length) {

	//Set up the buffer pointers
	p  = pStart;
	pl = pStart + length;
	
	//Handle the Post
	if (handlePost()) return -1;

	//Return the number of bytes consumed
	return p - pStart;
}
