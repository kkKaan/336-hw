; Created by Kaan Karacanta

PROCESSOR 18F8722

#include <xc.inc>

; CONFIGURATION (DO NOT EDIT)
; CONFIG1H
CONFIG OSC = HSPLL      ; Oscillator Selection bits (HS oscillator, PLL enabled (Clock Frequency = 4 x FOSC1))
CONFIG FCMEN = OFF      ; Fail-Safe Clock Monitor Enable bit (Fail-Safe Clock Monitor disabled)
CONFIG IESO = OFF       ; Internal/External Oscillator Switchover bit (Oscillator Switchover mode disabled)
; CONFIG2L
CONFIG PWRT = OFF       ; Power-up Timer Enable bit (PWRT disabled)
CONFIG BOREN = OFF      ; Brown-out Reset Enable bits (Brown-out Reset disabled in hardware and software)
; CONFIG2H
CONFIG WDT = OFF        ; Watchdog Timer Enable bit (WDT disabled (control is placed on the SWDTEN bit))
; CONFIG3H
CONFIG LPT1OSC = OFF    ; Low-Power Timer1 Oscillator Enable bit (Timer1 configured for higher power operation)
CONFIG MCLRE = ON       ; MCLR Pin Enable bit (MCLR pin enabled; RE3 input pin disabled)
; CONFIG4L
CONFIG LVP = OFF        ; Single-Supply ICSP Enable bit (Single-Supply ICSP disabled)
CONFIG XINST = OFF      ; Extended Instruction Set Enable bit (Instruction set extension and Indexed Addressing mode disabled (Legacy mode))
CONFIG DEBUG = OFF      ; Disable In-Circuit Debugger



GLOBAL var1
GLOBAL var2
GLOBAL var3
GLOBAL counter1
GLOBAL counter2
GLOBAL previousRe0
GLOBAL currentRe0
GLOBAL previousRe1
GLOBAL currentRe1
GLOBAL progressC_enabled
GLOBAL progressB_enabled
GLOBAL progressC_current
GLOBAL progressB_current
    
; Define space for the variables in RAM
PSECT udata_acs
var1:
    DS 1 ; Allocate 1 byte for var1
var2:
    DS 1 
var3:
    DS 1
previousRe0:
    DS 1
currentRe0:
    DS 1
previousRe1:
    DS 1
currentRe1:
    DS 1
counter1:
    DS 1 
counter2:
    DS 1  
progressC_enabled:
    DS 1 ; 1 if the progress bar on PORTC is enabled, 0 if disabled
progressB_enabled:
    DS 1
progressC_current:
    DS 1 ; The current value of the progress bar on PORTC
progressB_current:
    DS 1 ; The current value of the progress bar on PORTB

PSECT resetVec,class=CODE,reloc=2
resetVec:
    goto       main

PSECT CODE
main:
    clrf var1 ; set every bit in var1 to 0
    clrf var2	
    clrf var3	
    clrf previousRe0
    clrf currentRe0
    clrf previousRe1
    clrf currentRe1
    clrf counter1
    clrf counter2
    clrf progressC_enabled
    clrf progressB_enabled
    clrf progressC_current
    clrf progressB_current
    
    ; PORTB
    ; LATB
    ; TRISB determines whether the port is input/output
    ; set output ports
    clrf TRISB ; PORTB is output
    clrf TRISC ; PORTC is output
    clrf TRISD ; PORTD is output
    setf TRISE ; PORTE is input, RE0 toggle C, RE1 toggle B
    
    setf PORTB
    setf LATC ; light up all pins in PORTC
    setf LATD
    
    call busy_wait
    
    clrf PORTB
    clrf LATC ; turn off all pins in PORTC
    clrf LATD
    
    movlw 50 ; wreg = 50
    movwf counter1 ; counter1 = 50
    movlw 50 ; wreg = 50
    movwf counter2 ; counter2 = 50
    
main_loop:
    ; Round robin approach
    call check_buttons
    call update_display
    goto main_loop
    
; Subroutine to check the button states
check_buttons:
    call check_Re0
    call check_Re1
    return
    
; Subroutine to update the progress bar display on ports
update_display:
    ; Check counter1 and counter2 to check 500ms is passed
    decfsz counter1 ; decrease counter1, if it is 0, skip next instruction
    goto update_display_end ; if counter1 != 0, branch to update_display_end
    decfsz counter2 ; decrease counter2, if it is 0, skip next instruction
    goto update_display_end ; if counter2 != 0, branch to update_display_end

    ; 500ms has passed, reset the counters
    movlw 50 ; wreg = 50
    movwf counter1 ; counter1 = 50
    movlw 50 ; wreg = 50
    movwf counter2 ; counter2 = 50

    ; Toggle RD0
    ; To toggle, we use xorwf with a mask that has a '1' for the bit we want to toggle
    movlw 0x01 ; Load the mask for RD0
    xorwf LATD, F ; Toggle RD0 by performing an exclusive OR on the current value of LATD
    
    ; Update the progress bar on PORTB  
    btfsc progressB_enabled, 0 ; Skip next step if progress bar on PORTB is not enabled
    call update_portB_progress ; Call subroutine to update PORTB's progress bar
    btfss progressB_enabled, 0 ; Skip next step if progress bar on PORTB is enabled
    clrf LATB ; Clear the progress bar on PORTB
    btfss progressB_enabled, 0 ; Skip next step if progress bar on PORTB is enabled
    clrf progressB_current ; Clear the progress bar status of PORTB
    
    ; Update the progress bar on PORTC  
    btfsc progressC_enabled, 0 ; Skip next step if progress bar on PORTC is not enabled
    call update_portC_progress ; Call subroutine to update PORTC's progress bar
    btfss progressC_enabled, 0 ; Skip next step if progress bar on PORTC is enabled
    clrf LATC ; Clear the progress bar on PORTB
    btfss progressC_enabled, 0 ; Skip next step if progress bar on PORTC is enabled
    clrf progressC_current ; Clear the progress bar status of PORTC

    update_display_end:
        return
	
; Subroutine to update progress bar on PORTB, start from 0th bit light everything until all of them are on, then clear and repeat
update_portB_progress:
    ; Check if the progress bar is full
    movlw 0xFF ; Load the mask for all bits set
    cpfseq LATB ; Compare the mask with the current value of LATB
    goto increment_progressB ; If they are equal, branch to increment_progressB
    
    ; The progress bar is full so clear it
    clrf LATB ; Clear the progress counter
    clrf progressB_current 
    return

increment_progressB:
    ; Left shift and increment the progress counter
    rlncf progressB_current ; Rotate left no carry 
    incf progressB_current ; Increment the progress counter
    
    ; Update the progress bar
    movf progressB_current, W ; Move the progress counter to Wreg
    movwf LATB ; Move the progress counter to LATB
    
    return 
    
; Subroutine to update progress bar on PORTC, start from 0th bit light everything until all of them are on, then clear and repeat
update_portC_progress:
    ; Check if the progress bar is full
    movlw 0xFF ; Load the mask for all bits set
    cpfseq LATC ; Compare the mask with the current value of LATC
    goto increment_progressC ; If they are equal, branch to increment_progressC
    
    ; The progress bar is full so clear it
    clrf LATC ; Clear the progress counter
    clrf progressC_current
    return

increment_progressC:
    ; Right shift and add 128 to the progress counter
    rrncf progressC_current ; Rotate right no carry
    movf progressC_current, W ; Move the progress counter to Wreg
    addlw 128 ; Add 128 to Wreg
    movwf progressC_current ; Move the result back to the progress counter
    
    ; Update the progress bar
    movf progressC_current, W ; Move the progress counter to Wreg
    movwf LATC ; Move the Wreg to LATC
    
    return 
    
check_Re0:
    ; Read the current state of PORTE into 'current'
    movf PORTE, W ; move input in PORTE to Wreg
    movwf currentRe0

    ; Check for RE0 release
    btfsc currentRe0, 0     ; Check if RE0 is currently high, bit test file, skip if clear
    bsf previousRe0, 0      ; If it is high, set the corresponding bit in 'previous', bit set file
    btfss currentRe0, 0     ; Check if RE0 is currently low, bit test file, skip if set
    call re0_pressed	    ; If it is low, call the re0_pressed subroutine, relative call
    
    return

; Subroutine to handle RE0 button press
re0_pressed:
    ; Check if RE0 was previously high (1)
    btfss previousRe0, 0 ; bit test file, skip if set
    return ; Return if it was not high, meaning we have already handled the press
    
    ; Read the current state of progressC_enabled
    movf progressC_enabled, W ; Move progressC_enabled to W
    xorlw 0x01 ; Toggle the bit in W (if 0 becomes 1, if 1 becomes 0)
    movwf progressC_enabled ; Move the toggled state back to progressC_enabled

    ; Clear the bit in 'previous' to indicate we have handled the press
    bcf previousRe0, 0 ; bit clear file
    return
    
check_Re1:
    ; Read the current state of PORTE into 'currentRe1'
    movf PORTE, W ; move input in PORTE to wreg
    movwf currentRe1 ; move wreg to currentRe1

    ; Check for RE1 release
    btfsc currentRe1, 1     ; Check if RE1 is currently high
    bsf previousRe1, 0      ; If it is high, set the corresponding bit in 'previousRe1'
    btfss currentRe1, 1     ; Check if RE1 is currently low
    call re1_pressed       ; If it is low, call the re1_pressed subroutine

    return
    
; Subroutine to handle RE1 button press
re1_pressed:
    ; Check if RE1 was previously high (1)
    btfss previousRe1, 0 ; bit test file, skip if set, if previousRe1's 0th bit is 1, skip next instruction
    return ; Return if it was not high, meaning we have already handled the press

    ; Read the current state of progressB_enabled
    movf progressB_enabled, W ; Move progressB_enabled to W
    xorlw 0x01 ; Toggle the bit in W (if 0 becomes 1, if 1 becomes 0)
    movwf progressB_enabled ; Move the toggled state back to progressB_enabled

    ; Clear the bit in 'previous' to indicate we have handled the press
    bcf previousRe1, 0 ; bit clear file, clear the 0th bit in previousRe1
    return

; Subroutine to busy wait for 1000ms
busy_wait:
    movlw 250 ; wreg = 250
    movwf var2 ; var2 = 250
    movlw 230 ; wreg = 230
    movwf var1 ; var1 = 230
    outer_loop_start:
        loop_start:
            setf var3 ; set all bits in var3 to 1
            inner_loop_start:
                decf var3 ; var3--
                bnz inner_loop_start ; if var3 != 0, branch to inner_loop_start
            incfsz var1 ; increase var1, if it is 0, skip next instruction
            bra loop_start ; branch to loop_start
        incfsz var2 ; increase var2, if it is 0, skip next instruction
        bra outer_loop_start ; branch to outer_loop_start
    return
     
end resetVec
