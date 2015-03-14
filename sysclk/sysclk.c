#include <pthread.h> //thread stuff
#include <semaphore.h> 

#include <stdio.h>  //fopen printf
#include <errno.h>  //errno
#include <string.h> //strerror
#include <stdlib.h> //strdupa free
#include <stdint.h> //int64_t
#include <unistd.h>  //sleep

#include "../global.h"
#include "../lib/log.h"
#include "../lib/cfg.h"
#include "../lib/datetime.h"
#include "../lib/rtc.h"
#include "../lib/thread.h"
#include "../utc/utc.h"

#define SCAN_INTERVAL_SECONDS 100

//Payload
double SysClkOffsetSeconds;
double SysClkFrequencyPpm;
int SysClkSetTicksOffset(void) {


	
	//Get offset
	uint64_t tai;
	if (RtcGetNowTicks(UtcEpoch, UtcTicksPerSec, &tai)) return -1;
	uint64_t utc;
	char isLeap;
	if (TaiToUtc(tai, &utc, &isLeap)) return -1;
	uint64_t rtcTicks;
	if (UtcToTicks(utc, 1970, ONE_MILLION, &rtcTicks)) return -1;
	uint64_t sysTicks;
	if (DateTimeNowTicks(1970, ONE_MILLION, &sysTicks)) return -1;
	int64_t ticksOffset = rtcTicks - sysTicks;
	
	//Calculate offset
	SysClkOffsetSeconds = (double)ticksOffset / ONE_MILLION;
	
	//Get existing frequency
	if (DateTimeGetFreqOffsetPpm(&SysClkFrequencyPpm)) return -1;

	//Hold time of last run
	static time_t lastTime;

	//Set the time
	if (DateTimeSetTicksOffset(ONE_MILLION, ticksOffset)) return -1;
	
	//Get the time of this run
	time_t thisTime;
	if (DateTimeNowT(&thisTime)) return -1;
	
	//If this is not the first run
	if (lastTime)
	{
		long intervalSeconds = thisTime - lastTime;
		double freqOffset = SysClkOffsetSeconds / intervalSeconds;
		double sampleFreqOffsetPpm = freqOffset * ONE_MILLION;
				
		CfgLock();
		double gain = CfgSysClkFreqLoopGain;
		CfgUnlock();
		
		SysClkFrequencyPpm += gain * sampleFreqOffsetPpm;
		if (DateTimeSetFreqOffsetPpm(SysClkFrequencyPpm)) return -1;
	}
	
	//Save time of this run for next time
	lastTime = thisTime;
	return 0;
}

//Locals
static struct Thread t;

//Thread stuff
static void *worker(void *arg) {
	//Initialise thread part
	if (ThreadWorkerInit(&t)) return NULL;
	
	//Wait until RTC set
	while (!RtcIsSet()) sleep(1);
	
	//Go into the main loop
	while(1)
	{	
		if (ThreadCancelDisable(&t)) break;
		if (RtcIsSet()) SysClkSetTicksOffset();
		if (ThreadCancelEnable (&t)) break;
		sleep(SCAN_INTERVAL_SECONDS);
	}
	
	Log('w', "Ending thread");
	return NULL;
}

//External routines
void SysClkKill(void) {
	ThreadCancel(&t);
}
void SysClkInit(char *name, int priority) {
	t.Name = name;
	t.Worker = worker;
	t.NormalPriority = priority;
	ThreadStart(&t);
}
void SysClkJoin(void) {
	ThreadJoin(&t);
}
