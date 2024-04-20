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

#define TOP_LEFT     curTet.shape.b0
#define TOP_RIGHT    curTet.shape.b1
#define BOTTOM_LEFT  curTet.shape.b2
#define BOTTOM_RIGHT curTet.shape.b3

#define IS_OUTSIDE(x, y) (((x) < 0) || ((x) > 3) || ((y) > 7) || ((y) < 0))

// TODO: Change to appropriate values
#define T_PRESCALER    0x05
#define T_PRELOAD_HIGH 0x67
#define T_PRELOAD_LOW  0x69

#define bit char

#define DOT_PIECE 0
#define SQUARE_PIECE 1
#define L_PIECE 2

typedef union
{
    char byte;
    
    struct
    {
        unsigned int r0: 1; //least significant bit
        unsigned int r1: 1;
        unsigned int r2: 1;
        unsigned int r3: 1;
        
        unsigned int b3: 1;
        unsigned int b2: 1;
        unsigned int b1: 1; 
        unsigned int b0: 1; //most significant bit
    };
} Shape;

typedef struct
{
    char col0;
    char col1;
    char col2;
    char col3;
} Board;

typedef struct
{
    char type;
    char x, y; // position of top left corner
    Shape shape;
} Tetromino;

// Lookup table for the 7-segment display (0-9, common cathode)
const char segmentLookup[10] = {
    0x3F, // 0
    0x06, // 1
    0x5B, // 2
    0x4F, // 3
    0x66, // 4
    0x6D, // 5
    0x7D, // 6
    0x07, // 7
    0x7F, // 8
    0x6F  // 9
};

static const Tetromino DOT    = {.type = DOT_PIECE, .x = 0, .y = 0, .shape = { .byte = 0x80 }};
static const Tetromino SQUARE = {.type = SQUARE_PIECE, .x = 0, .y = 0, .shape = { .byte = 0xF0 }};
static const Tetromino L      = {.type = L_PIECE, .x = 0, .y = 0, .shape = { .byte = 0xE0 }};

Tetromino curTet;
Board board;
Board buffer;

char pieces = 0;
char counter = 0;

char prevA;
char lastPortA;
char lastPortB;

// 1 if displayed, 0 o.w.
char curTetDisplayed = 1;

void InitBoard();
void InitTimers();
void InitInterrupts();

void Update();
void Render();

void SetBoard(char x, char y, char v, Board* board);
char GetBoard(char x, char y, Board* board);

void ListenPortA();
void UpdateBoard();
void UpdateBuffer();

char BitwiseAnd(Shape shape1, Shape shape2);
void GetQuartet(char x, char y, Shape *shape, Board* board);
void GetQuartet2(char x, char y, Shape *shape);
void SetQuartet(char x, char y, Shape *shape, Board* board);

char IsColliding(bit dir0, bit dir1);
// void Move(bit dir0, bit dir1);
void RotateShape(Shape *shape);
char ShapeInBounds(bit dir0, bit dir1);
void Submit();

void HandleTimer();
void HandlePortB();

void DisplayOn7Segment(const char num);

// ============================ //
//          GLOBALS             //
// ============================ //

// You can write globals definitions here...

// ============================ //
//          FUNCTIONS           //
// ============================ //

void InitBoard()
{
    ADCON1 = 0x0F;

    // 0, 1, 2, 3rd bits are inputs
    PORTG = 0x00;
    LATG = 0x00;
    TRISG = 0b00011101;
    lastPortA = PORTG;

    // 5, 6th bits are inputs
    PORTB = 0x00;
    LATB = 0x00;
    TRISB = 0b10010000;
    lastPortB = PORTB;

    // Write to LAT, read from PORT
    LATC = 0x00;
    LATD = 0x00;
    LATE = 0x00;
    LATF = 0x00;

    // All outputs
    TRISC = 0x00;
    TRISD = 0x00;
    TRISE = 0x00;
    TRISF = 0x00;
    TRISJ = 0x00;
    TRISH = 0x00;

    for (char i = 0; i < 8; i++)
    {
        for (char j = 0; j < 4; j++)
        {
            SetBoard(j, i, 0, &board);
        }
    }

    // SetBoard(2, 3, 1);
    curTet = DOT;
    // RotateShape(&curTet.shape);
}

void InitTimers()
{
    T0CON = 0x00;           // Reset Timer0

    T0CON |= T_PRESCALER;   // Load prescaler
    TMR0H = T_PRELOAD_HIGH; // Pre-load the value
    TMR0L = T_PRELOAD_LOW;

    T0CONbits.TMR0ON = 1;   // Enable Timer0
}

void InitInterrupts()
{
    // TODO: Check the current state of RBIF in INTCON

    INTCON = 0x00;  // Clear all flags

    RCONbits.IPEN = 0;     // Disable interrupt priorities

    INTCONbits.PEIE   = 0; // Disable peripheral interrupts
    INTCONbits.TMR0IE = 1; // Enable TMR0 interrupts
    INTCONbits.RBIE   = 1; // Enable RB Port interrupts
    
    INTCON2bits.TMR0IP = 1;
    INTCON2bits.RBIP = 1;
    
    INTCONbits.INT0IE = 1;
    
    INTCONbits.GIE    = 1; // Enable global interrupts
}

void Update()
{
    ListenPortA();
    UpdateBoard();
    UpdateBuffer();
}

void Render()
{
    LATC = buffer.col0;
    LATD = buffer.col1;
    LATE = buffer.col2;
    LATF = buffer.col3;
    DisplayOn7Segment(pieces);
}

void SetBoard(char x, char y, char v, Board* board)
{
    switch (x)
    {
        case 0:
            board->col0 = v == 1 ? board->col0 | (v << y) : board->col0 & ~(1 << y);
            break;
        case 1:
            board->col1 = v == 1 ? board->col1 | (v << y) : board->col1 & ~(1 << y);
            break;
        case 2:
            board->col2 = v == 1 ? board->col2 | (v << y) : board->col2 & ~(1 << y);
            break;
        case 3:
            board->col3 = v == 1 ? board->col3 | (v << y) : board->col3 & ~(1 << y);
            break;
    };
}

char GetBoard(char x, char y, Board *board)
{
    char col;
    switch (x)
    {
        case 0:
            col = board->col0;
            break;
        case 1:
            col = board->col1;
            break;
        case 2:
            col = board->col2;
            break;
        case 3:
            col = board->col3;
            break;
    }
    
    return ((col >> y) & 0x01);
}

void ListenPortA()
{
    // Read the current state of Port A
    char currentPortA = PORTG;
    // Determine which bits have changed
    char changedBits = currentPortA ^ lastPortA;

    // Specifically check for changes in bits 0, 1, 2, 3
    char x = curTet.x;
    char y = curTet.y;
    if (changedBits & (1 << 0)) // right
    {
        if (currentPortA & (1 << 0))
        {
            if(ShapeInBounds(1, 0))
            {
                curTet.x++;
            }
        }
    }

    if (changedBits & (1 << 2)) // up
    {
        if (currentPortA & (1 << 2))
        {
            if(ShapeInBounds(1, 1))
            {
                curTet.y--;
            }
        }
    }

    if (changedBits & (1 << 3)) // down
    {
        if (currentPortA & (1 << 3))
        {
            if(ShapeInBounds(0, 1))
            {
                curTet.y++;
            }
        }
    }

    if (changedBits & (1 << 4)) // left
    {
        if (currentPortA & (1 << 4))
        {
            if(ShapeInBounds(0, 0))
            {
                curTet.x--;
            }
        }
    }

    // Update last known state of Port A
    lastPortA = currentPortA;
}

/*
 * Bitwise ORed, since if board value is 1 and 
 * tetromino's value is 0 at the same position, it should not set board to 0
 */
void UpdateBoard()
{   
    INTCONbits.TMR0IE = 0;
    Shape bq;
//    GetQuartet(curTet.x, curTet.y, &bq);
//    bq.byte |= curTet.shape.byte;
//    
//    SetQuartet(curTet.x, curTet.y, &bq);
    INTCONbits.TMR0IE = 1;
}

void UpdateBuffer()
{
    INTCONbits.TMR0IE = 0;
    buffer = board;

    Shape bq;
    if (curTetDisplayed)
    {
        GetQuartet(curTet.x, curTet.y, &bq, &buffer);
        bq.byte |= curTet.shape.byte;
        SetQuartet(curTet.x, curTet.y, &bq, &buffer);
    }
    else
    {
        Shape tempShape = curTet.shape;
        tempShape.byte = ~tempShape.byte;
        tempShape.byte &= 0xF0;
        GetQuartet(curTet.x, curTet.y, &bq, &buffer);
        bq.byte &= tempShape.byte;
        SetQuartet(curTet.x, curTet.y, &bq, &buffer);
    }

    INTCONbits.TMR0IE = 1;
}

//returns true if at least one bit is overlapping
char BitwiseAnd(Shape shape1, Shape shape2)
{
    return ((shape1.byte & shape2.byte) != 0);
}

//From board
void GetQuartet(char x, char y, Shape *shape, Board* board)
{
    shape->b0 = IS_OUTSIDE(x, y) ? 1 : GetBoard(x, y, board);
    shape->b1 = IS_OUTSIDE(x + 1, y) ? 1 : GetBoard(x + 1, y, board);
    shape->b2 = IS_OUTSIDE(x + 1, y + 1) ? 1 : GetBoard(x + 1, y + 1, board);
    shape->b3 = IS_OUTSIDE(x, y + 1) ? 1 : GetBoard(x, y + 1, board);
}

void GetQuartet2(char x, char y, Shape *shape)
{
    shape->b0 = IS_OUTSIDE(x, y) ? 1 : 0;
    shape->b1 = IS_OUTSIDE(x + 1, y) ? 1 : 0;
    shape->b2 = IS_OUTSIDE(x + 1, y + 1) ? 1 : 0;
    shape->b3 = IS_OUTSIDE(x, y + 1) ? 1 : 0;
}

//Set board
void SetQuartet(char x, char y, Shape *shape, Board* board)
{
    SetBoard(x, y, (char) shape->b0, board);
    SetBoard(x + 1, y, (char) shape->b1, board);
    SetBoard(x + 1, y + 1, (char) shape->b2, board);
    SetBoard(x, y + 1, (char) shape->b3, board);
}

/*
 * dir0, dir1: 1, 0 > +x direction
 * dir0, dir1: 0, 0 > -x direction
 * dir0, dir1: 0, 1 > +y direction
 * dir0, dir1: 1, 1 > -y direction
 */
char IsColliding(bit dir0, bit dir1)
{
    Shape shape, curQuartet;
    
    GetQuartet(curTet.x, curTet.y, &curQuartet, &board);
    curQuartet.byte ^= curTet.shape.byte;
    SetQuartet(curTet.x, curTet.y, &curQuartet, &board);

    GetQuartet((dir1 == 0) ? ( dir0 == 0 ? curTet.x - 1 : curTet.x + 1) : curTet.x, 
               (dir1 == 1) ? ( dir0 == 0 ? curTet.y + 1 : curTet.y - 1) : curTet.y,
               &shape,
               &board); //black magic
    
    char returnVal = BitwiseAnd(shape, curTet.shape);
    
    GetQuartet(curTet.x, curTet.y, &curQuartet, &board);
    curQuartet.byte |= curTet.shape.byte;
    SetQuartet(curTet.x, curTet.y, &curQuartet, &board);

    return returnVal;
}

char IsSubmitable()
{
    Shape shape;

    GetQuartet(curTet.x, curTet.y, &shape, &board); //black magic
    
    return !BitwiseAnd(shape, curTet.shape);
}

char ShapeInBounds(bit dir0, bit dir1)
{
    Shape shape;

    GetQuartet2((dir1 == 0) ? ( dir0 == 0 ? curTet.x - 1 : curTet.x + 1) : curTet.x, 
               (dir1 == 1) ? ( dir0 == 0 ? curTet.y + 1 : curTet.y - 1) : curTet.y,
               &shape); //black magic

    return !BitwiseAnd(shape, curTet.shape);
}

void Submit()
{
    if (IsSubmitable())
    {
        buffer = board; 
        Shape bq;
        GetQuartet(curTet.x, curTet.y, &bq, &buffer);
        bq.byte |= curTet.shape.byte;
        SetQuartet(curTet.x, curTet.y, &bq, &buffer);

        board = buffer;
        pieces++;
        
        switch (curTet.type)
        {
            case DOT_PIECE:
                curTet = SQUARE;
                break;
            case SQUARE_PIECE:
                curTet = L;
                break;
            case L_PIECE:
                curTet = DOT;
                break;
        }
        counter = 0;
        curTetDisplayed = 0x01;
    }
    
}

/*
void Move(bit dir0, bit dir1)
{
    Shape curQuartet;
    GetQuartet(curTet.x, curTet.y, &curQuartet);
    
    //clear current position of tetromino
    curQuartet.byte ^= curTet.shape.byte;
    SetQuartet(curTet.x, curTet.y, &curQuartet);
    
    curTet.x = (dir1 == 0) ? ( dir0 == 0 ? curTet.x - 1 : curTet.x + 1) : curTet.x;
    curTet.y = (dir1 == 1) ? ( dir0 == 0 ? curTet.y + 1 : curTet.y - 1) : curTet.y;
    
    SetQuartet(curTet.x, curTet.y, &curTet.shape);
}
*/

void RotateShape(Shape *shape)
{
    shape->byte >>= 1;
    shape->b0 = shape->r3;
    shape->r3 = 0;
}

// ============================ //
//   INTERRUPT SERVICE ROUTINE  //
// ============================ //

__interrupt(high_priority)
void HandleInterrupt()
{
    if (INTCONbits.TMR0IF) 
    {
        INTCONbits.TMR0IF = 0; // Clear TMR0 interrupt flag
        HandleTimer();
        
        
        TMR0H = T_PRELOAD_HIGH; // Reset the value
        TMR0L = T_PRELOAD_LOW;
    }
    else if (INTCONbits.RBIF)
    {
        HandlePortB();
        INTCONbits.RBIF = 0; // Clear RB port interrupt flag
    }
    else if (INTCONbits.INT0IF)
    {
        INTCONbits.INT0IF = 0;
    }
}

void HandleTimer()
{   
    if (++counter == 8)
    {
        if(ShapeInBounds(0, 1))
        {
            curTet.y++;
        }
        counter = 0;
    }
    
    curTetDisplayed ^= 0x01;
}

void HandlePortB()
{
    __delay_ms(2);
    char currentPortB = PORTB; // Read the current state of Port B
    char changedBits = currentPortB ^ lastPortB; // Determine which bits have changed

    // Specifically check for changes in bits 5 and 6
    if (changedBits & (1 << 4)) // RB5 = rotate
    {
        if (currentPortB & (1 << 4))
        {
            if (curTet.type == L_PIECE)
            {
                RotateShape(&curTet.shape);
            }
        }
    }

    if (changedBits & (1 << 7)) // RB6 = submit
    {
        if (currentPortB & (1 << 7))
        {
            Submit();
        }
    }

    lastPortB = currentPortB; // Update last known state of Port B
}

void DisplayOn7Segment(const char num)
{
    char onesDigit;
    char tensDigit;

    onesDigit = num % 10;
    tensDigit = num / 10;

    // PORTH3 is connected to D0 on the 7-segment display
    // D0 is the rightmost 7-segment display. (Please check your board. This representation assumes I-O boards of the boards towards up)

    // Given these ports above, any data in PORTJ will be sent to 7-segment displays where their
    // enable bit is one. For example, if only D0’s bit is enabled (PORTH3 is one), only D0 will
    // receive the PORTJ’s value.  

    // Display the ones digit
    PORTJ = segmentLookup[onesDigit];

    // Enable the rightmost 7-segment display
    PORTH = 0x08;
    __delay_ms(1);

    // Display the tens digit
    PORTJ = segmentLookup[tensDigit];

    // Enable the leftmost 7-segment display
    PORTH = 0x04;
    __delay_ms(1);

    // Disable all 7-segment displays
    // PORTH = 0x00;  
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
