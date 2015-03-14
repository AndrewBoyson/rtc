#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <ctype.h>
#include <poll.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/ioctl.h>

#include "gpio.h"
#include "../lib/log.h"


// BCM Magic
#define	BCM_PASSWORD		0x5A000000


// The BCM2835 has 54 GPIO pins.
//	BCM2835 data sheet, Page 90 onwards.
//	There are 6 control registers, each control the functions of a block
//	of 10 pins.
//	Each control register has 10 sets of 3 bits per GPIO pin - the ALT values
//
//	000 = GPIO Pin X is an input
//	001 = GPIO Pin X is an output
//	100 = GPIO Pin X takes alternate function 0
//	101 = GPIO Pin X takes alternate function 1
//	110 = GPIO Pin X takes alternate function 2
//	111 = GPIO Pin X takes alternate function 3
//	011 = GPIO Pin X takes alternate function 4
//	010 = GPIO Pin X takes alternate function 5
//
// So the 3 bits for port X are:
//	X / 10 + ((X % 10) * 3)

// Port function select bits
#define	FSEL_INPT		0b000
#define	FSEL_OUTP		0b001
#define	FSEL_ALT0		0b100
#define	FSEL_ALT1		0b101
#define	FSEL_ALT2		0b110
#define	FSEL_ALT3		0b111
#define	FSEL_ALT4		0b011
#define	FSEL_ALT5		0b010

// Access from ARM Running Linux
//	Taken from Gert/Doms code. Some of this is not in the manual
//	that I can find )-:

#define BCM2708_PERI_BASE	                 0x20000000
#define GPIO_PADS		(BCM2708_PERI_BASE + 0x00100000)
#define CLOCK_BASE		(BCM2708_PERI_BASE + 0x00101000)
#define GPIO_BASE		(BCM2708_PERI_BASE + 0x00200000)
#define GPIO_TIMER		(BCM2708_PERI_BASE + 0x0000B000)
#define GPIO_PWM		(BCM2708_PERI_BASE + 0x0020C000)

#define	BLOCK_SIZE		(4*1024)

#define	GPPUD	37

// PWM
//	Word offsets into the PWM control region

#define	PWM_CONTROL 0
#define	PWM_STATUS  1
#define	PWM0_RANGE  4
#define	PWM0_DATA   5
#define	PWM1_RANGE  8
#define	PWM1_DATA   9

//	Clock register offsets

#define	PWMCLK_CNTL	40
#define	PWMCLK_DIV	41

#define	PWM0_MS_MODE    0x0080  // Run in MS mode
#define	PWM0_USEFIFO    0x0020  // Data from FIFO
#define	PWM0_REVPOLAR   0x0010  // Reverse polarity
#define	PWM0_OFFSTATE   0x0008  // Ouput Off state
#define	PWM0_REPEATFF   0x0004  // Repeat last value if FIFO empty
#define	PWM0_SERIAL     0x0002  // Run in serial mode
#define	PWM0_ENABLE     0x0001  // Channel Enable

#define	PWM1_MS_MODE    0x8000  // Run in MS mode
#define	PWM1_USEFIFO    0x2000  // Data from FIFO
#define	PWM1_REVPOLAR   0x1000  // Reverse polarity
#define	PWM1_OFFSTATE   0x0800  // Ouput Off state
#define	PWM1_REPEATFF   0x0400  // Repeat last value if FIFO empty
#define	PWM1_SERIAL     0x0200  // Run in serial mode
#define	PWM1_ENABLE     0x0100  // Channel Enable

// Locals to hold pointers to the hardware
static volatile uint32_t *gpio ;
static volatile uint32_t *pwm ;
static volatile uint32_t *clk ;
static volatile uint32_t *pads ;

static uint8_t gpioToGPFSEL [] = {// Map a BCM_GPIO pin to it's Function Selection control port. (GPFSEL 0-5).
//	Groups of 10 - 3 bits per Function - 30 bits per port
  0,0,0,0,0,0,0,0,0,0,
  1,1,1,1,1,1,1,1,1,1,
  2,2,2,2,2,2,2,2,2,2,
  3,3,3,3,3,3,3,3,3,3,
  4,4,4,4,4,4,4,4,4,4,
  5,5,5,5,5,5,5,5,5,5,
} ;
static uint8_t gpioToShift [] = {//  Define the shift up for the 3 bits per pin in each GPFSEL port
  0,3,6,9,12,15,18,21,24,27,
  0,3,6,9,12,15,18,21,24,27,
  0,3,6,9,12,15,18,21,24,27,
  0,3,6,9,12,15,18,21,24,27,
  0,3,6,9,12,15,18,21,24,27,
} ;
static uint8_t gpioToGPSET [] = {//	(Word) offset to the GPIO Set registers for each GPIO pin
   7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
   8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
} ;
static uint8_t gpioToGPCLR [] = {//	(Word) offset to the GPIO Clear registers for each GPIO pin
  10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,
  11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,
} ;
static uint8_t gpioToGPLEV [] = {//	(Word) offset to the GPIO Input level registers for each GPIO pin
  13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,
  14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,
} ;

static uint8_t gpioToPUDCLK [] = {// (Word) offset to the Pull Up Down Clock register
  38,38,38,38,38,38,38,38,38,38,38,38,38,38,38,38,38,38,38,38,38,38,38,38,38,38,38,38,38,38,38,38,
  39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,
} ;
static uint8_t gpioToPwmALT [] = {// The ALT value to put a GPIO pin into PWM mode
          0,         0,         0,         0,         0,         0,         0,         0,	//  0 ->  7
          0,         0,         0,         0, FSEL_ALT0, FSEL_ALT0,         0,         0, 	//  8 -> 15
          0,         0, FSEL_ALT5, FSEL_ALT5,         0,         0,         0,         0, 	// 16 -> 23
          0,         0,         0,         0,         0,         0,         0,         0,	// 24 -> 31
          0,         0,         0,         0,         0,         0,         0,         0,	// 32 -> 39
  FSEL_ALT0, FSEL_ALT0,         0,         0,         0, FSEL_ALT0,         0,         0,	// 40 -> 47
          0,         0,         0,         0,         0,         0,         0,         0,	// 48 -> 55
          0,         0,         0,         0,         0,         0,         0,         0,	// 56 -> 63
} ;
static uint8_t gpioToPwmPort [] = {// The port value to put a GPIO pin into PWM mode
          0,         0,         0,         0,         0,         0,         0,         0,	//  0 ->  7
          0,         0,         0,         0, PWM0_DATA, PWM1_DATA,         0,         0, 	//  8 -> 15
          0,         0, PWM0_DATA, PWM1_DATA,         0,         0,         0,         0, 	// 16 -> 23
          0,         0,         0,         0,         0,         0,         0,         0,	// 24 -> 31
          0,         0,         0,         0,         0,         0,         0,         0,	// 32 -> 39
  PWM0_DATA, PWM1_DATA,         0,         0,         0, PWM1_DATA,         0,         0,	// 40 -> 47
          0,         0,         0,         0,         0,         0,         0,         0,	// 48 -> 55
          0,         0,         0,         0,         0,         0,         0,         0,	// 56 -> 63

} ;

static void delayMicrosecondsHard (unsigned int howLong) {
  struct timeval tNow, tLong, tEnd ;

  gettimeofday (&tNow, NULL) ;
  tLong.tv_sec  = howLong / 1000000 ;
  tLong.tv_usec = howLong % 1000000 ;
  timeradd (&tNow, &tLong, &tEnd) ;

  while (timercmp (&tNow, &tEnd, <))
    gettimeofday (&tNow, NULL) ;
}
static void delayMicroseconds (unsigned int howLong) {
/*
 * delayMicroseconds:
 *	This is somewhat interesting. It seems that on the Pi, a single call
 *	to nanosleep takes some 80 to 130 microseconds anyway, so while
 *	obeying the standards (may take longer), it's not always what we
 *	want!
 *
 *	So what I'll do now is if the delay is less than 100uS we'll do it
 *	in a hard loop, watching a built-in counter on the ARM chip. This is
 *	somewhat sub-optimal in that it uses 100% CPU, something not an issue
 *	in a microcontroller, but under a multi-tasking, multi-user OS, it's
 *	wastefull, however we've no real choice )-:
 *
 *      Plan B: It seems all might not be well with that plan, so changing it
 *      to use gettimeofday () and poll on that instead...
 *********************************************************************************
 */
  struct timespec sleeper ;
  unsigned int uSecs = howLong % 1000000 ;
  unsigned int wSecs = howLong / 1000000 ;

  /**/ if (howLong ==   0)
    return ;
  else if (howLong  < 100)
    delayMicrosecondsHard (howLong) ;
  else
  {
    sleeper.tv_sec  = wSecs ;
    sleeper.tv_nsec = (long)(uSecs * 1000L) ;
    nanosleep (&sleeper, NULL) ;
  }
}

int  GpioSetup (void) {
  int      fd ;

  if (geteuid () != 0)
  {
	Log('e', "wiringPiSetup: Must be root. (Did you forget sudo?)") ;
	return -1;
	}

// Open the master /dev/memory device

  if ((fd = open ("/dev/mem", O_RDWR | O_SYNC | O_CLOEXEC) ) < 0)
  {
    	Log('e', "wiringPiSetup: Unable to open /dev/mem: %s\n", strerror (errno)) ;
		return -1;
	}

// GPIO:

  gpio = (uint32_t *)mmap(0, BLOCK_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, GPIO_BASE) ;
  if ((int32_t)gpio == -1)
      {
    	Log('e', "wiringPiSetup: mmap (GPIO) failed: %s\n", strerror (errno)) ;
		return -1;
	}

// PWM

  pwm = (uint32_t *)mmap(0, BLOCK_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, GPIO_PWM) ;
  if ((int32_t)pwm == -1)
     {
    	Log('e', "wiringPiSetup: mmap (PWM) failed: %s\n", strerror (errno)) ;
		return -1;
	}
 
// Clock control (needed for PWM)

  clk = (uint32_t *)mmap(0, BLOCK_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, CLOCK_BASE) ;
  if ((int32_t)clk == -1)
      {
    	Log('e', "wiringPiSetup: mmap (CLOCK) failed: %s\n", strerror (errno)) ;
		return -1;
	}
 
// The drive pads

  pads = (uint32_t *)mmap(0, BLOCK_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, GPIO_PADS) ;
  if ((int32_t)pads == -1)
      {
    	Log('e', "wiringPiSetup: mmap (PADS) failed: %s\n", strerror (errno)) ;
		return -1;
		}

  return 0 ;
}
void GpioPinMode (int pin, int mode) { //Sets the mode of a pin to be input, output or PWM output
	int    fSel, shift, alt ;
	fSel    = gpioToGPFSEL [pin] ;
	shift   = gpioToShift  [pin] ;

	if      (mode == INPUT)  *(gpio + fSel) = (*(gpio + fSel) & ~(7 << shift)) ; // Sets bits to zero = input
	else if (mode == OUTPUT) *(gpio + fSel) = (*(gpio + fSel) & ~(7 << shift)) | (1 << shift) ;
	else if (mode == PWM_OUTPUT)
	{
		if ((alt = gpioToPwmALT [pin]) == 0)	return ;	// Not a PWM pin
		*(gpio + fSel) = (*(gpio + fSel) & ~(7 << shift)) | (alt << shift) ; // Set pin to PWM mode
		delayMicroseconds (110) ;		// See comments in pwmSetClockWPi

		GpioPwmSetMode  (PWM_MODE_BAL) ;	// Pi default mode
		GpioPwmSetRange (1024) ;		    // Default range of 1024
		GpioPwmSetClock (32) ;				// 19.2 / 32 = 600KHz - Also starts the PWM
	}
}
void GpioPullUpDnControl (int pin, int pud) { //Sets the input pull up /down resistors (about 50k)
    *(gpio + GPPUD)              = pud & 3;         delayMicroseconds (5);
    *(gpio + gpioToPUDCLK [pin]) = 1 << (pin & 31); delayMicroseconds (5);
    
    *(gpio + GPPUD)              = 0; delayMicroseconds (5);
    *(gpio + gpioToPUDCLK [pin]) = 0; delayMicroseconds (5);
}
int  GpioPinRead (int pin) {
	if ((*(gpio + gpioToGPLEV [pin]) & (1 << (pin & 31))) != 0) return HIGH ;
	else                                                        return LOW ;
}
void GpioPinWrite (int pin, int value) {
    if (value == LOW) *(gpio + gpioToGPCLR [pin]) = 1 << (pin & 31) ;
    else              *(gpio + gpioToGPSET [pin]) = 1 << (pin & 31) ;
}
void GpioPwmSetMode (int mode) { //Select the native "balanced" mode, or standard mark:space mode
    if (mode == PWM_MODE_MS)
      *(pwm + PWM_CONTROL) = PWM0_ENABLE | PWM1_ENABLE | PWM0_MS_MODE | PWM1_MS_MODE ;
    else
      *(pwm + PWM_CONTROL) = PWM0_ENABLE | PWM1_ENABLE ;
}
void GpioPwmSetRange (unsigned int range) { //Set the PWM range register. 
    *(pwm + PWM0_RANGE) = range ; delayMicroseconds (10) ;
    *(pwm + PWM1_RANGE) = range ; delayMicroseconds (10) ;
}
void GpioPwmSetClock (int divisor) { //Set/Change the PWM clock. 

    uint32_t pwm_control = *(pwm + PWM_CONTROL) ;						// preserve PWM_CONTROL

    *(pwm + PWM_CONTROL) = 0 ;											// Stop PWM prior to stopping PWM clock in MS mode otherwise BUSY stays high.

    *(clk + PWMCLK_CNTL) = BCM_PASSWORD | 0x01 ;						// Stop PWM Clock before changing the divisor
    delayMicroseconds (110) ;											// prevents clock going sloooow (95uS occasionally fails, 100uS OK)

    while ((*(clk + PWMCLK_CNTL) & 0x80) != 0) delayMicroseconds (1) ;	// Wait for clock to be !BUSY
     
	divisor &= 4095 ;
    *(clk + PWMCLK_DIV)  = BCM_PASSWORD | (divisor << 12) ; 			//Change the divisor

    *(clk + PWMCLK_CNTL) = BCM_PASSWORD | 0x11 ;						// Start PWM clock
    *(pwm + PWM_CONTROL) = pwm_control ;								// restore PWM_CONTROL

}
void GpioPwmWrite (int pin, int value) {
    *(pwm + gpioToPwmPort [pin]) = value ;
}
int  GpioPwmRead(int pin) {
	return *(pwm + gpioToPwmPort[pin]);
}


