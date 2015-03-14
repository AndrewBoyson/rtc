extern  int CurveGetOutputFromHeating(float ambient, float heating, uint16_t *pDensity);
extern  int CurveGetHeatingFromOutput(float ambient, uint16_t density, float *pHeating);
extern  int CurveAddHeatingAtOutput  (float ambient, uint16_t density, float heatingToAdd);
extern  int CurveGetMaxHeating       (float ambient, float *pMaxHeating, float *pMinHeating);
extern  int CurveLoad();
extern  int CurveSave();
extern void CurveAddPointsForHtmlChart(char **pp);
