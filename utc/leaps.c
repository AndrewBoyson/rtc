#include <stdio.h>   //fopen printf
#include <errno.h>   //errno
#include <string.h>  //strerror
#include <stdlib.h>  //free strdupa
#include <pthread.h> //rwlock
#include <stdint.h>  //uint64_t

#include <sys/socket.h> //connect read write
#include <unistd.h>     //close
#include <netdb.h>      //addrinfo
#include <arpa/inet.h>  //inet_ntop
#include <stdarg.h>     //
#include <sys/stat.h>   //File date

#include "utc.h"
#include "../global.h"
#include "../lib/log.h"
#include "../lib/cfg.h"
#include "../lib/socket.h"
#include "../lib/ftp.h"
#include "../lib/datetime.h"
#include "../lib/file.h"

static pthread_rwlock_t rwlockFile   = PTHREAD_RWLOCK_INITIALIZER;
static pthread_rwlock_t rwlockStruct = PTHREAD_RWLOCK_INITIALIZER;

static int   writelockFile  () {
	int result = pthread_rwlock_wrlock(&rwlockFile);
	if (result != 0)
	{
		LogNumber("Error acquiring write lock on leapsecond file", result);
		return -1;
	}
	return 0;
}
static int    readlockFile  () {
	int result = pthread_rwlock_rdlock(&rwlockFile);
	if (result != 0)
	{
		LogNumber("Error acquiring read lock on leapsecond file", result);
		return -1;
	}
	return 0;
}
static int      unlockFile  () {
	int result = pthread_rwlock_unlock(&rwlockFile);
	if (result != 0)
	{
		LogNumber("Error releasing leapsecond file lock", result);
		return -1;
	}
	return 0;
}

static int   writelockStruct() {
	int result = pthread_rwlock_wrlock(&rwlockStruct);
	if (result != 0)
	{
		LogNumber("Error acquiring write lock on leapsecond structure", result);
		return -1;
	}
	return 0;
}
static int    readlockStruct() {
	int result = pthread_rwlock_rdlock(&rwlockStruct);
	if (result != 0)
	{
		LogNumber("Error acquiring read lock on leapsecond structure", result);
		return -1;
	}
	return 0;
}
static int      unlockStruct() {
	int result = pthread_rwlock_unlock(&rwlockStruct);
	if (result != 0)
	{
		LogNumber("Error releasing leapsecond structure lock", result);
		return -1;
	}
	return 0;
}

//Variable
uint64_t LastUpdatedSeconds;
uint64_t ExpiresSeconds;
  time_t CacheFileTime;
struct LeapSecondStruct {
/* Explanation
Utc is an uint64 consisting of the era and the whole seconds part of the ntp timestamp
All times here are in terms of ntp protocol timestamp including the era (ie 64bit whole seconds) NOT in UNIX time.

NTP max: 1900 + 2^32 seconds = 1900 + 136 years = 2036 before next era to a 0.2ns precision.

The leap second here is that utc second immediately prior to the value in the leap seconds file: the value
in the leapseconds file is the first second following the leap.

In other words the leap second is the second second 59

nnnnnn798 23:59:58 58 10 31/01/1998
nnnnnn799 23:59:59 59 10 31/01/1998
nnnnnn799 23:59:60 59 11 31/01/1998 <= Leap second
nnnnnn800 00:00:00 00 11 01/01/1999 <= Value in leapseconds file
nnnnnn801 00:00:01 01 11 01/01/1999

*/
	uint64_t Utc; // Seconds since 1900 - NTP rolls back but this includes the era
	uint32_t Count;
	struct LeapSecondStruct *Next;
} LeapSecond;
static struct LeapSecondStruct *pLeapSeconds = NULL;

///Utilities
int LeapsUtcSecondsToCount(uint64_t utc, signed char leap, uint32_t *pCount) { //leap can be 0, 1 or -1
	if (pLeapSeconds == NULL)
	{
		Log('e', "Leap seconds list not loaded");
		return -1;
	}
	if (readlockStruct()) return -1;
	for(struct LeapSecondStruct *p = pLeapSeconds; p != NULL; p = p->Next)
	{
		uint64_t utcLeapSecond = p->Utc - 1;
		if (utc > utcLeapSecond)
		{
			*pCount = p->Count;
			if (unlockStruct()) return -1;
			return 0;
		}
		if (utc == utcLeapSecond)
		{
			switch(leap)
			{
				case 0:
					break;
				case 1:
					*pCount = p->Count;
					if (unlockStruct()) return -1;
					return 0;
				case -1:
					Log('e', "Ambiguous second");
					return -1;
			}
		}
	}
	if (unlockStruct()) return -1;
	*pCount = 0;
	return 0;
}
int LeapsTaiSecondsToCount(uint64_t tai, uint32_t *pCount, char *pLeap) {
	if (pLeapSeconds == NULL)
	{
		Log('e', "Leap seconds list not loaded");
		return -1;
	}
	if (readlockStruct()) return -1;
	for(struct LeapSecondStruct *p = pLeapSeconds; p != NULL; p = p->Next)
	{
		uint64_t utcLeapSecond = p->Utc - 1 + p->Count;
		if (tai > utcLeapSecond)
		{
			*pLeap = 0;
			*pCount = p->Count;
			if (unlockStruct()) return -1;
			return 0;
		}
		if (tai == utcLeapSecond)
		{
			*pLeap = 1;
			*pCount = p->Count;
			if (unlockStruct()) return -1;
			return 0;
		}
	}
	if (unlockStruct()) return -1;
	*pCount = 0;
	*pLeap = 0;
	return 0;
}

//Display Test
int LeapsDisplayLast(char *p) {
	uint64_t taiLeap = pLeapSeconds->Utc + pLeapSeconds->Count;
	int n = 0;
	for (uint64_t taiSeconds = taiLeap - 5; taiSeconds < taiLeap + 5; taiSeconds++)
	{
		uint64_t tai = taiSeconds << 24;
		uint64_t utc;
		char leap;
		if (TaiToUtc(tai, &utc, &leap)) break;
		uint64_t utcSeconds = utc >> 24;
		
		//Make string
		char sDate[30];
		UtcLeapToA(utc, leap, 30, sDate);
		
		n += sprintf(p + n, "Tai %lld\tUtc %lld\tLeap %d\ttai-utc %lld\t %s\n", taiSeconds, utcSeconds, leap, taiSeconds-utcSeconds, sDate);
	}
	return n;
}
int LeapsFetchTop(uint64_t *pTopLeapUtc, uint32_t *pTopLeapCount) {
	if (pLeapSeconds ==NULL)
	{
		Log('e', "Leap seconds not loaded");
		return -1;
	}
	*pTopLeapUtc = pLeapSeconds->Utc;
	*pTopLeapCount = pLeapSeconds->Count;
	return 0;
}

//Load
static int addLeapSecondToList(uint64_t utc, uint32_t count) {

	//Always writes from oldest (smallest utc) to latest (largest utc) 
	if (pLeapSeconds != NULL && pLeapSeconds->Utc >= utc) return 0;
	
	//Create the new more recent leap second
	struct LeapSecondStruct * p = malloc(sizeof(struct LeapSecondStruct));
	if (p == NULL)
	{
		LogErrno("Could not allocate memory ");
		return -1;
	}
	p->Utc = utc;
	p->Count = count;
	p->Next = pLeapSeconds;
	
	//Insert older leap second
	if (writelockStruct()) return -1;
	pLeapSeconds = p;
	if (unlockStruct()) return -1;
	
	return 0;
}
static int load       (char *cache) {

	//Write lock
	if (readlockFile()) return -1;

	//Open file
	FILE *fp = fopen(cache, "r");
	if (fp == NULL)
	{
		LogErrno("Error opening leap seconds file for reading");
		unlockStruct();
		unlockFile();
		return -1;
	}
	
	//Read each line
	uint64_t lastUpdatedSeconds;
	uint64_t expiresSeconds;
	char line[100];
	while (fgets(line, sizeof(line), fp))
	{
		if (strlen(line) > 2)
		{
			if (line[0] != '#')
			{
				//Make utc seconds and leap count
				uint64_t utc;
				uint32_t count;
				if (sscanf(line, "%llu %lu", &utc, (unsigned long *)&count) == 0)
				{
					Log('e', "Could not scan %s", line);
					fclose(fp);
					unlockStruct();
					unlockFile();
					return -1;
				}

				if (addLeapSecondToList(utc, count)) return -1;
			}
			else
			{
				if (line[1] == '$')
				{
					if (sscanf(&line[2], "%llu", &lastUpdatedSeconds) == 0)
					{
						Log('e', "Could not scan %s", line);
						fclose(fp);
						unlockStruct();
						unlockFile();
						return -1;
					}
				}
				if (line[1] == '@')
				{
					if (sscanf(&line[2], "%llu", &expiresSeconds) == 0)
					{
						Log('e', "Could not scan %s", line);
						fclose(fp);
						unlockStruct();
						unlockFile();
						return -1;
					}
				}
			}
		}
	}
	
	//Close the file
	fclose(fp);
	if (unlockFile()) return -1;
		
	//Save dates
	LastUpdatedSeconds = lastUpdatedSeconds;
	ExpiresSeconds = expiresSeconds;

	return 0;
}
static int loadIfNewer(char *cache) {
	time_t cachedate = FileDateOrZero(cache);
	if (CacheFileTime < cachedate)
	{
		if (load(cache)) return -1;
		CacheFileTime = cachedate;
	}
	return 0;
}

//FTP stuff
static   void splitUrl(char *p, char **ppHost, char **ppPath, char **ppFile) {

	*ppHost = p;            //Record the start of the host
	
	while(*p != '/') p++;   //Run forward to the first slash
	*p = 0;                 //Replace the slash with a null to end the host
	p++;                    //Move to the next character
	*ppPath = p;            //Record the start of the path
	
	while(*p !=  0 ) p++;   //Run forward to the end
	while(*p != '/') p--;   //Run back to the last slash
	*p = 0;                 //Replace the slash with a null to end the path
	p++;                    //Move to the next character
	*ppFile = p;            //Record the start of the file
}
static int downloadIfNewer(char *host, char *path, char *file, char *temp, char *cache, char *archive) {

	Log('i', "Checking %s/%s/%s for %s file", host, path, file, cache);

	//login
	int sck;
	int state = FtpLogin(host, &sck);
	if (state != FTP_LOGGED_IN) { FtpLogout(sck, state); return -1; }
	
	//Change to woking directory
	if (FtpCwd(sck, path)) { FtpLogout(sck, state); return -1; }
	
	//Get time of remte file
	time_t rft = FtpGetFileTime(sck, file);
	
	//Get time of local file
	time_t lft = FileDateOrZero(cache);
	
	//If the local file is newer then stop
	if (lft > rft)
	{
		if (LogIsAtLevel('i')) Log('i', "--- Local file is newer than remote file so nothing to do");
		else                   Log('w', "Leaps file is up to date");
		FtpLogout(sck, state);
		return 0;
	}
	
	//Download the remote file to a temporary file
	if (lft)
	{
		if (LogIsAtLevel('i')) Log('i', "--- Local file is out of date so downloading latest version");
		else                   Log('w', "Leaps file is out of date so downloading latest version");
	}
	else
	{
		if (LogIsAtLevel('i')) Log('i', "--- No local file so downloading a copy");
		else                   Log('w', "No leap seconds file is available so downloading a copy");
	}
	if (FtpDownload(sck, file, temp)) { FtpLogout(sck, state); return -1; }
	FtpLogout(sck, state);
	
	//Move the temporary file to the cache
	if (writelockFile()) return -1;
	if (lft) FileMoveOverWrite(cache, archive);
	FileMove(temp, cache);
	unlockFile();
	return 0;
}

//Public
uint64_t LeapsGetLastUpdatedSeconds1900(void) { //Used to take era into account when converting NTP timestamps
	return LastUpdatedSeconds;
}
     int LeapsGetCacheFileTime(time_t *t) {
	*t = CacheFileTime;
	return 0;
}
     int LeapsLoad(void) {
	CfgLock();
	char *url   = strdupa(CfgLeapSecondOnlineFileUTI);
	char *cache = strdupa(CfgLeapSecondCacheFileName);
	CfgUnlock();
	
	char *host;
	char *path;
	char *file;
	splitUrl(url, &host, &path, &file);
	
	char *end;
	
	char *temp = alloca(strlen(cache) + 6);
	end = stpcpy(temp, cache);
	strcpy(end, ".temp");
	
	char *arch = alloca(strlen(cache) + 6);
	end = stpcpy(arch, cache);
	strcpy(end, ".arch");
	
	if (downloadIfNewer(host, path, file, temp, cache, arch)) return -1;
	
	if (loadIfNewer(cache)) return -1;
	
	return 0;
}
