#include <stdint.h> 

extern int CfgLoad(void);
extern int CfgLock(void);
extern int CfgUnlock(void);

extern char  *CfgFile;

extern char  *CfgSampleServer;
extern int    CfgSampleIntervalHours;
extern int    CfgSampleCount;
extern int    CfgSampleInterDelaySecs;
extern char  *CfgSampleLogFileName;
extern char  *CfgSampleNasPath;

extern char  *CfgNtpClientLogFileName;

extern char   CfgLogLevel;

extern char  *CfgLeapSecondOnlineFileUTI;
extern char  *CfgLeapSecondCacheFileName;
extern int    CfgLeapSecondNewFileCheckDays;

extern double CfgSysClkFreqLoopGain;

extern int    CfgFanOutputPeriodSecs;
extern int    CfgFanPulsePeriodMs;
extern double CfgFanMinRunSpeed;
extern double CfgFanMinStartSpeed;
extern char  *CfgFanCurveFileName;
extern double CfgFanCurveCorrectionDivisor;
extern int    CfgFanCurveCorrectionCount;

extern double CfgFanProportionalGain;
extern double CfgFanIntegralTime;
extern double CfgFanDerivativeTime;
extern char  *CfgFanLogFileName;