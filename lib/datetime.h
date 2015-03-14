#include <time.h>
#include <sys/time.h>
#include <stdint.h>


extern int DateTimeTtoTm(   time_t t , struct tm *ptm);
extern int DateTimeTmToT(struct tm tm, int32_t *pt);
extern int DateTimeTmToA(struct tm tm, int size, char *sDate);
extern int DateTimeTmToS(struct tm tm, int size, char *sDate);
extern int DateTimeTmToF(struct tm tm, char *format, int size, char *sDate);

extern int DateTimeScaleTvToTicks      (                    uint64_t ticksPerSecond,    struct timeval tv,    uint64_t *pticks);
extern int DateTimeScaleTicksToTv      (                    uint64_t ticksPerSecond,    uint64_t ticks,    struct timeval *ptv);
extern int DateTimeScaleSignedTvToTicks(                    uint64_t ticksPerSecond,    struct timeval tv,     int64_t *pticks);
extern int DateTimeScaleSignedTicksToTv(                    uint64_t ticksPerSecond,     int64_t ticks,    struct timeval *ptv);


extern int DateTimeTvToTicks        (uint16_t epoch,     uint64_t ticksPerSecond,     struct timeval tv,    uint64_t *pticks);
extern int DateTimeTicksToTv        (uint16_t epoch,     uint64_t ticksPerSecond,     uint64_t ticks,    struct timeval *ptv);

extern int DateTimeScaleTicksToTicks(                    uint64_t ticksPerSecondFrom,  int64_t ticksFrom,                   uint64_t ticksPerSecondTo,  int64_t *ticksTo);
extern int DateTimeTicksToTicks     (uint16_t epochFrom, uint64_t ticksPerSecondFrom, uint64_t ticksFrom, uint16_t epochTo, uint64_t ticksPerSecondTo, uint64_t *ticksTo);

extern int DateTimeNowT (time_t *pt);
extern int DateTimeNowTm(struct tm *ptm);
extern int DateTimeNowTicks(uint16_t epoch, uint64_t ticksPerSecond, uint64_t *ticks);
extern int DateTimeNowA(int size, char *sDate);
extern int DateTimeNowS(int size, char *sDate);
extern int DateTimeNowF(char *format, int size, char *sDate);

extern int DateTimeGetFreqOffsetPpm(double *freqOffsetPpm);
extern int DateTimeSetFreqOffsetPpm(double  freqOffsetPpm);
extern int DateTimeSetTicksOffset(uint64_t ticksPerSecond, int64_t ticksOffset);

extern int32_t DateTimeDaysBetweenTwoEpochs(uint16_t epochFrom, uint16_t epochTo);