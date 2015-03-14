#include <time.h>		//gmtime_r, time
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>	//thread stuff
#include <stdarg.h>
#include <stdlib.h>

#include "../global.h"
#include "cfg.h"

#define BUFFER_SIZE 2048
char *LogFile = "/var/log/rtc";

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

char LogIsAtLevel(char type) {
	switch (CfgLogLevel)
	{
		case 'c': if (type == 'i' || type == 'w' || type == 'e') return 0;
		case 'e': if (type == 'i' || type == 'w') return 0;
		case 'w': if (type == 'i') return 0;
	}
	return 1;
}

static void rawlog(char type, char *threadName, char *t1, char *t2) {

	if (!LogIsAtLevel(type)) return;

	time_t t;
	struct tm tm;
	time(&t);
	gmtime_r(&t, &tm); //Don't use DateTimeNowTm: if there is an error it calls LogErrno which calls here -> boom!
	char sTime[20];
	strftime(sTime, sizeof(sTime), "%Y/%m/%d %H:%M:%S", &tm);
	
	FILE *fp;
	if (DAEMONISE)
	{
		fp = fopen(LogFile, "a");
	}
	else
	{
		fp = stdout;
	}
	
	fprintf(fp, "%s %c <%s>", sTime, type, threadName);
	fprintf(fp, "%*s", 8 - strlen(threadName), "");
	fprintf(fp, "%s", t1);
	if (t2) fprintf(fp, "%s", t2);
	fprintf(fp, "\n");
	
	if (DAEMONISE)
	{
		fclose(fp);
	}

}

void LogP(char type, char *t1, char *t2) {
	//Get thread name
	pthread_t thr = pthread_self();
	char threadName[20];
	int result = pthread_getname_np(thr, threadName, sizeof(threadName));
	if (result != 0)
	{
		rawlog('e', threadName, "problem getting thread name", strerror(result));
	}
	
	//Acquire lock 
	result = pthread_mutex_lock(&mutex);
	if (result != 0)
	{
		rawlog('e', threadName, "problem acquiring log lock", strerror(result));
	}
		
	//Do the log itself
	rawlog(type, threadName, t1, t2);
	
	//Release the lock
	result = pthread_mutex_unlock(&mutex);
	if (result != 0)
	{
		rawlog('e', threadName, "problem releasing log lock", strerror(result));
	}	
}
void LogPN(char type, char *t1, int n1, char *t2, int n2) {
	char *p1 = NULL;
	char *p2 = NULL;
	
	p1 = malloc(n1 + 1);
	if (t2 != NULL) p2 = malloc(n2 + 2);
	
	strncpy(p1, t1, n1);
	if (t2 != NULL) strncpy(p2, t2, n2);
	
	LogP(type, p1, p2);
	
	free(p1);
	if (t2 != NULL) free(p2);
}
void Log(char type, const char *format, ...) {

	//Get the stuff to print and put it into buffer
 	char buffer[BUFFER_SIZE];
	va_list args;
	va_start (args, format);
	vsprintf (buffer, format, args);
	va_end (args);

	LogP(type, buffer, NULL);
}
void LogErrno(char *text) {
	Log('e', "%s %s", text, strerror(errno));
}
void LogNumber(char *text, int number) {
	Log('e', "%s %s", text, strerror(number));
}
