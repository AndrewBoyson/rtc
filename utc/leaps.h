#include <stdint.h>
#include <time.h>

extern  int LeapsUtcSecondsToCount(uint64_t utc, signed char isLeap, uint32_t *pCount);
extern  int LeapsTaiSecondsToCount(uint64_t tai,                     uint32_t *pCount, char *pIsLeap);
extern  int LeapsFetchTop         (                  uint64_t *pUtc, uint32_t *pCount);
extern  int LeapsDisplayLast      (char *);

extern uint64_t LeapsGetLastUpdatedSeconds1900(void);
extern  int     LeapsGetCacheFileTime(time_t *);
extern  int     LeapsLoad(void);
