extern int PidGetTarget            (float *);
extern int PidGetInput             (float *);
extern int PidGetOutput            (float *);
extern int PidGetHeating           (float *);
extern int PidGetAuto              (char  *);
extern int PidGetWarning           (char  *);
extern int PidGetLocked            (char  *);
extern int PidGetAmbient           (float *);
extern int PidGetActual            (float *);
extern int PidGetActualMinusAmbient(float *);

extern int PidSetTarget       (float);
extern int PidSetOutput       (float);
extern int PidSetHeating      (float);
extern int PidSetAuto         (char );

extern int PidGetStepAmount   (float *);
extern int PidGetStepDirection(char  *);
extern int PidGetStepAuto     (char  *);

extern int PidSetStepAmount   (float);
extern int PidSetStepDirection(char );
extern int PidSetStepAuto     (char );

extern int PidDoStep          (void);
extern int PidScan            (void);
extern int PidInit            (void);


