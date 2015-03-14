#include <stdint.h>    //int64_t etc
#include <time.h>      //gmtime_r, timegm, strftime, time, struct tm
#include <sys/time.h>  //struct timeval, gettimeofday, settimeofday, adjtime, timeradd
#include <sys/timex.h> //struct timex adjtimex

#include "../global.h"
#include "log.h"

/*UNIX time_t and struct tm - seconds from epoch of 1970
struct timeval {
    time_t      tv_sec;  long
    suseconds_t tv_usec; long
} tv
time_t t
UNIX time_t can hold up to 2^32 seconds since 1970 so, if unsigned, will rollover in 2106.
This is ok so long as using uint64_t ticks rather than int64_t ticks.
*/
#define MAX_OFFSET_SECONDS_BEFORE_SLEW   10

//Conversion utilities
int DateTimeTtoTm(   time_t t, struct tm *ptm) {
	if (gmtime_r(&t, ptm) == NULL)
	{
		LogErrno ("ERROR with gmtime_r");
		return -1;
	}
	return 0;
}
int DateTimeTmToT(struct tm tm,   time_t *pt)  {
	*pt = timegm(&tm);
	if (*pt == -1)
	{
		LogErrno ("ERROR with timegm");
		return -1;
	}
	return 0;
}
int DateTimeTmToA(struct tm tm, int size, char *sDate) { //Displays 2013 Feb  1 Fri 16:31:14
	if (strftime(sDate, size, "%Y %b %e %a %H:%M:%S", &tm) == -1)
	{
		LogErrno ("ERROR with strftime");
		return -1;
	}
	return 0;
}
int DateTimeTmToS(struct tm tm, int size, char *sDate) { //Displays 2013/02/01 16:31:14
	if (strftime(sDate, size, "%Y/%m/%d %H:%M:%S", &tm) == -1)
	{
		LogErrno ("ERROR with strftime");
		return -1;
	}
	return 0;
}
int DateTimeTmToF(struct tm tm, char *format, int size, char *sDate) { //Displays tm as a formatted string
	if (strftime(sDate, size, format, &tm) == -1)
	{
		LogErrno ("ERROR with strftime");
		return -1;
	}
	return 0;
}

int DateTimeScaleTvToTicks(         uint64_t ticksPerSecond, struct timeval tv, uint64_t *pticks) {

	switch (ticksPerSecond)
	{
		case SIZE_32_BITS:
			*pticks  =  (uint64_t)(uint32_t)tv.tv_sec  << 32;
			*pticks += ((uint64_t)(uint32_t)tv.tv_usec << 32) / ONE_MILLION;
			break;
		case SIZE_24_BITS:
			*pticks  =  (uint64_t)(uint32_t)tv.tv_sec  << 24;
			*pticks += ((uint64_t)(uint32_t)tv.tv_usec << 24) / ONE_MILLION;
			break;
		case 1000:
			*pticks  = tv.tv_sec  * 1000ULL;
			*pticks += tv.tv_usec / 1000ULL;
			break;		
		case ONE_MILLION:
			*pticks  = tv.tv_sec  * ONE_MILLION;
			*pticks += tv.tv_usec;
			break;
		case ONE_BILLION:
			*pticks  = tv.tv_sec  * ONE_BILLION;
			*pticks += tv.tv_usec * 1000ULL;
			break;
		default:
			*pticks  = tv.tv_sec  * ticksPerSecond;
			*pticks += tv.tv_usec * ticksPerSecond / ONE_MILLION;
			break;
	}
	return 0;
}
int DateTimeScaleTicksToTv(         uint64_t ticksPerSecond, uint64_t ticks, struct timeval *ptv) {
	
	uint64_t tickRemainder;
	switch (ticksPerSecond)
	{
		case SIZE_32_BITS:
			ptv->tv_sec = ticks >> 32;                          //Take the seconds  part of ticks
			tickRemainder = ticks & ALL_32_BITS;                //Take the fraction part of ticks
			ptv->tv_usec = (tickRemainder * ONE_MILLION) >> 32; //We know that 2^32 = one million usec so multiply by one million and divide by 2^32
			break;
		case SIZE_24_BITS:
			ptv->tv_sec = ticks >> 24;                          //Take the seconds  part of ticks
			tickRemainder = ticks & ALL_24_BITS;                //Take the fraction part of ticks
			ptv->tv_usec = (tickRemainder * ONE_MILLION) >> 24; //We know that 2^24 = one million usec so multiply by one million and divide by 2^24
			break;
		case 1000:
			ptv->tv_sec = ticks / 1000;
			tickRemainder = ticks - (ptv->tv_sec * 1000);
			ptv->tv_usec = tickRemainder * 1000;
			break;
		case ONE_MILLION:
			ptv->tv_sec = ticks / ONE_MILLION;
			tickRemainder = ticks - (ptv->tv_sec * ONE_MILLION);
			ptv->tv_usec = tickRemainder;
			break;
		case ONE_BILLION:
			ptv->tv_sec = ticks / ONE_BILLION;
			tickRemainder = ticks - (ptv->tv_sec * ONE_BILLION);
			ptv->tv_usec = tickRemainder / 1000;
			break;
		default:
			ptv->tv_sec = ticks / ticksPerSecond;
			tickRemainder = ticks - (ptv->tv_sec * ticksPerSecond);
			ptv->tv_usec = tickRemainder * ONE_MILLION / ticksPerSecond;
			break;
	}
	return 0;
}
int DateTimeScaleSignedTvToTicks(   uint64_t ticksPerSecond, struct timeval tv,  int64_t *pticks) {
	char isNegative = tv.tv_sec < 0;
	if (isNegative) //timeval usec is always +ve so -1.4 is -2s + 0.6us
	{
		tv.tv_sec  = -tv.tv_sec - 1;
		tv.tv_usec = ONE_MILLION - tv.tv_usec;
	}
	DateTimeScaleTvToTicks(ticksPerSecond, tv, (uint64_t *)pticks);

	if (isNegative) *pticks = -*pticks;
	
	return 0;
}
int DateTimeScaleSignedTicksToTv(   uint64_t ticksPerSecond,  int64_t ticks, struct timeval *ptv) {
	char isNegative = ticks < 0;
	if (isNegative) ticks = -ticks;
	
	DateTimeScaleTicksToTv(ticksPerSecond, (uint64_t)ticks, ptv);
	
	if (isNegative) //timeval usec is always +ve so -1.4 is -2s + 0.6us
	{
		ptv->tv_sec  = -ptv->tv_sec - 1;
		ptv->tv_usec = ONE_MILLION - ptv->tv_usec;
	}
	return 0;
}
int DateTimeScaleSignedTicksToTicks(uint64_t ticksPerSecondFrom,   int64_t ticksFrom, uint64_t ticksPerSecondTo,  int64_t *pTicksTo) {
	struct timeval tv;
	if (DateTimeScaleSignedTicksToTv(ticksPerSecondFrom, ticksFrom, &tv)) return -1;
	if (DateTimeScaleSignedTvToTicks(ticksPerSecondTo,   tv,   pTicksTo)) return -1;
	return 0;
}

int DateTimeTvToTicks     (uint16_t epoch, uint64_t ticksPerSecond, struct timeval tv,  uint64_t *pticks) {

	switch (epoch)
	{
		case 1970:                                 break; //timeval epoch is already 1970
		case 1900: tv.tv_sec += SECONDS_1900_1970; break; //add    seconds between 1900 and 1970
		case 2000: tv.tv_sec -= SECONDS_2000_1970; break; //remove seconds between 1970 and 2000
		default:
			Log('e', "Epoch %d not recognised", epoch);
			return -1;
	}
	if (DateTimeScaleTvToTicks(ticksPerSecond, tv, pticks)) return -1;
	
	return 0;
}
int DateTimeTicksToTv     (uint16_t epoch, uint64_t ticksPerSecond,  uint64_t ticks, struct timeval *ptv) {

	if (DateTimeScaleTicksToTv(ticksPerSecond, ticks, ptv)) return -1;
	switch (epoch)
	{
		case 1970:                                   break; //timeval epoch is already 1970
		case 1900: ptv->tv_sec -= SECONDS_1900_1970; break; //remove seconds between 1900 and 1970
		case 2000: ptv->tv_sec += SECONDS_2000_1970; break; //add seconds between 1970 and 2000
		default:
			Log('e', "Epoch %d not recognised", epoch);
			return -1;
	}
	return 0;
}

int DateTimeScaleTicksToTicks(                    uint64_t ticksPerSecondFrom,  int64_t ticksFrom,                   uint64_t ticksPerSecondTo,  int64_t *pTicksTo) {
	struct timeval tv;
	if (DateTimeScaleSignedTicksToTv(ticksPerSecondFrom, ticksFrom, &tv)) return -1;
	if (DateTimeScaleSignedTvToTicks(ticksPerSecondTo,   tv,   pTicksTo)) return -1;
	return 0;
}
int DateTimeTicksToTicks     (uint16_t epochFrom, uint64_t ticksPerSecondFrom, uint64_t ticksFrom, uint16_t epochTo, uint64_t ticksPerSecondTo, uint64_t *pTicksTo) {
	struct timeval tv;
	if (DateTimeTicksToTv(epochFrom, ticksPerSecondFrom, ticksFrom, &tv)) return -1;
	if (DateTimeTvToTicks(epochTo,   ticksPerSecondTo,   tv,   pTicksTo)) return -1;
	return 0;
}

//UTC now
int DateTimeNowT (time_t *pt)  {
	if (time(pt) == -1)
	{
		LogErrno ("ERROR with time");
		return -1;
	}
	return 0;
}
int DateTimeNowTm(struct tm *ptm) {
	time_t t = time(NULL);
	if (DateTimeTtoTm(t, ptm)) return -1;
	return 0;
}
int DateTimeNowTicks(uint16_t epoch, uint64_t ticksPerSecond, uint64_t *pticks) {
	struct timeval tv;
	if (gettimeofday(&tv, NULL))
	{
		LogErrno ("ERROR with gettimeofday");
		return -1;
	}
	if (DateTimeTvToTicks(epoch, ticksPerSecond, tv, pticks)) return -1;	
	return 0;
}
int DateTimeNowA(int size, char *sDate) { //Displays 2013 Feb  1 Fri 16:31:14
	struct tm tm;
	if (DateTimeNowTm(&tm)) return -1;
	if (DateTimeTmToA(tm, size, sDate)) return -1;
	return 0;
}
int DateTimeNowS(int size, char *sDate) { //Displays 2013/02/01 16:31:14
	struct tm tm;
	if (DateTimeNowTm(&tm)) return -1;
	if (DateTimeTmToS(tm, size, sDate)) return -1;
	return 0;
}
int DateTimeNowF(char *format, int size, char *sDate) { //Displays now as a formatted string
	struct tm tm;
	if (DateTimeNowTm(&tm)) return -1;
	if (DateTimeTmToF(tm, format, size, sDate)) return -1;
	return 0;
}

//Clock set utility
#define ADJ_OFFSET            0x0001 // time offset
#define ADJ_FREQUENCY         0x0002 // frequency offset
#define ADJ_MAXERROR          0x0004 // maximum time error
#define ADJ_ESTERROR          0x0008 // estimated time error
#define ADJ_STATUS            0x0010 // clock status
#define ADJ_TIMECONST         0x0020 // pll time constant
#define ADJ_TICK              0x4000 // tick value
#define ADJ_OFFSET_SINGLESHOT 0x8001 // old-fashioned adjtime()

#define TIME_OK   0 /* clock synchronized */
//#define TIME_INS  1 /* insert leap second */
//#define TIME_DEL  2 /* delete leap second */
//#define TIME_OOP  3 /* leap second in progress */
//#define TIME_WAIT 4 /* leap second has occurred */
//#define TIME_BAD  5 /* clock not synchronized */


#define KERNEL_FREQ_LIMIT_PPM 512    //observedDrift value is clamped (kept within bounds) to the kernel frequency offset limit, that is +/-512ppm.
/*struct timex {
    int modes;           // mode selector
    long offset;         // time offset (usec)
    long freq;           // frequency offset (scaled ppm. The scaling is 1 << SHIFT_USEC where SHIFT_USEC = 16.)
    long maxerror;       // maximum error (usec)
    long esterror;       // estimated error (usec)
    int status;          // clock command/status
    long constant;       // pll time constant
    long precision;      // clock precision (usec) (read-only)
    long tolerance;      // clock frequency tolerance (ppm)(read-only)
    struct timeval time; // current time (read-only)
    long tick;           // usecs between clock ticks
};
*/
int DateTimeGetFreqOffsetPpm(double *freqOffsetPpm) {
	struct timex timex;
	timex.modes = 0;
	int state = adjtimex(&timex);
	if (state  < 0)
	{
		LogErrno("ERROR getting freq offset using adjtimex: ");
		return -1;
	}
	*freqOffsetPpm = (double)timex.freq / SIZE_16_BITS;
	return 0;
}
int DateTimeSetFreqOffsetPpm(double  freqOffsetPpm) {

	struct timex timex;
	double limitedFreqOffsetPpm = freqOffsetPpm;
	if      (limitedFreqOffsetPpm >  KERNEL_FREQ_LIMIT_PPM)
	{
		limitedFreqOffsetPpm =  KERNEL_FREQ_LIMIT_PPM;
		Log('i', "Limited system clock freq to %+1.0f ppm", limitedFreqOffsetPpm);
	}
	else if (limitedFreqOffsetPpm < -KERNEL_FREQ_LIMIT_PPM)
	{
		limitedFreqOffsetPpm = -KERNEL_FREQ_LIMIT_PPM;
		Log('i', "Limited system clock freq to %+1.0f ppm", limitedFreqOffsetPpm);
	}
	else
	{
		//Log('i', "Setting system clock freq to %+1.1f ppm", limitedFreqOffsetPpm);
		timex.status = TIME_OK;
		timex.modes = ADJ_STATUS;
		int state = adjtimex(&timex);
		if (state  < 0)
		{
			LogErrno("ERROR setting status to TIME_OK using adjtimex: ");
			return -1;
		}
	}
	timex.freq = (long)(limitedFreqOffsetPpm * SIZE_16_BITS);
	timex.modes = ADJ_FREQUENCY;
	int state = adjtimex(&timex);
	if (state  < 0)
	{
		LogErrno("ERROR setting freq offset using adjtimex: ");
		return -1;
	}
	if (state) Log('w', "System clock state is %d", state);
	return 0;
}
int DateTimeSetTicksOffset(uint64_t ticksPerSecond, int64_t ticksOffset) {
	struct timeval tvOffset;
	if (DateTimeScaleSignedTicksToTv(ticksPerSecond, ticksOffset, &tvOffset)) return -1;
	if (tvOffset.tv_sec > MAX_OFFSET_SECONDS_BEFORE_SLEW || tvOffset.tv_sec < -MAX_OFFSET_SECONDS_BEFORE_SLEW )
	{
		Log('w', "Resetting system clock by %d seconds", tvOffset.tv_sec); 
		struct timeval tv;
		if (gettimeofday(&tv, NULL))
		{
			LogErrno ("ERROR with gettimeofday");
			return -1;
		}
		struct timeval tvNew;
		timeradd(&tv, &tvOffset, &tvNew);
		if (settimeofday(&tvNew, NULL))
		{
			LogErrno ("ERROR with settimeofday");
			return -1;
		}
	}
	else
	{
		//Log('i', "Slewing system clock time by %+1.1fms", tvOffset.tv_sec * 1000.0 + tvOffset.tv_usec / 1000.0);
		if(adjtime(&tvOffset, NULL))
		{
			LogErrno ("ERROR slewing system clock using adjtime");
			return -1;
		}
	}
	return 0;
}
int32_t DateTimeDaysBetweenTwoEpochs(uint16_t epochFrom, uint16_t epochTo) {
	
	uint32_t o, n;
	if (epochFrom > epochTo)
	{
		o = epochTo;
		n = epochFrom;
	}
	else         
	{
		n = epochTo;
		o = epochFrom;
	}
	int32_t days = 0;
	for (uint16_t year = o; year < n; year++)
	{ //is leap id divisible by 4 unless divisble by 10 unless divisible by 400
		char div4   = year %   4 == 0; 
		char div100 = year % 100 == 0; 
		char div400 = year % 400 == 0; 
		days += div4 && (!div100 || div400) ? 366 : 365;
	}
	
	if (epochFrom > epochTo) return -days;
	                         return  days;
}