#include <stdint.h>
#include <time.h>

//General
extern int RtcGetError                   (uint32_t *); extern int RtcRstError                   (void      );
extern int RtcGetBuildInfo  (int bufsize, char     *);
extern int RtcGetModelInfo  (int bufsize, char     *);
extern int RtcGetMakerInfo  (int bufsize, char     *);
extern int RtcGetTemperature             (float    *);
extern int RtcGetVoltage                 (float    *);
extern int RtcGetAmbient                 (float    *);
extern int RtcGetLED                     (char     *); extern int RtcSetLED                     (char      );
extern int RtcGetDebug0                  (int64_t  *); extern int RtcSetDebug0                  (int64_t   );
extern int RtcGetDebug1                  (int64_t  *); extern int RtcSetDebug1                  (int64_t   );
extern int RtcGetDebug2                  (int64_t  *); extern int RtcSetDebug2                  (int64_t   );
extern int RtcGetDebug3                  (int64_t  *); extern int RtcSetDebug3                  (int64_t   );
extern int RtcGetComment    (int bufsize, char     *); extern int RtcSetComment                 (char     *);
extern int RtcGetPollPeriod              (uint32_t *);

//Aging
extern int RtcGetAgingParamFo            (int32_t  *); extern int RtcSetAgingParamFo            (int32_t   );
extern int RtcGetAgingParamAo            (int32_t  *); extern int RtcSetAgingParamAo            (int32_t   );

//Regulator
extern int RtcGetRegulatorOutputPpb      (int32_t  *); extern int RtcSetRegulatorOutputPpb      (int32_t   );
extern int RtcGetRegulatorParamTo        (float    *); extern int RtcSetRegulatorParamTo        (float     );
extern int RtcGetRegulatorParamKo        (float    *); extern int RtcSetRegulatorParamKo        (float     );

//Slew
extern int RtcGetSlewAmountNs            (int32_t  *); extern int RtcSetSlewAmountNs            (int32_t   );
extern int RtcGetSlewRateNsPerS          (int32_t  *); extern int RtcSetSlewRateNsPerS          (int32_t   );

//Clock
extern int RtcGetClockIsSet              (char     *); extern int RtcRstClock                   (void      );
extern int RtcGetClockEpoch              (uint16_t *); extern int RtcSetClockEpoch              (uint16_t  );
extern int RtcGetClockNowTicNs           (uint64_t *);
extern int RtcGetClockNowIntNs           (uint64_t *);
extern int RtcGetClockNowAbsNs           (uint64_t *);
                                                       extern int RtcSetClockFromOffset         (int64_t   );

//Sample
extern int RtcGetSampleThisTicNs         (uint64_t *);
extern int RtcGetSampleThisIntNs         (uint64_t *);
extern int RtcGetSampleThisAbsNs         (uint64_t *);
extern int RtcGetSampleThisExtNs         (uint64_t *);
extern int RtcGetSampleLastTicNs         (uint64_t *);
extern int RtcGetSampleLastIntNs         (uint64_t *);
extern int RtcGetSampleLastAbsNs         (uint64_t *);
extern int RtcGetSampleLastExtNs         (uint64_t *);
extern int RtcGetSampleTemperature       (float    *);
                                                       extern int RtcSetSampleFromOffset        (int64_t   );
extern int RtcGetSampleTreated           (char     *);
extern int RtcGetSampleControlOn         (char     *); extern int RtcSetSampleControlOn         (char      );

//Rate loop
extern int RtcGetRateLoopParamKr         (int16_t  *); extern int RtcSetRateLoopParamKr         (int16_t   );
extern int RtcGetRateLoopParamMax        (int32_t  *); extern int RtcSetRateLoopParamMax        (int32_t   );
extern int RtcGetRateLoopInputDiffPpb    (int64_t  *);
extern int RtcGetRateLoopInputPeriodNs   (uint64_t *);
extern int RtcGetRateLoopOutputPpb       (int32_t  *);

//Time loop
extern int RtcGetTimeLoopParamKa         (int16_t  *); extern int RtcSetTimeLoopParamKa         (int16_t   );
extern int RtcGetTimeLoopParamMax        (int32_t  *); extern int RtcSetTimeLoopParamMax        (int32_t   );
extern int RtcGetTimeLoopParamSpreadSecs (int32_t  *); extern int RtcSetTimeLoopParamSpreadSecs (int32_t   );
extern int RtcGetTimeLoopInputAbsDiffNs  (int64_t  *);
extern int RtcGetTimeLoopOutputAmountNs  (int32_t  *);
extern int RtcGetTimeLoopOutputRateNsPerS(int32_t  *);

//Stuff used by NTP and sample
extern int RtcToTicks(uint64_t rtc, uint16_t epoch, uint64_t ticksPerSecond, uint64_t *pTicks);
extern int RtcGetNowTicks(uint16_t epoch, uint64_t ticksPerSecond, uint64_t *pTicks);
extern int RtcSetSampleOffsetTicks(uint64_t ticksPerSecond, int64_t ticks);
extern char RtcIsSet(void);
extern int RtcInit(void);