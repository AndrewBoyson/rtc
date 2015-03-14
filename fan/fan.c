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

#include "gpio.h"
#define PWM_PIN 18
#define RANGE 1024

//Data from outside
static uint16_t output;

//Data calculated
static uint16_t      density;
static  int32_t       onTime;
static  int32_t      offTime;
static uint16_t pulseDensity;
static  int32_t    pulseTime;

int FanSetOutput (uint16_t  value) {

	//Set this output
	if (value >= RANGE) output = RANGE - 1;
	else                output = value;
	
	return 0;
}
int FanGetOutput (uint16_t *value) {
	*value = output;
	return 0;
}

int FanGetDensity(uint16_t *value) {
	*value = density;
	return 0;
}
int FanGetOnTime ( int32_t *value) {
	*value = onTime;
	return 0;
}
int FanGetOffTime( int32_t *value) {
	*value = offTime;
	return 0;
}
int FanGetPulse  ( int32_t *value) {
	*value = pulseTime;
	return 0;
}

static void mssleep(int ms) {
	struct timespec request;
	request.tv_sec  = ms / 1000;
	request.tv_nsec = (ms - request.tv_sec * 1000) * ONE_MILLION;
	nanosleep (&request, NULL);
}
static void calculateWaitTimesAndPwmFromOutput(char stallWarning) { //Called every second
	CfgLock();
		int     outputPeriod = CfgFanOutputPeriodSecs * 1000;
		int      pulsePeriod = CfgFanPulsePeriodMs;
	    int      minRunSpeed = CfgFanMinRunSpeed;
	    int    minStartSpeed = CfgFanMinStartSpeed;
	CfgUnlock();

	//Used to carry over any very low (< one pulse or 800 * 0.5 / 20 = 20) outputs for next time
	static int carryOutput;

	//The fan can just be set as it is already above minimum start speed
	if (output > minStartSpeed)
	{
		density = output;
		offTime = 0;
		pulseDensity = 0;
		pulseTime = 0;
		onTime = outputPeriod;
		carryOutput = 0;
		return;
	}

	//If there was off time last scan or the fan might be stalled add a pulse
	if (offTime || stallWarning)
	{
		pulseTime = pulsePeriod;
		pulseDensity = minStartSpeed;
	}
	else
	{
		pulseTime = 0;
		pulseDensity = 0;
	}
		
	//See what remains after the pulse if there is one
	int areaRemaining = (output + carryOutput) * outputPeriod - pulseDensity * pulseTime; //20 * 1000 * 1024 ~~> 2^25 so an int (+/-31bit) is fine
	int timeRemaining = outputPeriod - pulseTime;

	//If even just the pulse would be too much (0.5*800/20 = 20) just send zero and carry the output over to next time
	if (areaRemaining < 0)
	{
		density = 0;
		offTime = outputPeriod;
		pulseDensity = 0;
		pulseTime = 0;
		onTime = 0;
		carryOutput += output;
		return;
	}
	
	//See if can run without gaps
	int densityRemaining = areaRemaining / timeRemaining;
	if (densityRemaining > minRunSpeed)
	{
		offTime = 0;
		onTime = timeRemaining;
		density = densityRemaining;
		carryOutput = 0;
		return;
	}
	
	//Don't go less than the mimimum density: insert gaps instead
	density = minRunSpeed;
	onTime = areaRemaining / minRunSpeed;
	offTime = timeRemaining - onTime;
	carryOutput = 0;
}
static void setPwmAndDoWaits(void) {
	if (pulseTime)
	{
		GpioPwmWrite (PWM_PIN, pulseDensity);
		mssleep(pulseTime);
	}
	if (onTime)
	{
		GpioPwmWrite (PWM_PIN, (int)density);
		mssleep(onTime);
	}
	if (offTime)
	{
		GpioPwmWrite (PWM_PIN, 0);
		mssleep(offTime);
	}		
}
void FanScan(char stallWarning) {
	calculateWaitTimesAndPwmFromOutput(stallWarning);
	setPwmAndDoWaits();
}
void FanInit(void) {
	GpioSetup() ;
	GpioPinMode (PWM_PIN, PWM_OUTPUT);
	FanSetOutput(0);
}
