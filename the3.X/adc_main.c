/*
 * File:   adc_main.c
 * Author: oguzhan
 *
 * Created on May 13, 2024, 3:58 PM
 */

#include "pragmas.h"
#include <stdio.h>

void main(void) {
  // Set ADC Inputs
  TRISH = 0x10; // AN12 input RH4
  // Configure ADC
  ADCON0 = 0x31; // Channel 12; Turn on AD Converter
  ADCON1 = 0x00; // All analog pins
  ADCON2 = 0xAA; // Right Align | 12 Tad | Fosc/32
  ADRESH = 0x00;
  ADRESL = 0x00;

  while(1){
    // Get ADC Sample
    GODONE = 1; // Start ADC conversion
    while(GODONE); // Poll and wait for conversion to finish.
    unsigned int result = (ADRESH << 8) + ADRESL; // Get the result;

    // 4bytes for ADC Res + 1 byte for custom char + 1 byte null;
    char buf[6];
    sprintf(buf, "%04u", result);
    buf[4]=0; // Address of custom char
    buf[5]=0; // Null terminator

  }
  return;
}
