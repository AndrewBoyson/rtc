#include "utc.h"
#include "leaps.h"
#include "../global.h"
#include "../lib/log.h"
#include "../lib/cfg.h"
#include "../lib/socket.h"
#include "../lib/datetime.h"
#include "../lib/file.h"

/*UTC and TAI time - count of 2^-24 seconds from epoch 1900
      +----------u32---------+---------u32---------+
      |     NTP Seconds      |    NTP Fractions    | 64 bit NTP timestamp valid for 130   years with 0.2ns resolution
      +----------------------+---------------------+
+-u8--+----------u32---------+---------u32---------+
| Era |     NTP Seconds      |    NTP Fractions    | 72 bit     timestamp valid for 33280 years with 0.2ns resolution
+-----+----------------------+---------------------+
+-------------u40------------+------u24------+
|           Seconds          |   Fractions   |       64 bit UTC timestamp valid for 33280 years with  60ns resolution
+----------------------------+---------------+
*/

const uint16_t UtcEpoch       = 1900;
const uint64_t UtcTicksPerSec = SIZE_24_BITS;

//Conversions 
uint64_t NtpToUtc(uint64_t ntp) { //Converts the NTP value to UTC and takes the era into account
	
	uint64_t lastUpdatedUtc = LeapsGetLastUpdatedSeconds1900() << 24;
	
	//Convert the ntp date to UTC format using the current era from the leaps file
	uint64_t utc = (ntp >> 8) + (lastUpdatedUtc & 0xFF00000000000000ULL);
	
	//if the ntp date is less than the leaps file date then add an era
	if (utc < lastUpdatedUtc) utc += 0x0100000000000000ULL;
	
	return utc;
}
uint64_t UtcToNtp(uint64_t utc) {
	return utc << 8;
}
time_t   UtcToT  (uint64_t utc) {
	return (utc >> 24) - SECONDS_1900_1970;
}

int UtcToTai   (uint64_t utc,  signed char leap,  uint64_t *pTai             ) {
	uint64_t utcSeconds = utc >> 24;
	uint32_t count;
	if (LeapsUtcSecondsToCount(utcSeconds, leap, &count)) return -1;
	*pTai = utc + ((uint64_t)count << 24);
	return 0;
}
int TaiToUtc   (uint64_t tai,                     uint64_t *pUtc, char *pLeap) {
	uint64_t taiSeconds = tai >> 24;
	uint32_t count;
	if (LeapsTaiSecondsToCount(taiSeconds, &count, pLeap)) return -1;
	*pUtc = tai - ((uint64_t)count << 24);
	return 0;
}
int UtcLeapToTm(uint64_t utc,  char isLeap, struct tm *ptm) {
	time_t t;
	t = UtcToT(utc);                      //Make time_t from utc
	if (DateTimeTtoTm(t, ptm)) return -1; //Make broken time from time_t
	if (isLeap) ptm->tm_sec++;            //Take seconds from 59 to 60 if leap
	return 0;
}
int UtcLeapToA (uint64_t utc,  char isLeap, int size, char *sDate) {
	struct tm tm;
	if (UtcLeapToTm(utc, isLeap, &tm)) return -1;
	DateTimeTmToA(tm, size, sDate);       //Make string from broken time
	
	return 0;
}
int UtcLeapToS (uint64_t utc,  char isLeap, int size, char *sDate) {
	struct tm tm;
	if (UtcLeapToTm(utc, isLeap, &tm)) return -1;
	DateTimeTmToS(tm, size, sDate);       //Make string from broken time
	return 0;
}
int UtcLeapToF(char *format, uint64_t utc,  char isLeap, int size, char *sDate) {
	struct tm tm;
	if (UtcLeapToTm(utc, isLeap, &tm)) return -1;
	DateTimeTmToF(tm, format, size, sDate);       //Make string from broken time	
	return 0;
}
int UtcToTicks(uint64_t rtc, uint16_t epoch, uint64_t ticksPerSecond, uint64_t *pTicks) {
	return DateTimeTicksToTicks(UtcEpoch, UtcTicksPerSec, rtc, epoch, ticksPerSecond, pTicks);
}

int UtcInit(void) {
	return LeapsLoad();
}