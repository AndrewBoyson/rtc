#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h> 
#include <errno.h>
#include <unistd.h>  //sleep

#include <sys/stat.h>   //File date

#include "../lib/log.h"
#include "../lib/thread.h"
#include "../ntp/ntp.h"
#include "../lib/rtc.h"

static struct Thread  t;

static void *worker(void *arg) {
	//Initialise thread part
	if (ThreadWorkerInit(&t)) return NULL;
	
	//Wait until RTC set
	while (!RtcIsSet()) sleep(1);

	//Initialise local part
	if (NtpBind()) return NULL;
	
	//Handle requests
	while(1) NtpSendTime();
	
    NtpUnbind();
    return 0; 
}
void NtpSvrKill(void) {
	ThreadCancel(&t);
}
void NtpSvrInit(char *name, int priority) {
	t.Name = name;
	t.Worker = worker;
	t.NormalPriority = priority;
	ThreadStart(&t);
}
void NtpSvrJoin(void) {
	ThreadJoin(&t);
}
