// Pin modes
#define	INPUT			 0
#define	OUTPUT			 1
#define	PWM_OUTPUT		 2
#define	GPIO_CLOCK		 3

//Pin states
#define	LOW				 0
#define	HIGH			 1

// Pull up/down/none
#define	PUD_OFF			 0
#define	PUD_DOWN		 1
#define	PUD_UP			 2

// PWM
#define	PWM_MODE_MS		0
#define	PWM_MODE_BAL	1

extern int  GpioSetup           (void) ;
extern void GpioPinMode         (int pin, int mode) ;
extern void GpioPullUpDnControl (int pin, int pud) ;
extern int  GpioPinRead         (int pin) ;
extern void GpioPinWrite        (int pin, int value) ;
extern void GpioPwmWrite        (int pin, int value) ;
extern int  GpioPwmRead         (int pin) ;
extern void GpioPwmSetMode      (int mode) ;
extern void GpioPwmSetRange     (unsigned int range) ;
extern void GpioPwmSetClock     (int divisor) ;

