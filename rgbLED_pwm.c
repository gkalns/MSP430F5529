/*
Author: gkalns

Driving common anode RGB LED.
Anode is connected to P1.5
Cathodes are connected to P1.2 - P1.4
Uses PWM output, changing from MIN_VAL to MAX_VAL
PWM frequencies are f, f/2 and f/4.

*/

#include <msp430.h>

#define MIN_VAL	50
#define MAX_VAL 100

int up_flag;
int counter;

int main(void)
{
  WDTCTL = WDT_MDLY_32;
  SFRIE1 |= WDTIE;                          // Enable WDT interrupt

  P1DIR |= BIT2+BIT3+BIT4+BIT5;
  P1SEL |= BIT2+BIT3+BIT4;
  P1OUT |= BIT5;

  TA0CCR0 = 100;                          // PWM Period
  TA0CCTL1 = OUTMOD_7;                      // CCR1 reset/set
  TA0CCR1 = MIN_VAL;                            // CCR1 PWM duty cycle
  TA0CCTL2 = OUTMOD_7;                      // CCR2 reset/set
  TA0CCR2 = MIN_VAL;                            // CCR2 PWM duty cycle
  TA0CCTL3 = OUTMOD_7;                      // CCR3 reset/set
  TA0CCR3 = MIN_VAL;                            // CCR3 PWM duty cycle
  TA0CTL = TASSEL1+MC0;

  up_flag |= BIT2+BIT3+BIT4;
  counter = 0;

  __bis_SR_register(LPM0_bits + GIE);       // Enter LPM0, enable interrupts
  __no_operation();                         // For debugger
}

// Watchdog Timer interrupt service routine
#pragma vector=WDT_VECTOR
__interrupt void WDT_ISR(void)
{
	counter++;
	if( up_flag & BIT2 ){
		if( TA0CCR1 >= MAX_VAL ){ up_flag ^= BIT2; } else { TA0CCR1++; }
	} else {
		if( TA0CCR1 <= MIN_VAL ){ up_flag ^= BIT2; } else { TA0CCR1--; }
	}

	if( counter & BIT0 ){
		if( up_flag & BIT3 ){
			if( TA0CCR2 >= MAX_VAL ){ up_flag ^= BIT3; } else { TA0CCR2++; }
		} else {
			if( TA0CCR2 <= MIN_VAL ){ up_flag ^= BIT3; } else { TA0CCR2--; }
		}
	}

	if( (counter & BIT0) && (counter & BIT1) ){
		if( up_flag & BIT4 ){
			if( TA0CCR3 >= MAX_VAL ){ up_flag ^= BIT4; } else { TA0CCR3++; }
		} else {
			if( TA0CCR3 <= MIN_VAL ){ up_flag ^= BIT4; } else { TA0CCR3--; }
		}
	}
}


