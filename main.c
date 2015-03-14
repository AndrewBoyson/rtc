#include <stdlib.h>    //free
#include <unistd.h>    //sleep
#include <locale.h>    //setlocale
#include <semaphore.h>
#include <stdio.h>     //printf
#include <sys/stat.h>  //umask, mkdir
#include <errno.h>     //EEXIST

#include "global.h"
#include "lib/log.h"
#include "lib/cfg.h"
#include "lib/thread.h"
#include "utc/utc.h"
#include "lib/spi.h"
#include "lib/rtc.h"

#include "http/httpsvr.h"
#include "utc/leapsvr.h"
#include "ntp/ntpsvr.h"
#include "sample/sample.h"
#include "sysclk/sysclk.h"
#include "fan/fansvr.h"

#define PRIORITY_LEAPS           0
#define PRIORITY_FAN             1
#define PRIORITY_SAMPLE_NORMAL   2
#define PRIORITY_HTTP            3
#define PRIORITY_SYSCLK          4
#define PRIORITY_NTPSRV          5
#define PRIORITY_SAMPLE_CRITICAL 6

#define WORKING_DIRECTORY "/var/lib/rtc"

/* Startup
	
	To run at startup go to the /etc folder and use sudo nano rc.local to add /home/pi/rtc/rtc 
	
	Dependancies
	------------
	LeapSvr	<-- nothing
	HttpSvr	<-- nothing
	Sample	<-- UTC
	SysClk  <-- RTC ok
	NtpSvr  <-- RTC ok
*/

int Start(void) {

	//Initialise
	int r = mkdir(WORKING_DIRECTORY, 0777);
	if (r && errno != EEXIST)
	{
		LogErrno("ERROR creating working directory");
        return 1;
	}
	chdir(WORKING_DIRECTORY);// Change the current working directory.
	setlocale(LC_ALL,"en_GB.UTF-8");
	if (CfgLoad()) return 1;
	if (UtcInit()) return 1;
	if (SpiInit()) return 1;
	if (RtcInit()) return 1;
	
	//Start threads
	Log('i', "Starting threads");
	LeapSvrInit( "leaps", PRIORITY_LEAPS);
	 FanSvrInit(   "fan", PRIORITY_FAN);
	 SampleInit("sample", PRIORITY_SAMPLE_NORMAL, PRIORITY_SAMPLE_CRITICAL);
	HttpSvrInit(  "http", PRIORITY_HTTP);
	 SysClkInit("sysclk", PRIORITY_SYSCLK);
	 NtpSvrInit(   "ntp", PRIORITY_NTPSRV);
	sleep(1); //Give time for the other threads to start
	Log('w', "Application started");
	
	//Exit
	LeapSvrJoin();
	 FanSvrJoin();
	 SampleJoin();
	HttpSvrJoin();
	 SysClkJoin();
	 NtpSvrJoin();
	Log('w', "Exit");
	return 0;
}
int daemonise(void) { //returns 0 for the child, +1 for the parent and -1 on error
		pid_t process_id = 0;
		pid_t sid = 0;
		
		// Create child process
		process_id = fork();
		if (process_id < 0)
		{
			printf("fork failed!\n");
			return -1;
		}
		
		// We are the parent process and just to need to exit successfully
		if (process_id > 0)
		{
			printf("process_id of child process %d \n", process_id);
			return 1;
		}
		
		//We are the child process
		umask(0);             //unmask the file mode
		sid = setsid();       //set new session
		if(sid < 0) return -1;
				
		// Close stdin. stdout and stderr
		close(STDIN_FILENO);
		close(STDOUT_FILENO);
		close(STDERR_FILENO);
		
		return 0;
}
int main() {
	if (DAEMONISE)
	{
		switch(daemonise())
		{
			case -1: return 1; //Error
			case +1: return 0; //Parent
			case  0: break;    //Child
		}
	}
	
	//Run the application
	Start();
	
	return 0;
}
void Stop(void) {
	Log('w', "Stopping threads");
	LeapSvrKill();
	 FanSvrKill();
	HttpSvrKill();
	 SampleKill();
	 SysClkKill();
	 NtpSvrKill();
	   SpiClose();
		
	sleep(1); //pause thread for one second to allow other threads time to exit
}
