#include <msp430.h>

// TLC5940 pins
#define BLANK_PIN	BIT1
#define BLANK_REG	P6OUT
#define XLAT_PIN	BIT2
#define XLAT_REG	P6OUT
// GSCLOCK - P2.2 smclck
// SIN - P3.0 SPI
// SCLK - 3.2 SPI
// others not used


// Anodes
#define ANODE1REG	P8OUT
#define ANODE2REG	P3OUT
#define ANODE3REG	P4OUT
#define ANODE4REG	P4OUT
#define ANODE5REG	P1OUT
#define ANODE6REG	P1OUT
#define ANODE7REG	P1OUT
#define ANODE8REG	P1OUT

#define ANODE1PIN	BIT2;
#define ANODE2PIN	BIT7;
#define ANODE3PIN	BIT0;
#define ANODE4PIN	BIT3;
#define ANODE5PIN	BIT2;
#define ANODE6PIN	BIT3;
#define ANODE7PIN	BIT4;
#define ANODE8PIN	BIT5;

// Useful macros
#define setHigh(reg, pin)	( reg |= pin )
#define setLow(reg, pin)	( reg &= ~pin )
#define pulse(reg, pin)	{ setHigh(reg, pin); setLow(reg, pin); }


// display data - 8 lines, 192 bits each line, sorted in 24 Bytes
unsigned char display_data[8][24] = {
		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //12 red bytes
		128, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },//12 blue bytes
		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //12 red bytes
		0, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },//12 blue bytes
		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //12 red bytes
		0, 0, 0, 128, 0, 0, 0, 0, 0, 0, 0, 0 },//12 blue bytes
		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //12 red bytes
		0, 0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0 },//12 blue bytes
		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //12 red bytes
		0, 0, 0, 0, 0, 0, 128, 0, 0, 0, 0, 0 },//12 blue bytes
		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //12 red bytes
		0, 0, 0, 0, 0, 0, 0, 8, 0, 0, 0, 0 },//12 blue bytes
		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //12 red bytes
		0, 0, 0, 0, 0, 0, 0, 0, 0, 128, 0, 0 },//12 blue bytes
		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //12 red bytes
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 8, 0 }//12 blue bytes
};








unsigned char row_counter = 0;
unsigned char spi_sent_data_counter = 0;
unsigned char xlatch_needed = 0;
unsigned char row_to_load;


int main(void) {
    WDTCTL = WDTPW | WDTHOLD;	// Stop watchdog timer

    // Set 4 MHz clock -----------------------------------------------------
    UCSCTL3 |= SELREF_2;                      // Set DCO FLL reference = REFO
	UCSCTL4 |= SELA_2;                        // Set ACLK = REFO

	__bis_SR_register(SCG0);                  // Disable the FLL control loop
	UCSCTL0 = 0x0000;                         // Set lowest possible DCOx, MODx
	UCSCTL1 = DCORSEL_3;
	UCSCTL2 = 121;                   		// Set DCO Multiplier (N + 1) * FLLRef = Fdco
	__bic_SR_register(SCG0);                  // Enable the FLL control loop

	// 32 x 32 x 4 MHz / 32,768 Hz = 125000 = MCLK cycles for DCO to settle
	__delay_cycles(125000);

	// Loop until XT1,XT2 & DCO fault flag is cleared
	do{
		UCSCTL7 &= ~(XT2OFFG + XT1LFOFFG + DCOFFG); // Clear XT2,XT1,DCO fault flags
		SFRIFG1 &= ~OFIFG;                      // Clear fault flags
	}while (SFRIFG1&OFIFG);                   // Test oscillator fault flag
    // End of - Set 4 MHz clock-------------------------------------------------------

    // Set up UART ------------------------------------------------------------------
	P3SEL = BIT3+BIT4;                        // P3.4,5 = USCI_A0 TXD/RXD
	UCA0CTL1 |= UCSWRST;                      // **Put state machine in reset**
	UCA0CTL1 |= UCSSEL_2;                     // SMCLK
	UCA0BRW = 416;                              // 4MHz 9600 (see User's Guide)
	UCA0MCTL = UCBRS_6;   						// Modln UCBRSx=0, UCBRFx=0,

	UCA0CTL1 &= ~UCSWRST;                     // **Initialize USCI state machine**
	UCA0IE |= UCRXIE;                         // Enable USCI_A0 RX interrupt
    // End of - Set up UART --------------------------------------------------------

    P6DIR |= BLANK_PIN | XLAT_PIN;
    P2DIR |= BIT2; // SMCLK output for GSCLCK
	P2SEL |= BIT2; // SMCLK set out to pins

	// Init TLC output pins
	setLow(XLAT_REG, XLAT_PIN);
	setHigh(BLANK_REG, BLANK_PIN);

    // Set anodes as outputs
	P8DIR |= BIT2;
	P3DIR |= BIT7;
	P4DIR |= BIT0 | BIT3;
	P1DIR |= BIT2 | BIT3 | BIT4 | BIT5;

	// turn off all anodes
	ANODE1REG &= ~ANODE1PIN;
	ANODE2REG &= ~ANODE2PIN;
	ANODE3REG &= ~ANODE3PIN;
	ANODE4REG &= ~ANODE4PIN;
	ANODE5REG &= ~ANODE5PIN;
	ANODE6REG &= ~ANODE6PIN;
	ANODE7REG &= ~ANODE7PIN;
	ANODE8REG &= ~ANODE8PIN;

	// Set up SPI on USCI_B0 ------------------------------------------------
	P3SEL |= BIT0;                       // master data out
	P3SEL |= BIT2;                       // clock

	UCB0CTL1 |= UCSWRST;                      // **Put state machine in reset**
	UCB0CTL0 |= UCMST+UCSYNC+UCMSB+UCCKPH;    		// 3-pin, 8-bit SPI master
											// MSB
	UCB0CTL1 |= UCSSEL_2;                     // SMCLK
	UCB0BR0 = 0x02;                           // /2
	UCB0BR1 = 0;                              //
	UCB0CTL1 &= ~UCSWRST;                     // **Initialize USCI state machine**
	// End of - Set up SPI on USCI_B0 -------------------------------------------

	// update data without interrupts (1st time, 1st line) ----------------------
	unsigned char data_counter = 0;
	for(data_counter = 0; data_counter < 24; data_counter++ ){

		UCB0TXBUF = display_data[0][data_counter];
		while (!(UCB0IFG&UCTXIFG));               // USCI_A0 TX buffer ready?
	}
	pulse(XLAT_REG, XLAT_PIN);
	// End of - update data without interrupts (1st time, 1st line) --------------

	// start the first anode
	ANODE1REG |= ANODE1PIN;

	// Set up timerA for grayscale blanking
	TA0CCTL0 = CCIE;                          // CCR0 interrupt enabled
	TA0CCR0 = 4096;
	TA0CTL = TASSEL_2 + MC_1 + TACLR;         // SMCLK, upmode, clear TAR
	setLow(BLANK_REG, BLANK_PIN);						// Start grayscale cycle

	__bis_SR_register(LPM0_bits + GIE);       // Enter LPM0, enable interrupts
	__no_operation();
}


// Timer0 A0 interrupt service routine
#pragma vector=TIMER0_A0_VECTOR
__interrupt void TIMER0_A0_ISR(void)
{
	setHigh(BLANK_REG, BLANK_PIN);

	if(xlatch_needed){ pulse(XLAT_REG, XLAT_PIN); xlatch_needed = 0; }

	row_counter++;
	row_counter &= 7; // leave just first 3 bits

	// turn on necessary anode
	switch(row_counter){
		case 0: ANODE8REG &= ~ANODE8PIN; ANODE1REG |= ANODE1PIN; break;
		case 1: ANODE1REG &= ~ANODE1PIN; ANODE2REG |= ANODE2PIN; break;
		case 2: ANODE2REG &= ~ANODE2PIN; ANODE3REG |= ANODE3PIN; break;
		case 3: ANODE3REG &= ~ANODE3PIN; ANODE4REG |= ANODE4PIN; break;
		case 4: ANODE4REG &= ~ANODE4PIN; ANODE5REG |= ANODE5PIN; break;
		case 5: ANODE5REG &= ~ANODE5PIN; ANODE6REG |= ANODE6PIN; break;
		case 6: ANODE6REG &= ~ANODE6PIN; ANODE7REG |= ANODE7PIN; break;
		case 7: ANODE7REG &= ~ANODE7PIN; ANODE8REG |= ANODE8PIN; break;
	}

//	TA0CTL = TASSEL_2 + MC_1 + TACLR;
	setLow(BLANK_REG, BLANK_PIN); // start grayscale

	row_to_load = row_counter +1;
	row_to_load &= 7;

	// update data. Start SPI transmission.
	spi_sent_data_counter = 0;
	UCB0IE |= UCTXIE;
	while (!(UCB0IFG&UCTXIFG)); // check if buffer clean
	UCB0TXBUF = display_data[row_to_load][spi_sent_data_counter++];
	// interrupts do the rest
}


#pragma vector=USCI_B0_VECTOR
__interrupt void USCI_B0_ISR(void)
{
  switch(__even_in_range(UCB0IV,4))
  {
  case 0:break;                             // Vector 0 - no interrupt
  case 2:break;								// Vector 2 - RXIFG
  case 4:									// Vector 4 - TXIFG
	  UCB0TXBUF = display_data[row_to_load][spi_sent_data_counter++];
	  if (spi_sent_data_counter == 24){ // TX over?
		  UCB0IE &= ~UCTXIE;
		  xlatch_needed = 1;
		  spi_sent_data_counter = 0;
	  }
	  break;
  default: break;
  }
}

// uart data receiving states
typedef enum States {state_READY, state_GET_LINE, state_GET_DATA} receiving_state;
receiving_state current_state = state_READY;
// TODO: CRC

// bluetooth uart stuff
unsigned char candidate_line[24] = { 0 };
unsigned char received_halfbyte_counter = 0;
unsigned char received_line_number = 0;
char received_byte;


#pragma vector=USCI_A0_VECTOR
__interrupt void USCI_A0_ISR(void)
{
  switch(__even_in_range(UCA0IV,4))
  {
  case 0:break;                             // Vector 0 - no interrupt
  case 2:                                   // Vector 2 - RXIFG
	  received_byte = UCA0RXBUF;
	  switch(current_state){
		case state_READY:
			if( received_byte == '$' ){				// signals the start
				received_halfbyte_counter = 0;
				current_state = state_GET_LINE;
			}
			break;
		case state_GET_LINE:
			switch(received_byte){
				case '0': received_line_number = 0; current_state = state_GET_DATA; break;
				case '1': received_line_number = 1; current_state = state_GET_DATA; break;
				case '2': received_line_number = 2; current_state = state_GET_DATA; break;
				case '3': received_line_number = 3; current_state = state_GET_DATA; break;
				case '4': received_line_number = 4; current_state = state_GET_DATA; break;
				case '5': received_line_number = 5; current_state = state_GET_DATA; break;
				case '6': received_line_number = 6; current_state = state_GET_DATA; break;
				case '7': received_line_number = 7; current_state = state_GET_DATA; break;
				case '$': current_state = state_GET_LINE; return;
				default: current_state = state_READY; return;
			}
			break;
		case state_GET_DATA:
			switch(received_byte){
				case '0': received_byte = 0; break;
				case '1': received_byte = 1; break;
				case '2': received_byte = 2; break;
				case '3': received_byte = 3; break;
				case '4': received_byte = 4; break;
				case '5': received_byte = 5; break;
				case '6': received_byte = 6; break;
				case '7': received_byte = 7; break;
				case '8': received_byte = 8; break;
				case '9': received_byte = 9; break;
				case 'a': received_byte = 10; break;
				case 'b': received_byte = 11; break;
				case 'c': received_byte = 12; break;
				case 'd': received_byte = 13; break;
				case 'e': received_byte = 14; break;
				case 'f': received_byte = 15; break;
				case '$': current_state = state_GET_LINE; return;
				default: current_state = state_READY; return; //break;
			}

			// if received_halfbyte_counter is even - it is msb
			if( received_halfbyte_counter & 1 ){
				candidate_line[received_halfbyte_counter/2+12] |= received_byte;
			} else{
				received_byte <<= 4;
				candidate_line[received_halfbyte_counter/2+12] = received_byte;
			}

			received_halfbyte_counter++;
			if(received_halfbyte_counter == 24){
			  received_halfbyte_counter = 0;
			  current_state = state_READY;

			  // register the line
			  unsigned char i;
			  for(i = 0; i < 24; i++){
				  display_data[received_line_number][i] = candidate_line[i];
			  }

			}
			break;
		default: break;
	  }
    break;
  case 4:break;                             // Vector 4 - TXIFG
  default: break;
  }
}












