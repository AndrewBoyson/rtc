#include <pthread.h> //thread stuff
#include <semaphore.h> 

#include <stdio.h>   //fopen printf
#include <errno.h>   //errno
#include <string.h>  //strerror
#include <stdlib.h>  //free
#include <unistd.h>  //sleep

#include "leaps.h"
#include "../lib/log.h"
#include "../lib/cfg.h"
#include "../lib/thread.h"
#include "../lib/datetime.h"

#define RETRY_INTERVAL_MINS 30

//Locals
static struct Thread t;

//Thread stuff
static long readConfigScanMinutes(void) {
	CfgLock();
	int daysToWait = CfgLeapSecondNewFileCheckDays;
	CfgUnlock();
	return daysToWait * 24 * 60;
}
static int workerWait(long minsOrReadFromConfig) { //Specify a negative number to read the wait time from configuration

	long minutesCount = 0; //Updated after each sleep in the loop
	
	while(1)
	{
		//Calculate how long we have to wait
		long minutesToWait = minsOrReadFromConfig < 0 ? readConfigScanMinutes() : minsOrReadFromConfig;
		
		//Check if time to run
		if (minutesCount >= minutesToWait) return 0;
		
		//Wait a minute
		sleep(60);
		
		//Increment the number of minutes waited so far
		minutesCount++;
	}
}
static void *worker(void *arg) {
	//Initialise thread part
	if (ThreadWorkerInit(&t)) return NULL;
	
	long minutesToWait = -1;
	
	//Thread loop
	while(1)
	{
		if (workerWait(minutesToWait)) break;

		if (ThreadCancelDisable(&t)) break;

		minutesToWait = LeapsLoad() ? RETRY_INTERVAL_MINS : -1; //Specify a negative number to read the wait time from configuration
		
		if (ThreadCancelEnable (&t)) break;
		
	}
	
	Log('w', "Ending thread");
	return NULL;
}

//External routines
void LeapSvrKill(void) {
	ThreadCancel(&t);
}
void LeapSvrInit(char *name, int priority) {
	t.Name = name;
	t.Worker = worker;
	t.NormalPriority = priority;
	ThreadStart(&t);
}
void LeapSvrJoin(void) {
	ThreadJoin(&t);
}
