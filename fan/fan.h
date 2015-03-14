int FanSetOutput (uint16_t  value);
int FanGetOutput (uint16_t *value);
int FanGetDensity(uint16_t *value);
int FanGetOnTime ( int32_t *value);
int FanGetOffTime( int32_t *value);
int FanGetPulse  ( int32_t *value);
void FanScan(char stall);
void FanInit(void);
