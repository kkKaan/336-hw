// ============================ //
// Do not edit this part!!!!    //
// ============================ //
// 0x300001 - CONFIG1H
#pragma config OSC = HSPLL      // Oscillator Selection bits (HS oscillator,
                                // PLL enabled (Clock Frequency = 4 x FOSC1))
#pragma config FCMEN = OFF      // Fail-Safe Clock Monitor Enable bit
                                // (Fail-Safe Clock Monitor disabled)
#pragma config IESO = OFF       // Internal/External Oscillator Switchover bit
                                // (Oscillator Switchover mode disabled)
// 0x300002 - CONFIG2L
#pragma config PWRT = OFF       // Power-up Timer Enable bit (PWRT disabled)
#pragma config BOREN = OFF      // Brown-out Reset Enable bits (Brown-out
                                // Reset disabled in hardware and software)
// 0x300003 - CONFIG1H
#pragma config WDT = OFF        // Watchdog Timer Enable bit
                                // (WDT disabled (control is placed on the SWDTEN bit))
// 0x300004 - CONFIG3L
// 0x300005 - CONFIG3H
#pragma config LPT1OSC = OFF    // Low-Power Timer1 Oscillator Enable bit
                                // (Timer1 configured for higher power operation)
#pragma config MCLRE = ON       // MCLR Pin Enable bit (MCLR pin enabled;
                                // RE3 input pin disabled)
// 0x300006 - CONFIG4L
#pragma config LVP = OFF        // Single-Supply ICSP Enable bit (Single-Supply
                                // ICSP disabled)
#pragma config XINST = OFF      // Extended Instruction Set Enable bit
                                // (Instruction set extension and Indexed
                                // Addressing mode disabled (Legacy mode))

#pragma config DEBUG = OFF      // Disable In-Circuit Debugger

#define KHZ 1000UL
#define MHZ (KHZ * KHZ)
#define _XTAL_FREQ (40UL * MHZ)

// ============================ //
//             End              //
// ============================ //

#include <xc.h>

// ============================ //
//        DEFINITIONS           //
// ============================ //

#define DOT_PIECE {.x = 0, .y = 0, .shape = {1, 0, 0, 0}}
#define SQUARE_PIECE {.x = 0, .y = 0, .shape = {1, 1, 1, 1}}
#define L_PIECE {.x = 0, .y = 0, .shape = {1, 1, 1, 0}}

typedef struct Tetromino
{
    char x, y; //position of top left corner
    bit shape[4];
} Tetromino;

Tetromino curTetromino;
bit board[8][4];

// ============================ //
//          GLOBALS             //
// ============================ //

// You can write globals definitions here...

// ============================ //
//          FUNCTIONS           //
// ============================ //

void InitBoard()
{
    // Initialize all board cells to 0
    for (char i = 0; i < 8; i++)
    {
        for (char j = 0; j < 4; j++)
        {
            board[i][j] = 0;
        }
    }
}

void InitInterrupts()
{
    INTCONbits.GIE = 1; // Global Interrupt Enable
    INTCONbits.PEIE = 1; // Peripheral Interrupt Enable
}

void InitTimers()
{
    T0CON = 0x07; // Set Timer0 to increment every 256 clock cycles
    TMR0 = 0; // Set Timer0 count to 0
    INTCONbits.TMR0IE = 1; // Enable Timer0 interrupt
}

void UpdateBoard()
{
    board[curTetromino.y][curTetromino.x] = curTetromino.shape[0];
    board[curTetromino.y][curTetromino.x + 1] = curTetromino.shape[1];
    board[curTetromino.y + 1][curTetromino.x + 1] = curTetromino.shape[2];
    board[curTetromino.y + 1][curTetromino.x] = curTetromino.shape[3];
}

void RenderBoard()
{
    
}

// ============================ //
//   INTERRUPT SERVICE ROUTINE  //
// ============================ //
__interrupt(high_priority)
void HandleInterrupt()
{
    if (INTCONbits.TMR0IF) 
    {
        HandleTimer();
        INTCONbits.TMR0IF = 0; // Clear Timer0 interrupt flag
    }
    else if (INTCONbits.RBIF)
    {
        HandlePortB();
        INTCONbits.RBIF = 0;
    }
}

void HandleTimer()
{
    
}

void HandlePortB()
{
    // Logic for handling PortB interrupts
}

// ============================ //
//            MAIN              //
// ============================ //
void main()
{
    InitBoard();
    InitTimers();
    InitInterrupts();

    while (1) 
    {
        UpdateBoard();
        RenderBoard();
    }
}
