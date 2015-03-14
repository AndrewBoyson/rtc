#include <stdio.h>     //fopen printf
#include <errno.h>     //errno
#include <string.h>    //strerror
#include <stdlib.h>    //free strdupa
#include <stdint.h>    //int32_t etc
#include <unistd.h>    //sleep
#include <time.h>      //struct tm

#include "../global.h"
#include "../lib/log.h"
#include "../lib/cfg.h"
#include "../lib/datetime.h"
#include "../lib/file.h"
#include "../lib/rtc.h"
#include "../lib/thread.h"
#include "../ntp/ntp.h"
#include "../utc/utc.h"
#include "../fan/pid.h"

#define     RETRY_INTERVAL_MINS         10

//Locals
static struct Thread t;
static          FILE *fp;

//Sample stuff
#define VALUE_SIZE 30
#define FILE_NAME_SIZE 30
static char value[VALUE_SIZE];

static char *cfgServer;
static int   cfgInterDelaySecs;
static int   cfgMaxCount;


static int openLog(char *filename) {
	
	fp = fopen(filename, "a");
	if (fp == NULL)
	{
		LogErrno("ERROR opening sample log file: ");
		return -1;
	}
	return 0;
}
static int closeLog(void) {
	if (fclose(fp))
	{
		fclose(fp);
		LogErrno("Error closing sample log file: ");
		return -1;
	}
	return 0;
}
static int appendLog(int level, char *name) {
   switch (level)
	{
		case 1: if (fprintf(fp, "  "    ) < 0) { LogErrno("Error writing to sample log file: "); closeLog(); return -1; } break;
		case 2: if (fprintf(fp, "    "  ) < 0) { LogErrno("Error writing to sample log file: "); closeLog(); return -1; } break;
		case 3: if (fprintf(fp, "      ") < 0) { LogErrno("Error writing to sample log file: "); closeLog(); return -1; } break;
	}
	if (fprintf(fp, "%s = %s\r\n", name, &value[0]) < 0) { LogErrno("Error writing to sample log file: "); closeLog(); return -1; }
	return 0;
}
static int saveSample(void) {

	//Get the current time
	struct tm tm;
	if (DateTimeNowTm(&tm)) return -1;
	
	//Open the file
	CfgLock();
	char *template = strdupa(CfgSampleLogFileName);
	CfgUnlock();
	char filename[FILE_NAME_SIZE];
	if (DateTimeTmToF(tm, template, FILE_NAME_SIZE, filename)) return -1;
	if (openLog(filename)) return -1;
	
	//Date
	if (DateTimeTmToS(tm, VALUE_SIZE, value)) return -1;
	if (appendLog(0, "sample"))          return -1;
	
	//Read PIC
	float temperature;
	float voltage;
	int32_t Fo;
	uint64_t thisExtClock, thisAbsClock, thisIntClock, thisTicClock;
	uint64_t lastExtClock, lastAbsClock, lastIntClock, lastTicClock;
	if (RtcGetSampleTemperature(&temperature))  return -1;
	if (RtcGetVoltage          (&voltage))      return -1;
	if (RtcGetAgingParamFo     (&Fo))           return -1;
	if (RtcGetSampleThisTicNs  (&thisTicClock)) return -1;
	if (RtcGetSampleThisIntNs  (&thisIntClock)) return -1;
	if (RtcGetSampleThisAbsNs  (&thisAbsClock)) return -1;
	if (RtcGetSampleThisExtNs  (&thisExtClock)) return -1;
	if (RtcGetSampleLastTicNs  (&lastTicClock)) return -1;
	if (RtcGetSampleLastIntNs  (&lastIntClock)) return -1;
	if (RtcGetSampleLastAbsNs  (&lastAbsClock)) return -1;
	if (RtcGetSampleLastExtNs  (&lastExtClock)) return -1;
	
	//Temperature
	if (sprintf(value, "%1.1f", (double)temperature) < 0)  { LogErrno("Error formatting for sample log file: "); closeLog(); return -1; }
	if (appendLog(1, "temperature"))               return -1;
	
	//Voltage
	if (sprintf(value, "%1.2f", (double)voltage) < 0)  { LogErrno("Error formatting for sample log file: "); closeLog(); return -1; }
	if (appendLog(1, "battery"))               return -1;
	
	//Mean offset from PIC
	if (sprintf(value, "%1.1f", (double)((int64_t)(thisExtClock - thisAbsClock))  / (double) ONE_MILLION) < 0)  { LogErrno("Error formatting for sample log file: "); closeLog(); return -1; }
	if (appendLog(1, "ms offset from pic"))  return -1;
	
	//Base rate (Fo)
	if (sprintf(value, "%d", Fo) < 0)  { LogErrno("Error formatting for sample log file: "); closeLog(); return -1; }
	if (appendLog(1, "base rate"))     return -1;
	
	//Record the stuff which is only available if there was a previous sample
	if (lastExtClock != 0)
	{
		int64_t periodInt = thisIntClock - lastIntClock;
		int64_t periodExt = thisExtClock - lastExtClock;
		int64_t periodTic = thisTicClock - lastTicClock;

		double nsXtalPeriodDiff = (double)(periodTic - periodExt) / (double)periodTic * ONE_BILLION;
		double nsCompPeriodDiff = (double)(periodInt - periodExt) / (double)periodTic * ONE_BILLION;

		if (sprintf(value, "%1.0f", nsXtalPeriodDiff) < 0)  { LogErrno("Error formatting for sample log file: "); closeLog(); return -1; }
		if (appendLog(1, "xtal rate")) return -1;
		
		if (sprintf(value, "%1.0f", nsCompPeriodDiff) < 0)  { LogErrno("Error formatting for sample log file: "); closeLog(); return -1; }
		if (appendLog(1, "compensated rate")) return -1;
	}
	
	if (closeLog()) return -1;
	
	return 0;
}
static int connect(void) {
	
	return NtpConnect(cfgServer);
}
static int disconnect(void) {
	return NtpClose();
}
static int getSingleTime(int64_t *pRtcMinusNtpTai, int64_t *pTravelTimeTai) {
	if (t.Pthread && ThreadSetCriticalPriority(&t)) return -1;
	int r = NtpGetTime(pRtcMinusNtpTai, pTravelTimeTai);
	if (t.Pthread && ThreadSetNormalPriority(&t)) return -1;
	return r;
}
static int getTime(int64_t *pRtcMinusNtpTai) {
	int count = 0;
	int64_t totalRtcMinusNtpTai = 0;
	while (1)
	{
		if (count >= cfgMaxCount) break; //If maxCount = 0 then this will break without doing anything
		if (count) sleep(cfgInterDelaySecs);
		count++;
		int64_t rtcMinusNtpTai;
		int64_t travelTimeTai;
		if (getSingleTime(&rtcMinusNtpTai, &travelTimeTai)) return -1;
		totalRtcMinusNtpTai += rtcMinusNtpTai;
	}
	if (count == 0)
	{
		Log('e', "Count is zero");
		return -1;
	}
	*pRtcMinusNtpTai = totalRtcMinusNtpTai / count;
	return 0;
}
static int waitForPIC(void) {
	for (int tries = 0; tries < 5; tries++)
	{
		char treated;
		if (RtcGetSampleTreated(&treated)) return -1;
		if (treated) return 0;
		sleep(1);
	}
	Log('e', "Timed out waiting for PIC to treat sample");
	return -1;
}

int SampleFetch(void) {
		
	//Read the configuration
	CfgLock();
		cfgServer          = strdupa(CfgSampleServer);
		cfgInterDelaySecs  = CfgSampleInterDelaySecs;
		cfgMaxCount        = CfgSampleCount;
	CfgUnlock();
	
	//Connect to the NTP server
	if (connect()) return -1;
	
	//Get the time from the NTP server
	int64_t rtcMinusNtpTai;
	if (getTime(&rtcMinusNtpTai))
	{
		disconnect();
		return -1;
	}
	
	//Close the connection
	if (disconnect()) return -1;
		
	//Send the sample
	int64_t ntpMinusRtcTai = -rtcMinusNtpTai;
	if (RtcSetSampleOffsetTicks(UtcTicksPerSec, ntpMinusRtcTai)) return -1;
	
	//Wait for the PIC to treat the sample
	if (waitForPIC()) return -1;
	
	//Record the sample
	if (saveSample()) return -1;
	
	//Increment fan temperature
	PidDoStep();
	
	return 0;
}

//Thread stuff
static long readSampleIntervalSeconds(void) {
	CfgLock();
	int hoursToWait = CfgSampleIntervalHours;
	CfgUnlock();
	return hoursToWait * 60 * 60;
}
static int   workerWait(long secondsOrReadFromConfig) { //Specify a negative number to read the wait time from configuration
	
	while(1)
	{
		//Don't wait if clock is not set
		char clockIsSet;
		if (RtcGetClockIsSet(&clockIsSet)) return -1;
		if (!clockIsSet) return 0;
		
		//Calculate how long we have to wait
	    long secondsToWaitAfterLastSample = secondsOrReadFromConfig < 0 ? readSampleIntervalSeconds() : secondsOrReadFromConfig;
		
		//Check if time to run
		uint64_t rtcNow;
		uint64_t rtcLastSample;
		if (RtcGetClockNowAbsNs  (&rtcNow       )) return -1;
		if (RtcGetSampleThisAbsNs(&rtcLastSample)) return -1;
		if (rtcNow > rtcLastSample + secondsToWaitAfterLastSample * ONE_BILLION)  return  0;

		//Wait for scan period
		sleep(60);
	}
}
static void *worker(void *arg) {
	//Initialise thread part
	if (ThreadWorkerInit(&t)) return NULL;

	//Go into the main loop; don't wait if clock is not set
	int secondsToWaitAfterLastSample = RtcIsSet() ? -1 : 0;
	while(1)
	{
		workerWait(secondsToWaitAfterLastSample);
		ThreadCancelDisable(&t);
		if (SampleFetch()) secondsToWaitAfterLastSample = RtcIsSet() ? RETRY_INTERVAL_MINS * 60 : 60;
		else               secondsToWaitAfterLastSample = -1;
		ThreadCancelEnable (&t);
	}
	Log('w', "Ending thread");
    return NULL;
}

//External routines
void SampleKill(void) {
	ThreadCancel(&t);
}
void SampleInit(char *name, int normalPriority, int criticalPriority) {
	t.Name = name;
	t.Worker = worker;
	t.NormalPriority   = normalPriority;
	t.CriticalPriority = criticalPriority;
	ThreadStart(&t);
}
void SampleJoin(void) {
	ThreadJoin(&t);
}
 int SampleUpload(void) {

	//Get the current time
	struct tm tm;
	if (DateTimeNowTm(&tm)) return -1;
	
	//Get the configuration
	CfgLock();
	char *template  = strdupa(CfgSampleLogFileName); //this gives us our own local copy of the template
	char *nasFolder = strdupa(CfgSampleNasPath);
	CfgUnlock();
	
	//Make filename
	char currentFilename[FILE_NAME_SIZE];
	if (DateTimeTmToF(tm, template, FILE_NAME_SIZE, currentFilename)) return -1;
	
	//Make stem
	char stem[FILE_NAME_SIZE];
	int i = 0;
	while(template[i] != '\0' && template[i] != '%')
	{
		stem[i] = template[i];
		i++;
	}
	stem[i] = '\0';
	
	//Open the directory
	if (DirOpen(".")) return -1;
	
	//Parse each file
	while (1)
	{
		//Fetch the next file which matches our stem
		char *filename = DirNextMatchStem(stem);
		if (filename == NULL) break;
		
		//Copy it to the NAS if it is newer
		char nasFile[256];
		sprintf(nasFile, "%s%s", nasFolder, filename);
		
		if (FileCopyIfNewer(filename, nasFile))
		{
			DirClose();
			return -1; 
		}
		
		//Purge the local folder of non current files
		if (strcmp(filename, currentFilename) != 0) FileDelete(filename);

		//Reord that the file has been uploaded has been done
		Log('w', "Uploaded '%s' to '%s'", filename, nasFile);
	}
	
	//Close the file
	if (DirClose()) return -1;
	
	return 0;
}
