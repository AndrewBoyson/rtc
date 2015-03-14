#include <pthread.h> //thread stuff
#include <semaphore.h> 

#include <stdint.h> 
#include <time.h>    //nanosleep, timespec

#include<stdlib.h>
#include<stdio.h>

#include "../global.h"
#include "../lib/cfg.h"
#include "../lib/log.h"
#include "../lib/thread.h"
#include "fan.h"
#include "pid.h"
#include "curve.h"

static char isAuto(void) {
	char pidAuto;
	PidGetAuto(&pidAuto);
	return pidAuto;
}
static char isStalled(void) {
	char stall;
	PidGetWarning(&stall);
	return stall;
}
static void correctFanCurve(void) {
	CfgLock();
		float divisor = CfgFanCurveCorrectionDivisor;
		int   count   = CfgFanCurveCorrectionCount;
	CfgUnlock();
	if (divisor < 0.0001) return;
	if (count   < 1     ) return;
	
	char loopLocked;
	PidGetLocked(&loopLocked);
	if (!loopLocked) return;

	static int   sumCount;
	static float sumActualHeating;
	static float sumActual;
	static int   sumFan;

	float actualHeating;
	float actual;
	uint16_t fan;

	PidGetActualMinusAmbient(&actualHeating);
	PidGetActual(&actual);
	FanGetOutput(&fan);
	
	sumCount++;
	sumActualHeating += actualHeating;
	sumActual        += actual;
	sumFan           += fan;
	
	if (sumCount < count) return;

	actualHeating =            sumActualHeating / count;
	actual        =            sumActual        / count;
	fan           = (uint16_t)(sumFan           / count);

	float curveHeating;
	CurveGetHeatingFromOutput(actual, fan, &curveHeating);

	float amountToAdd = (actualHeating - curveHeating) / divisor * count;

	CurveAddHeatingAtOutput(actual, fan, amountToAdd);
	
	CurveSave();

	sumCount         = 0;
	sumActualHeating = 0;
	sumActual        = 0;
	sumFan           = 0;
}
static void setFanFromPid(void) {
	float heating;
	float actual;
	uint16_t output;
	PidGetHeating(&heating);
	PidGetActual(&actual);
	CurveGetOutputFromHeating(actual, heating, &output);
	FanSetOutput(output);
}
static void setPidFromFan(void) {
	float heating;
	float actual;
	uint16_t output;
	FanGetOutput(&output);
	PidGetActual(&actual);
	CurveGetHeatingFromOutput(actual, output, &heating);
	PidSetHeating(heating);
}

//thread stuff
static struct Thread t;
static void *worker(void *arg) {
	if (ThreadWorkerInit(&t)) return NULL;
	CurveLoad();
	FanInit();
	PidInit();
	
	while(1)
	{
		PidScan();
		correctFanCurve();
		if (isAuto()) setFanFromPid();
		else          setPidFromFan();
		FanScan(isStalled());
	}
	
	Log('w', "Ending thread");
	return NULL;
}

//External routines
void FanSvrKill(void) {
	ThreadCancel(&t);
}
void FanSvrInit(char *name, int priority) {
	t.Name = name;
	t.Worker = worker;
	t.NormalPriority = priority;
	ThreadStart(&t);
}
void FanSvrJoin(void) {
	ThreadJoin(&t);
}
