#include <stdint.h>    //int64_t
#include <sys/time.h>  //struct timeval, gettimeofday, settimeofday, adjtime, timeradd

#include "../global.h"
#include "spi.h"
#include "datetime.h"
#include "log.h"

#define TICKS_PER_SECOND ONE_BILLION
#define STD_DELAY_US 800

//General
int RtcGetError                   (uint32_t *value) { return SpiRead4(0x10,          STD_DELAY_US, value); } int RtcRstError                   (void           ) { return SpiSend0(0x10 , STD_DELAY_US       ); }
int RtcGetBuildInfo  (int bufsize, char     *value) { return SpiReadS(0x11, bufsize, STD_DELAY_US, value); }
int RtcGetModelInfo  (int bufsize, char     *value) { return SpiReadS(0x12, bufsize, STD_DELAY_US, value); }
int RtcGetMakerInfo  (int bufsize, char     *value) { return SpiReadS(0x13, bufsize, STD_DELAY_US, value); }
int RtcGetTemperature             (float    *value) { return SpiRead4(0x14,          STD_DELAY_US, value); }
int RtcGetVoltage                 (float    *value) { return SpiRead4(0x15,          STD_DELAY_US, value); }
int RtcGetAmbient                 (float    *value) { return SpiRead4(0x16,          STD_DELAY_US, value); }
int RtcGetLED                     (char     *value) { return SpiRead1(0x17,          STD_DELAY_US, value); } int RtcSetLED                     (char      value) { return SpiSend1(0x17, STD_DELAY_US, &value); }
int RtcGetDebug0                  (int64_t  *value) { return SpiRead8(0x18,          STD_DELAY_US, value); } int RtcSetDebug0                  (int64_t   value) { return SpiSend8(0x18, STD_DELAY_US, &value); }
int RtcGetDebug1                  (int64_t  *value) { return SpiRead8(0x19,          STD_DELAY_US, value); } int RtcSetDebug1                  (int64_t   value) { return SpiSend8(0x19, STD_DELAY_US, &value); }
int RtcGetDebug2                  (int64_t  *value) { return SpiRead8(0x1A,          STD_DELAY_US, value); } int RtcSetDebug2                  (int64_t   value) { return SpiSend8(0x1A, STD_DELAY_US, &value); }
int RtcGetDebug3                  (int64_t  *value) { return SpiRead8(0x1B,          STD_DELAY_US, value); } int RtcSetDebug3                  (int64_t   value) { return SpiSend8(0x1B, STD_DELAY_US, &value); }
int RtcGetComment    (int bufsize, char     *value) { return SpiReadS(0x1C, bufsize, STD_DELAY_US, value); } int RtcSetComment                 (char     *value) { return SpiSendS(0x1C, STD_DELAY_US,  value); }
int RtcGetPollPeriod              (uint32_t *value) { return SpiRead4(0x1D,          STD_DELAY_US, value); }

//Aging
int RtcGetAgingParamFo            (int32_t  *value) { return SpiRead4(0x20,          STD_DELAY_US, value); } int RtcSetAgingParamFo            (int32_t   value) { return SpiSend4(0x20, STD_DELAY_US, &value); }
int RtcGetAgingParamAo            (int32_t  *value) { return SpiRead4(0x21,          STD_DELAY_US, value); } int RtcSetAgingParamAo            (int32_t   value) { return SpiSend4(0x21, STD_DELAY_US, &value); }

//Regulator
int RtcGetRegulatorParamTo        (float    *value) { return SpiRead4(0x22,          STD_DELAY_US, value); } int RtcSetRegulatorParamTo        (float     value) { return SpiSend4(0x22, STD_DELAY_US, &value); }
int RtcGetRegulatorParamKo        (float    *value) { return SpiRead4(0x23,          STD_DELAY_US, value); } int RtcSetRegulatorParamKo        (float     value) { return SpiSend4(0x23, STD_DELAY_US, &value); }
int RtcGetRegulatorOutputPpb      (int32_t  *value) { return SpiRead4(0x24,          STD_DELAY_US, value); }

//Slew
int RtcGetSlewAmountNs            (int32_t  *value) { return SpiRead4(0x25,          STD_DELAY_US, value); } int RtcSetSlewAmountNs            (int32_t   value) { return SpiSend4(0x25, STD_DELAY_US, &value); }
int RtcGetSlewRateNsPerS          (int32_t  *value) { return SpiRead4(0x26,          STD_DELAY_US, value); } int RtcSetSlewRateNsPerS          (int32_t   value) { return SpiSend4(0x26, STD_DELAY_US, &value); }

//Clock
int RtcGetClockIsSet              (char     *value) { return SpiRead1(0x30,          STD_DELAY_US, value); } int RtcRstClock                   (void           ) { return SpiSend0(0x30, STD_DELAY_US        ); }
int RtcGetClockEpoch              (uint16_t *value) { return SpiRead2(0x31,          STD_DELAY_US, value); } int RtcSetClockEpoch              (uint16_t  value) { return SpiSend2(0x31, STD_DELAY_US, &value); }
int RtcGetClockNowTicNs           (uint64_t *value) { return SpiRead8(0x32,          STD_DELAY_US, value); }
int RtcGetClockNowIntNs           (uint64_t *value) { return SpiRead8(0x33,          STD_DELAY_US, value); }
int RtcGetClockNowAbsNs           (uint64_t *value) { return SpiRead8(0x34,          STD_DELAY_US, value); }
                                                                                                             int RtcSetClockFromOffset         (int64_t   value) { return SpiSend8(0x35, STD_DELAY_US, &value); }
//Sample
int RtcGetSampleThisTicNs         (uint64_t *value) { return SpiRead8(0x40,          STD_DELAY_US, value); }
int RtcGetSampleThisIntNs         (uint64_t *value) { return SpiRead8(0x41,          STD_DELAY_US, value); }
int RtcGetSampleThisAbsNs         (uint64_t *value) { return SpiRead8(0x42,          STD_DELAY_US, value); }
int RtcGetSampleThisExtNs         (uint64_t *value) { return SpiRead8(0x43,          STD_DELAY_US, value); }
int RtcGetSampleLastTicNs         (uint64_t *value) { return SpiRead8(0x44,          STD_DELAY_US, value); }
int RtcGetSampleLastIntNs         (uint64_t *value) { return SpiRead8(0x45,          STD_DELAY_US, value); }
int RtcGetSampleLastAbsNs         (uint64_t *value) { return SpiRead8(0x46,          STD_DELAY_US, value); }
int RtcGetSampleLastExtNs         (uint64_t *value) { return SpiRead8(0x47,          STD_DELAY_US, value); }
int RtcGetSampleTemperature       (float    *value) { return SpiRead4(0x48,          STD_DELAY_US, value); }
                                                                                                             int RtcSetSampleFromOffset        (int64_t   value) { return SpiSend8(0x49,         2000, &value); }
int RtcGetSampleTreated           (char     *value) { return SpiRead1(0x4A,          STD_DELAY_US, value); }
int RtcGetSampleControlOn         (char     *value) { return SpiRead1(0x4B,          STD_DELAY_US, value); } int RtcSetSampleControlOn         (char      value) { return SpiSend1(0x4B, STD_DELAY_US, &value); }

//Rate loop
int RtcGetRateLoopParamKr         (int16_t  *value) { return SpiRead2(0x50,          STD_DELAY_US, value); } int RtcSetRateLoopParamKr         (int16_t   value) { return SpiSend2(0x50, STD_DELAY_US, &value); }
int RtcGetRateLoopParamMax        (int32_t  *value) { return SpiRead4(0x51,          STD_DELAY_US, value); } int RtcSetRateLoopParamMax        (int32_t   value) { return SpiSend4(0x51, STD_DELAY_US, &value); }
int RtcGetRateLoopInputDiffPpb    (int64_t  *value) { return SpiRead8(0x52,          STD_DELAY_US, value); }
int RtcGetRateLoopInputPeriodNs   (uint64_t *value) { return SpiRead8(0x53,          STD_DELAY_US, value); }
int RtcGetRateLoopOutputPpb       (int32_t  *value) { return SpiRead4(0x54,          STD_DELAY_US, value); }

//Time loop
int RtcGetTimeLoopParamKa         (int16_t  *value) { return SpiRead2(0x60,          STD_DELAY_US, value); } int RtcSetTimeLoopParamKa         (int16_t   value) { return SpiSend2(0x60, STD_DELAY_US, &value); }
int RtcGetTimeLoopParamMax        (int32_t  *value) { return SpiRead4(0x61,          STD_DELAY_US, value); } int RtcSetTimeLoopParamMax        (int32_t   value) { return SpiSend4(0x61, STD_DELAY_US, &value); }
int RtcGetTimeLoopParamSpreadSecs (int32_t  *value) { return SpiRead4(0x62,          STD_DELAY_US, value); } int RtcSetTimeLoopParamSpreadSecs (int32_t   value) { return SpiSend4(0x62, STD_DELAY_US, &value); }
int RtcGetTimeLoopInputAbsDiffNs  (int64_t  *value) { return SpiRead8(0x63,          STD_DELAY_US, value); }
int RtcGetTimeLoopOutputAmountNs  (int32_t  *value) { return SpiRead4(0x64,          STD_DELAY_US, value); }
int RtcGetTimeLoopOutputRateNsPerS(int32_t  *value) { return SpiRead4(0x65,          STD_DELAY_US, value); }

//Stuff used by NTP and sample
int  RtcToTicks(uint64_t rtc, uint16_t epoch, uint64_t ticksPerSecond, uint64_t *pTicks) {
	uint16_t rtcEpoch;
	if (RtcGetClockEpoch(&rtcEpoch)) return -1;
	return DateTimeTicksToTicks(rtcEpoch, TICKS_PER_SECOND, rtc, epoch, ticksPerSecond, pTicks); //This will throw out epochs other than 2000, 1970 and 1900
}
int  RtcGetNowTicks(uint16_t epoch, uint64_t ticksPerSecond, uint64_t *pTicks) {
	uint64_t rtc;
	if (RtcGetClockNowAbsNs(&rtc)) return -1;
	if (RtcToTicks(rtc, epoch, ticksPerSecond, pTicks)) return -1;
	return 0;
}
int  RtcSetSampleOffsetTicks(uint64_t ticksPerSecond, int64_t ticks) {
	//Scale the offset from NTP (32bit) to RTC (ns)
	int64_t rtcOffsetNs;
	if (DateTimeScaleTicksToTicks(ticksPerSecond, ticks, TICKS_PER_SECOND, &rtcOffsetNs)) return -1;

	//Send the sample
	if (RtcSetSampleFromOffset(rtcOffsetNs)) return -1;

	return 0;
}
char RtcIsSet(void) {
	char rtcIsSet;
	if (RtcGetClockIsSet(&rtcIsSet)) return 0;
	return rtcIsSet;
}
int RtcInit(void) {
	uint16_t rtcEpoch;
	if (RtcGetClockEpoch(&rtcEpoch)) return -1;
	
	if (!rtcEpoch)
	{
		Log('w', "RTC epoch was not set - setting to 1970");
		if (RtcSetClockEpoch(1970)) return -1;
	}
	
	return 0;	
}