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

#define TOP_LEFT curTet.shape[0]
#define TOP_RIGHT curTet.shape[1]
#define BOTTOM_LEFT curTet.shape[2]
#define BOTTOM_RIGHT curTet.shape[3]

#define IS_OUTSIDE(x, y) x < 0 || x > 3 || y > 7 || y < 0

#define bit __bit

typedef struct Tetromino
{
    char x, y; //position of top left corner
    bit shape[4]; // top left, top right, bottom right, bottom left
} Tetromino;

Tetromino curTet;
bit board[8][4];

unsigned char lastPortB;

// ============================ //
//          GLOBALS             //
// ============================ //

// You can write globals definitions here...

// ============================ //
//          FUNCTIONS           //
// ============================ //

void InitBoard()
{
    TRISC = 0x00; 
    TRISD = 0x00;
    TRISE = 0x00;
    TRISF = 0x00;
    
    PORTC = 0x00;
    PORTB = 0x00;
    PORTD = 0x00;
    PORTF = 0x00;
    
    for (char i = 0; i < 8; i++)
    {
        for (char j = 0; j < 4; j++)
        {
            board[i][j] = 0;
        }
    }
    
    board[2][3] = 1;
}

void InitInterrupts()
{
    INTCONbits.GIE = 1; // Global Interrupt Enable
    INTCONbits.PEIE = 1; // Peripheral Interrupt Enable
    INTCONbits.RBIE = 1;
    
    TRISB = 0b01100000;
}

void InitTimers()
{
    T0CON = 0x07; // Set Timer0 to increment every 256 clock cycles
    TMR0 = 0; // Set Timer0 count to 0
    INTCONbits.TMR0IE = 1; // Enable Timer0 interrupt
}

int BitwiseAnd(bit region1[4], bit region2[4])
{
    for (int i = 0; i < 4; ++i)
        if (region1[i] & region2[i]) return 1;
    
    return 0;
}

void ExtractBoard(char x, char y, bit reg[4])
{
    reg[0] = IS_OUTSIDE(x,y) ? 1 : board[y][x];
    reg[1] = IS_OUTSIDE(x,y) ? 1 : board[y][x+1];
    reg[2] = IS_OUTSIDE(x,y) ? 1 : board[y+1][x+1];
    reg[3] = IS_OUTSIDE(x,y) ? 1 : board[y+1][x];
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
                 (dir1 == 1) ? : curTet.y + dir1, reg); //black magic
    
    return BitwiseAnd(reg, curTet.shape);
}

void UpdateBoard()
{
    /*
     * Bitwise ORed, since if board value is 1 and 
     * tetromino's value is 0 at the same position, it should not set board to 0
     */
    
    board[curTet.y][curTet.x] |= curTet.shape[0];
    board[curTet.y][curTet.x + 1] |= curTet.shape[1];
    board[curTet.y + 1][curTet.x + 1] |= curTet.shape[2];
    board[curTet.y + 1][curTet.x] |= curTet.shape[3];
}

void RenderBoard()
{
    PORTCbits.RC0 = board[0][0];
    PORTDbits.RD0 = board[0][1];
    PORTEbits.RE0 = board[0][2];
    PORTFbits.RF0 = board[0][3];

    PORTCbits.RC1 = board[1][0];
    PORTDbits.RD1 = board[1][1];
    PORTEbits.RE1 = board[1][2];
    PORTFbits.RF1 = board[1][3];

    PORTCbits.RC2 = board[2][0];
    PORTDbits.RD2 = board[2][1];
    PORTEbits.RE2 = board[2][2];
    PORTFbits.RF2 = board[2][3];

    PORTCbits.RC3 = board[3][0];
    PORTDbits.RD3 = board[3][1];
    PORTEbits.RE3 = board[3][2];
    PORTFbits.RF3 = board[3][3];

    PORTCbits.RC4 = board[4][0];
    PORTDbits.RD4 = board[4][1];
    PORTEbits.RE4 = board[4][2];
    PORTFbits.RF4 = board[4][3];

    PORTCbits.RC5 = board[5][0];
    PORTDbits.RD5 = board[5][1];
    PORTEbits.RE5 = board[5][2];
    PORTFbits.RF5 = board[5][3];

    PORTCbits.RC6 = board[6][0];
    PORTDbits.RD6 = board[6][1];
    PORTEbits.RE6 = board[6][2];
    PORTFbits.RF6 = board[6][3];

    PORTCbits.RC7 = board[7][0];
    PORTDbits.RD7 = board[7][1];
    PORTEbits.RE7 = board[7][2];
    PORTFbits.RF7 = board[7][3];
}

// ============================ //
//   INTERRUPT SERVICE ROUTINE  //
// ============================ //
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
