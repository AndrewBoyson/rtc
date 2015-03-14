extern double SysClkOffsetSeconds;
extern double SysClkFrequencyPpm;
extern int  SysClkSetTicksOffset(void);
extern void SysClkGo  (void);
extern void SysClkKill(void);
extern void SysClkInit(char *name, int priority);
extern void SysClkJoin(void);
