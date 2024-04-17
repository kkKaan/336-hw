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

#define TOP_LEFT curTet.shape.b0
#define TOP_RIGHT curTet.shape.b1
#define BOTTOM_LEFT curTet.shape.b2
#define BOTTOM_RIGHT curTet.shape.b3

#define GET_BOARD(x, y) ((board.col##x >> y) & 0x01)
#define SET_BOARD(x, y, v) board.col##x |= (v << y)

#define IS_OUTSIDE(x, y) (x < 0 || x > 3 || y > 7 || y < 0)

#define bit _Bool

typedef union Shape
{
    char byte;
    
    struct
    {
        unsigned int r0: 1;
        unsigned int r1: 1;
        unsigned int r2: 1;
        unsigned int r3: 1;
        
        unsigned int b3: 1;
        unsigned int b2: 1;
        unsigned int b1: 1;
        unsigned int b0: 1;
    };
} Shape;


typedef struct Board
{
    char col0;
    char col1;
    char col2;
    char col3;
} Board;

typedef struct Tetromino
{
    char type;
    char x, y; //position of top left corner
    Shape shape;
} Tetromino;

static const Tetromino DOT_PIECE = {.x = 0, .y = 0, .shape = { .byte = 0x80 }};
static const Tetromino SQUARE_PIECE = { .x = 0, .y = 0, .shape = { .byte = 0xF0 }};
static const Tetromino L_PIECE = {.x = 0, .y = 0, .shape = { .byte = 0xE0 }};

char prevA;

Tetromino curTet;
Board board;

unsigned char lastPortB;

void RotateShape(Shape *shape);

// ============================ //
//          GLOBALS             //
// ============================ //

// You can write globals definitions here...

// ============================ //
//          FUNCTIONS           //
// ============================ //

void SetBoard(char x, char y, char v)
{
    switch (x)
    {
        case 0:
            board.col0 = v == 1 ? board.col0 | (v << y) : board.col0 & ~(1 << y);
            break;
        case 1:
            board.col1 = v == 1 ? board.col1 | (v << y) : board.col1 & ~(1 << y);
            break;
        case 2:
            board.col2 = v == 1 ? board.col2 | (v << y) : board.col2 & ~(1 << y);
            break;
        case 3:
            board.col3 = v == 1 ? board.col3 | (v << y) : board.col3 & ~(1 << y);
            break;
    };
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
            SetBoard(j, i, 1);
        }
    }
    
    SetBoard(2, 3, 1);
    curTet = L_PIECE;
    RotateShape(&curTet.shape);
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

int BitwiseAnd(Shape shape1, Shape shape2)
{
    if (shape1.b0 && shape2.b0) return 1;
    if (shape1.b1 && shape2.b1) return 1;
    if (shape1.b2 && shape2.b2) return 1;
    if (shape1.b3 && shape2.b3) return 1;
    
    return 0;
}

//From board
void GetQuartet(char x, char y, Shape *shape)
{
    shape->b0 = GetBoard(x, y);
    shape->b1 = GetBoard(x + 1, y);
    shape->b2 = GetBoard(x + 1, y + 1);
    shape->b3 = GetBoard(x, y + 1);
}

//Set board
void SetQuartet(char x, char y, Shape *shape)
{
    SetBoard(x, y, (char) shape->b0);
    SetBoard(x + 1, y, (char) shape->b1);
    SetBoard(x + 1, y + 1, (char) shape->b2);
    SetBoard(x, y + 1, (char) shape->b3);
}

/*
 * dir0, dir1: 1, 0 > +x direction
 * dir0, dir1: 0, 0 > -x direction
 * dir0, dir1: 0, 1 > +y direction
 * dir0, dir1: 1, 1 > -y direction
 */
bit CheckCollision(bit dir0, bit dir1)
{
    Shape shape;
    
    GetQuartet(dir1 == 0 ? ( dir0 == 0 ? curTet.x - 1 : curTet.x + 1) : curTet.x, 
                 (dir1 == 1) ? curTet.y : curTet.y + dir1, &shape); //black magic
    
    return BitwiseAnd(shape, curTet.shape);
}

void RotateShape(Shape *shape)
{
    shape->byte >>= 1;
    shape->b0 = shape->r3;
    shape->r3 = 0;
}

void ListenPortA()
{
    if (PORTAbits.RA0)
}

void UpdateBoard()
{
    /*
     * Bitwise ORed, since if board value is 1 and 
     * tetromino's value is 0 at the same position, it should not set board to 0
     */
    
    Shape bq;
    GetQuartet(curTet.x, curTet.y, &bq);
    bq.byte |= curTet.shape.byte;
    SetQuartet(curTet.x, curTet.y, &bq);
}

void Update()
{
    ListenPortA();
    UpdateBoard();
}

void Render()
{
    PORTC = board.col0;
    PORTD = board.col1;
    PORTE = board.col2;
    PORTF = board.col3;
}

// ============================ //
//   INTERRUPT SERVICE ROUTINE  //
// ============================ //
void HandleTimer()
{
    //TODO: timer
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
            // TODO: Handle logic for RB5 going high

        }
        else
        {
            // TODO: Handle logic for RB5 going low

        }
    }

    if (changedBits & (1 << 6))
    { // Check if RB6 has changed
        if (portBState & (1 << 6))
        {
            // TODO: Handle logic for RB6 going high
        }
        else
        {
            // TODO: Handle logic for RB6 going low
        }
    }

    lastPortB = portBState; // Update last known state of Port B
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
        Update();
        Render();
    }
}
