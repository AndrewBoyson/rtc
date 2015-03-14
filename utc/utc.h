#include <stdint.h>
extern const uint16_t UtcEpoch;
extern const uint64_t UtcTicksPerSec;

extern uint64_t NtpToUtc(uint64_t ntp);
extern uint64_t UtcToNtp(uint64_t utc);

extern  int UtcToTai(uint64_t utc, signed char isLeap, uint64_t *pTai             );
extern  int TaiToUtc(uint64_t tai,                     uint64_t *pUtc, char *pLeap);
extern  int UtcLeapToA(uint64_t utc,  char isLeap, int size, char *sDate);
extern  int UtcLeapToS(uint64_t utc,  char isLeap, int size, char *sDate);
extern  int UtcLeapToF(char *format, uint64_t utc,  char isLeap, int size, char *sDate);
extern  int UtcToTicks(uint64_t rtc, uint16_t epoch, uint64_t ticksPerSecond, uint64_t *pTicks);
extern  int UtcInit(void);
