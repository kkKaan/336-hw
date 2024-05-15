/*
 * File:   main.c
 * Author: merve
 *
 * Created on May 13, 2024, 2:04 PM
 */
#include "pragmas.h"
#include <xc.h>

char hello[6] = "hello";
unsigned char hello_ind = 0;
char hello_chr = 0;
char rcvd_chr = 0;

void transmitCharAndHello(char chr)
{
    hello_chr = chr;
    TXSTA1bits.TXEN = 1; //enable transmission.
}

void transmitData()
{
    if(hello_ind <= 4)
    {
        TXREG1 = hello[hello_ind++];
    }
    else if(hello_ind == 5)
    {
        TXREG1 = hello_chr;
        hello_ind++;
    }
    else
    {
        hello_ind = 0;
        // wait until the TSR1 register is empty
        //while (TXSTA1bits.TRMT == 0){}
        TXSTA1bits.TXEN = 0;// disable transmission
        
    }
}


void __interrupt (high_priority) highPriorityISR (void) {
		
	if (PIR1bits.RC1IF == 1) {
        rcvd_chr = RCREG1;
        PIR1bits.RC1IF = 0;	
        transmitCharAndHello(rcvd_chr); //hello+char_received
	}
    //  the TXREGx register is empty
    else if (PIR1bits.TX1IF == 1 ) {
        //PIR1bits.TX1IF = 0; // it cannot be cleared in software.
		transmitData();
	}
}

void init_int() {
    
    /* configure I/O ports */
    TRISCbits.RC7 = 1; // TX1 and RX1 pin configuration
    TRISCbits.RC6 = 0;
    
	/* configure USART transmitter/receiver */
	TXSTA1 = 0x04;      // (= 00000100) 8-bit transmit, transmitter NOT enabled,TXSTA1.TXEN not enabled!
                        // asynchronous, high speed mode
	RCSTA1 = 0x90;      // (= 10010000) 8-bit receiver, receiver enabled,
                        // continuous receive, serial port enabled RCSTA.CREN = 1
    BAUDCON1bits.BRG16 = 0;
    SPBRG1 = 255 ;		// for 40 MHz, to have 9600 baud rate, it should be 255
    
    /* configure the interrupts */
    INTCON = 0;			// clear interrupt register completely
	PIE1bits.TX1IE = 1;	// enable USART transmit interrupt
	PIE1bits.RC1IE = 1;	// enable USART receive interrupt
	PIR1 = 0;			// clear all peripheral flags
	
	INTCONbits.PEIE = 1;// enable peripheral interrupts
	INTCONbits.GIE = 1;	// globally enable interrupts
	
}

void main(void) {
    init_int();
    while(1) {

    }
    return;
    
}
