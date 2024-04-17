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

#define DOT_PIECE    {.x = 0, .y = 0, .shape = {1, 0, 0, 0}}
#define SQUARE_PIECE {.x = 0, .y = 0, .shape = {1, 1, 1, 1}}
#define L_PIECE      {.x = 0, .y = 0, .shape = {1, 1, 1, 0}}

#define TOP_LEFT     curTet.shape.b0
#define TOP_RIGHT    curTet.shape.b1
#define BOTTOM_LEFT  curTet.shape.b2
#define BOTTOM_RIGHT curTet.shape.b3

#define GET_BOARD(x, y)    ((board.col##x >> y) & 0x01)
#define SET_BOARD(x, y, v) board.col##x |= (v << y)

#define IS_OUTSIDE(x, y) (x < 0 || x > 3 || y > 7 || y < 0)

#define bit _Bool

typedef union
{
    char byte;

    struct
    {
        unsigned int b0: 1;
        unsigned int b1: 1;
        unsigned int b2: 1;
        unsigned int b3: 1;
        unsigned int br: 4;
    };
} Shape;

typedef struct
{
    char col0;
    char col1;
    char col2;
    char col3;
} Board;

typedef struct Tetromino
{
    char x, y; // position of top left corner
    Shape shape;
} Tetromino;

Tetromino curTet;
Board board;

unsigned char lastPortB;

void InitBoard();
void SetBoard(char x, char y, char v);
char GetBoard(char x, char y);
void InitTimers();
void InitInterrupts();

int BitwiseAnd(bit region1[4], bit region2[4]);
void ExtractBoard(char x, char y, Shape *shape);
bit CheckCollision(bit dir0, bit dir1);

void UpdateBoard();
void RenderBoard();

void HandleTimer();
void HandlePortB();

// ============================ //
//          GLOBALS             //
// ============================ //

// You can write globals definitions here...

// ============================ //
//          FUNCTIONS           //
// ============================ //

void InitBoard()
{
    // Write to LAT, read from PORT
    LATC = 0x00;
    LATB = 0x00;
    LATD = 0x00;
    LATF = 0x00;

    // All outputs
    TRISC = 0x00;
    TRISD = 0x00;
    TRISE = 0x00;
    TRISF = 0x00;

    for (char i = 0; i < 8; i++)
    {
        for (char j = 0; j < 4; j++)
        {
            SetBoard(j, i, 0);
        }
    }

    SetBoard(2, 3, 1);
}

void SetBoard(char x, char y, char v)
{
    switch (x)
    {
        case 0:
            board.col0 |= (v << y);
            break;
        case 1:
            board.col1 |= (v << y);
            break;
        case 2:
            board.col2 |= (v << y);
            break;
        case 3:
            board.col3 |= (v << y);
            break;
    }
}

char GetBoard(char x, char y)
{
    char col;
    switch (x)
    {
        case 0:
            col = board.col0;
            break;
        case 1:
            col = board.col1;
            break;
        case 2:
            col = board.col2;
            break;
        case 3:
            col = board.col3;
            break;
    }
    return ((col >> y) & 0x01);
}

void InitTimers()
{
    T0CON = 0x07; // Set Timer0 to increment every 256 clock cycles
    TMR0 = 0; // Set Timer0 count to 0
    INTCONbits.TMR0IE = 1; // Enable Timer0 interrupt
}

void InitInterrupts()
{
    // TODO: Check the current state of RBIF in INTCON

    INTCONbits.GIE  = 1; // Global Interrupt Enable
    INTCONbits.PEIE = 0; // Peripheral Interrupt Enable
    INTCONbits.RBIE = 1; // PORTB Interrupt Enable

    TRISB = 0b01100000; // PORTB5 and PORTB6 as inputs
}

int BitwiseAnd(bit region1[4], bit region2[4])
{
    for (int i = 0; i < 4; ++i)
        if (region1[i] & region2[i]) return 1;
    
    return 0;
}

void ExtractBoard(char x, char y, Shape *shape)
{

}

/*
 * dir0, dir1: 1, 0 > +x direction
 * dir0, dir1: 0, 0 > -x direction
 * dir0, dir1: 0, 1 > +y direction
 * dir0, dir1: 1, 1 > -y direction
 */
bit CheckCollision(bit dir0, bit dir1)
{
    bit reg[4];
    
    ExtractBoard(dir1 == 0 ? ( dir0 == 0 ? curTet.x - 1 : curTet.x + 1) : curTet.x, 
                 (dir1 == 1) ? curTet.y : curTet.y + dir1, reg); //black magic
    
    //return BitwiseAnd(reg, curTet.shape);
    return 0;
}

void UpdateBoard()
{
    /*
     * Bitwise ORed, since if board value is 1 and 
     * tetromino's value is 0 at the same position, it should not set board to 0
     */
    
    
}

void RenderBoard()
{
    PORTC = board.col0;
    PORTD = board.col1;
    PORTE = board.col2;
    PORTF = board.col3;
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
    unsigned char portBState = PORTB; // Read the current state of Port B
    unsigned char changedBits = portBState ^ lastPortB; // Determine which bits have changed

    // Specifically check for changes in bits 5 and 6
    if (changedBits & (1 << 5)) 
    { // Check if RB5 has changed
        if (portBState & (1 << 5))
        {
            // Handle logic for RB5 going high
            // Example: printf("RB5 went high\n");
        }
        else
        {
            // Handle logic for RB5 going low
            // Example: printf("RB5 went low\n");
        }
    }

    if (changedBits & (1 << 6))
    { // Check if RB6 has changed
        if (portBState & (1 << 6))
        {
            // Handle logic for RB6 going high
            // Example: printf("RB6 went high\n");
        }
        else
        {
            // Handle logic for RB6 going low
            // Example: printf("RB6 went low\n");
        }
    }

    lastPortB = portBState; // Update last known state of Port B
    // INTCONbits.RBIF = 0; // Clear the interrupt flag
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
