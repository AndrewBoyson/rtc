#include <pthread.h> //rwlock
#include <stdio.h>   //fopen printf
#include <errno.h>   //errno
#include <string.h>  //strerror
#include <stdlib.h>  //free
#include <stdint.h> 
#include "log.h"

#define MAX_NAME_OR_VALUE 100

char  *CfgFile = "/etc/rtc";

char  *CfgSampleServer;
int    CfgSampleIntervalHours;
int    CfgSampleCount;
int    CfgSampleInterDelaySecs;
char  *CfgSampleLogFileName;
char  *CfgSampleNasPath;

char  *CfgNtpClientLogFileName;

char   CfgLogLevel;

char  *CfgLeapSecondOnlineFileUTI;
char  *CfgLeapSecondCacheFileName;
int    CfgLeapSecondNewFileCheckDays;

double CfgSysClkFreqLoopGain;

int    CfgFanOutputPeriodSecs;
int    CfgFanPulsePeriodMs;
double CfgFanMinRunSpeed;
double CfgFanMinStartSpeed;
char  *CfgFanCurveFileName;
double CfgFanCurveCorrectionDivisor;
int    CfgFanCurveCorrectionCount;

double CfgFanProportionalGain;
double CfgFanIntegralTime;
double CfgFanDerivativeTime;
char  *CfgFanLogFileName;


static pthread_rwlock_t rwlock = PTHREAD_RWLOCK_INITIALIZER;
static int acquireWriteLock() {
	int result = pthread_rwlock_wrlock(&rwlock);
	if (result != 0)
	{
		LogNumber("Error acquiring write lock on cfg", result);
		return -1;
	}
	return 0;
}
static int  acquireReadLock() {
	int result = pthread_rwlock_rdlock(&rwlock);
	if (result != 0)
	{
		LogNumber("Error acquiring read lock on cfg", result);
		return -1;
	}
	return 0;
}
static int      releaseLock() {
	int result = pthread_rwlock_unlock(&rwlock);
	if (result != 0)
	{
		LogNumber("Error releasing cfg lock", result);
		return -1;
	}
	return 0;
}

static void saveString(char *value, char  **dest) {
	*dest = realloc(*dest, strlen(value) + 1); //strlen does not include the null so add 1
	if (*dest == NULL)
	{
		LogErrno("Error reallocating memory in saveValue");
		return;
	}
	*dest = strcpy(*dest, value);
}
static void saveInt   (char *value, int    *dest) {
	*dest = atoi(value);
}
static void saveDouble(char *value, double *dest) {
	*dest = atof(value);
}
static void rtrim     (int i, char *s) { //i is the length of the thing to trim
	while(1)
	{
		s[i] = '\0';
		if (--i < 0) break;
		if (s[i] != ' ' && s[i] != '\t') break;
	}
}

static void handleLine(int n, int v, char *name, char *value) {
	rtrim(n, name);
	rtrim(v, value);
	if (strcmp(name, "sample server")                    == 0) saveString   (value, &CfgSampleServer);
	if (strcmp(name, "sample interval hours")            == 0) saveInt      (value, &CfgSampleIntervalHours);
	if (strcmp(name, "sample count")                     == 0) saveInt      (value, &CfgSampleCount);
	if (strcmp(name, "sample inter delay secs")          == 0) saveInt      (value, &CfgSampleInterDelaySecs);
	if (strcmp(name, "sample log file name")             == 0) saveString   (value, &CfgSampleLogFileName);
	if (strcmp(name, "sample nas path")                  == 0) saveString   (value, &CfgSampleNasPath);

	if (strcmp(name, "ntp client log file name")         == 0) saveString   (value, &CfgNtpClientLogFileName);
	
	if (strcmp(name, "log level character")              == 0) CfgLogLevel = value[0];
	
	if (strcmp(name, "leap second online file UTI")      == 0) saveString   (value, &CfgLeapSecondOnlineFileUTI);
 	if (strcmp(name, "leap second cache file name")      == 0) saveString   (value, &CfgLeapSecondCacheFileName);
 	if (strcmp(name, "leap second new file check days")  == 0) saveInt      (value, &CfgLeapSecondNewFileCheckDays);
	
 	if (strcmp(name, "system clock frequency loop gain") == 0) saveDouble   (value, &CfgSysClkFreqLoopGain);

	if (strcmp(name, "fan output period secs")           == 0) saveInt      (value, &CfgFanOutputPeriodSecs);
	if (strcmp(name, "fan pulse period ms")              == 0) saveInt      (value, &CfgFanPulsePeriodMs);
 	if (strcmp(name, "fan minimum start speed")          == 0) saveDouble   (value, &CfgFanMinStartSpeed);
 	if (strcmp(name, "fan minimum run speed")            == 0) saveDouble   (value, &CfgFanMinRunSpeed);
	if (strcmp(name, "fan curve file name")              == 0) saveString   (value, &CfgFanCurveFileName);
	if (strcmp(name, "fan curve correction divisor")     == 0) saveDouble   (value, &CfgFanCurveCorrectionDivisor);
	if (strcmp(name, "fan curve correction count")       == 0) saveInt      (value, &CfgFanCurveCorrectionCount);

 	if (strcmp(name, "fan proportional gain")            == 0) saveDouble   (value, &CfgFanProportionalGain);
 	if (strcmp(name, "fan integral time")                == 0) saveDouble   (value, &CfgFanIntegralTime);
 	if (strcmp(name, "fan derivative time")              == 0) saveDouble   (value, &CfgFanDerivativeTime);
	if (strcmp(name, "fan log file name")                == 0) saveString   (value, &CfgFanLogFileName);
}
static void resetValues(void) {
	
	free(CfgSampleServer);            CfgSampleServer               = NULL;
	                                  CfgSampleIntervalHours        = 0;
	                                  CfgSampleCount                = 0;
	                                  CfgSampleInterDelaySecs       = 0;
	free(CfgSampleLogFileName);       CfgSampleLogFileName          = NULL;
	free(CfgSampleNasPath);           CfgSampleNasPath              = NULL;

	free(CfgNtpClientLogFileName);    CfgNtpClientLogFileName       = NULL;

	                                  CfgLogLevel                   = 0;

	free(CfgLeapSecondOnlineFileUTI); CfgLeapSecondOnlineFileUTI    = NULL;
	free(CfgLeapSecondCacheFileName); CfgLeapSecondCacheFileName    = NULL;
	                                  CfgLeapSecondNewFileCheckDays = 0;

	                                  CfgSysClkFreqLoopGain         = 0.0;
	
	                                  CfgFanOutputPeriodSecs        = 0;
	                                  CfgFanPulsePeriodMs           = 0;
	                                  CfgFanMinRunSpeed             = 0.0;
	                                  CfgFanMinStartSpeed           = 0.0;
	free(CfgFanCurveFileName);        CfgFanCurveFileName           = NULL;
	                                  CfgFanCurveCorrectionDivisor  = 0.0;
	                                  CfgFanCurveCorrectionCount    = 0;

	                                  CfgFanProportionalGain        = 0.0;
	                                  CfgFanIntegralTime            = 0.0;
	                                  CfgFanDerivativeTime          = 0.0;

	free(CfgFanLogFileName);          CfgFanLogFileName             = NULL;
}
int CfgLoad()   {

	if (acquireWriteLock()) return -1;

	FILE *fp = fopen(CfgFile, "r");
	if (fp == NULL)
	{
		LogErrno("Error opening file rtc.conf for reading");
		releaseLock();
		return -1;
	}
	resetValues();
	char  name[MAX_NAME_OR_VALUE];
	char value[MAX_NAME_OR_VALUE];
	char isName  = 1;
	char isValue = 0;
	char isStart = 0; //Used to trim starts
	int n = 0;
	int v = 0;
	while (1)
	{
		int c = fgetc(fp);
		if (c == EOF)  { handleLine(n, v, name, value); break;}
		if (c == '\0') { handleLine(n, v, name, value); break;}
		if (c == '\n') { handleLine(n, v, name, value); isName = 1; isValue = 0; isStart = 0; n = 0; v = 0; continue;}
		if (c == '=' && isName) {                       isName = 0; isValue = 1; isStart = 0;               continue;}
		if (c == '#')  {                                isName = 0; isValue = 0;                            continue;}
		if (c != ' ' && c != '\t') isStart = -1;
		if (isName  && isStart && n < MAX_NAME_OR_VALUE - 1)  name[n++] = (char)c; //n can never exceed MAX_NAME_OR_VALUE - 1 leaving room for a null at length n
		if (isValue && isStart && v < MAX_NAME_OR_VALUE - 1) value[v++] = (char)c; //v can never exceed MAX_NAME_OR_VALUE - 1 leaving room for a null at length v
	}
		
	fclose(fp);
	
	if (releaseLock()) return -1;
	
	return 0;
}
int CfgLock()   {
	return acquireReadLock();
}
int CfgUnlock() {
	return releaseLock();
}
