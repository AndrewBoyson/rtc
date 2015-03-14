#include <stdio.h>  //File stuff
#include <stdint.h> 
#include <string.h> //strdupa

#include "../global.h"
#include "../lib/cfg.h"
#include "../lib/datetime.h"
#include "../lib/log.h"
#include "../lib/rtc.h"
#include "../lib/state.h"
#include "curve.h"

static float target;

static float actual;
static float ambient;
static float maxHeating;
static float minHeating;

static float    lastInput = 0.0;
static uint64_t lastScan  = 0;

static float stepAmount;
static char  stepDirection = 0;
static char  stepAuto = 0;

static char isAuto = 1;

static float proportional = 0.0;
static float integral     = 0.0;
static float derivative   = 0.0;

static char stalled = 0;
static char locked  = 0;

static int record(void) {

	//Get the filename and check if wanted
	char *filename = NULL;
	CfgLock();
		if (CfgFanLogFileName != NULL) filename = strdupa(CfgFanLogFileName);
	CfgUnlock();
	if (filename == NULL) return 0;
	if (filename[0] == 0) return 0;

	//Get the time
	char time[30];
	if (DateTimeNowS(30, time)) return -1;
	
	//Open the log
	FILE *fp = fopen(filename, "a");
	if (fp == NULL)
	{
		LogErrno("ERROR opening fan log file: ");
		return -1;
	}
	
	//Append the date
	if (fprintf(fp, "%s", time) < 0) { LogErrno("Error appending to fan log file"); fclose(fp); return -1; }
	
	//Append the pid values
	if (fprintf(fp, "\t%2.3f\t%2.3f\t%2.3f\t%2.3f\t%2.3f\t%2.3f", target, actual, ambient, proportional, integral, derivative) < 0)
	{
		LogErrno("Error appending to fan log file");
		fclose(fp);
		return -1;
	}
	
	//Append a LF
	if (fprintf(fp, "\n") < 0) { LogErrno("Error appending to fan log file"); fclose(fp); return -1; }

	//Close the file
	if (fclose(fp))
	{
		fclose(fp);
		LogErrno("Error closing fan log file: ");
		return -1;
	}

	return 0;
}
static int getTemperatures(void) {
	if (RtcGetTemperature (&actual )) return -1;
	if (RtcGetAmbient     (&ambient)) return -1;
	if (CurveGetMaxHeating( ambient, &maxHeating, &minHeating)) return -1;
	return 0;
}
int PidGetAuto(char *value) {
	*value = isAuto;
	return 0;
}
int PidSetAuto(char value) {
	isAuto = value;
	return 0;
}
int PidGetTarget(float *value) {
	*value = target;
	return 0;
}
int PidSetTarget(float value) {
	target = value;
	StateWrite("pidtarget", "%.3f", value);
	return 0;
}
int PidGetInput(float *value) {
	*value = lastInput;
	return 0;
}
int PidGetWarning(char *value) {
	*value = stalled;
	return 0;
}
int PidGetLocked(char *value) {
	*value = locked;
	return 0;
}
int PidSetOutput(float value) {
	integral = value - proportional - derivative;
	return 0;
}
int PidGetOutput(float *pValue) {
	*pValue = proportional + integral + derivative;
	return 0;
}
int PidSetHeating(float heating) {
	float pid = heating + ambient - target; //Do inverse of what is done in PidGetHeating
	integral = pid - proportional - derivative;
	return 0;
}
int PidGetHeating(float *pHeating) {
	float pid = proportional + integral + derivative;
	float heating = pid - ambient + target; //higher ambient ==> less heating; higher target ==> more heating 
	*pHeating = heating;
	return 0;
}
int PidGetAmbient(float *value) {
	*value = ambient;
	return 0;
}
int PidGetActual(float *value) {
	*value = actual;
	return 0;
}
int PidGetActualMinusAmbient(float *value) {
	*value = actual - ambient;
	return 0;
}

int PidGetStepAmount(float *value) {
	*value = stepAmount;
	return 0;
}
int PidSetStepAmount(float value) {
	StateWrite("pidstepamount", "%.3f", value);
	stepAmount = value;
	return 0;
}
int PidGetStepDirection(char *value) {
	*value = stepDirection;
	return 0;
}
int PidSetStepDirection(char value) {
	StateWrite("pidstepdirection", "%d", value);
	stepDirection = value;
	return 0;
}
int PidGetStepAuto(char *value) {
	*value = stepAuto;
	return 0;
}
int PidSetStepAuto(char value) {
	StateWrite("pidstepauto", "%d", value);
	stepAuto = value;
	return 0;
}
int PidDoStep(void) {
	if (stepAuto)
	{
		if (stepDirection)
		{
			if (target + stepAmount > ambient + maxHeating) PidSetStepDirection(0);
			else                                            PidSetTarget(target + stepAmount);
		}
		else
		{
			if (target - stepAmount < ambient + minHeating) PidSetStepDirection(1);
			else                                            PidSetTarget(target - stepAmount);
		}
	}
	return 0;
}

int PidScan(void) {
	//Get this scan sample time
	uint64_t thisScan;
	if (DateTimeNowTicks(1970, ONE_MILLION, &thisScan)) return -1;
	
	//Read the configuration
	CfgLock();
		float Kp = CfgFanProportionalGain;
		float Ti = CfgFanIntegralTime;
		float Td = CfgFanDerivativeTime;
	CfgUnlock();

	//Get the error
	if (getTemperatures()) return -1;
	float thisInput = target - actual;
	
	//Check for locked or stalled
	stalled = thisInput < -0.5;
	 locked = thisInput > -0.5 && thisInput < 0.5;

	//Calculate the proportional
	proportional = Kp * thisInput;
	
	//Get the derivative and integral if have a scan duration
	if (lastScan)
	{
		float dt = (float)(thisScan - lastScan) / ONE_MILLION;
		float rateOfChangeOfError = (thisInput - lastInput) / dt;
		float     integratedError = dt * thisInput;
		derivative   = Kp * Td * rateOfChangeOfError;
		integral    += Kp / Ti * integratedError;
	}
	else
	{
		derivative = 0;
		integral = 0;
	}

	//Clamp the integral to prevent wind-up
	float heating;
	PidGetHeating(&heating);
	if (heating < minHeating) PidSetHeating(minHeating);
	if (heating > maxHeating) PidSetHeating(maxHeating);

	//Prepare for the next scan
	lastInput = thisInput;
	lastScan  = thisScan;
	
	//Record the result
	record();
	
	return 0;
}
int PidInit(void) {
	StateRead("pidtarget",        "%f", &target);
	StateRead("pidstepamount",    "%f", &stepAmount);
	StateRead("pidstepdirection", "%d", &stepDirection);
	StateRead("pidstepauto",      "%d", &stepAuto);
	lastInput = 0.0;
	lastScan  = 0;
	if (getTemperatures()) return -1;
	return 0;
}