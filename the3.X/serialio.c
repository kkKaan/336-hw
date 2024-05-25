/*
 * File:   serialio.c
 * Author: saranli
 * 
 * This file implements a simple serial IO example, which accepts message 
 * packets containing postfix expressions from the serial input with 
 * integers and arithmetic operators and outputs results from the serial output.
 * It illustrates proper buffering and processing principles.
 * 
 * Using this program with the simulator requires the following settings 
 * (Project Properties -> Simulator)
 * - (Oscillator options) Set the instruction frequency to 10MHz
 * - (UART1 IO Options) Enable, output to either window or file
 * 
 * Then, in Stimulus->Register Injection, you need to setup injection into 
 * the RCREG1 register with the following setup:
 * - Reg/Var: RCREG1
 * - Trigger: Message
 * - Data filename: calcinput.txt (or your own filename)
 * - Wrap: No (Yes if you want to continually process the same input)
 * - Format: Pkt
 * 
 * I would also recommend adding RB0,1,2,3,4,5 to the I/O pins monitor since 
 * these are used to signal various errors within the program
 * 
 * Input packet format:
 *  Our convention is such that packets start with 0x00 and end with 0xFF. 
 * In between, packets are expected to contain strings with the following
 * format:
 *    "C <infix expression>"
 * 
 * Infix expressions are a convenient and unambiguous way of writing 
 * arithmetic expressions, wherein operands appear first and operators 
 * afterwards. So, the arithmetic expression
 *  (((1+2)-3)*(4+5))/6
 * would be written as
 *   1 2 + 3 - 4 5 + * 6 /
 * 
 * Infix expressions are best understood and implemented with a stack data
 * structure. When a number is encountered, it is pushed onto the stack. When
 * an operator is encountered, two operands are popped from the stack, the
 * result computed and pushed onto the stack.
 * 
 * This implementation only supports positive integer literals as operands
 * to simplify parsing, but that's not a problem since negative numbers can 
 * always be expressed as "0 13 -", which will yield -13 on the stack.
 * 
 * Variables:
 *  Within an infix expression, if a token in the form "Sx" is encountered,
 * this defines a variable with the name 'x', and the value popped from
 * the top of the stack. 'x' can later be used instead of a number,
 * which is then searched in the database of previously defined variables
 * and pushed onto the stack. This allows saving and reuse of previous 
 * results from the computation. For example, the arithmetic expression
 * ((1+3)*5)^3 can be implemented with the following expression (despite 
 * the absence of support for the exponent operator) as
 * "1 3 + 5 * Sx x x * x *"
 * 
 * Throughout execution, various PORTB pins are used to signal errors
 * RB0 : Input/Output buffer overflow
 * RB1 : Input/Output buffer underflow
 * RB2 : Packet format error
 * RB3 : Syntax error
 * RB4 : Stack error (underflow or overflow)
 * RB5 : Variable error. Invalid name or undefined variable
 * 
 * For reference, here is the BNF grammar for the supported infix expressions
 * 
 * <calc>       -> <calc_cmd> <infix_expr>
 * <calc_cmd>   -> 'C' | 'c'
 * <infix_expr> -> <infix_term> <infix_expr> 
 * <infix_term> -> <posint>         # Positive integer literals
 *                  | <varname>     # Single letter lowercase variables names
 *                  | <operator>   
 * <operator>   -> '+' | '-' | '*' | '/' # Arithmetic operators  
 *                  | ('S' <varname>)    # S and <varname> are concatenated
 * 
 * Created on May 15, 2022, 3:16 PM
 */


#include <xc.h>
#include <stdint.h>

// These are used to disable/enable UART interrupts before/after
// buffering functions are called from the main thread. This prevents
// critical race conditions that result in corrupt buffer data and hence
// incorrect processing
inline void disable_rxtx( void ) { PIE1bits.RC1IE = 0;PIE1bits.TX1IE = 0;}
inline void enable_rxtx( void )  { PIE1bits.RC1IE = 1;PIE1bits.TX1IE = 1;}

/* **** Error outputs and functions **** */
char * err_str = 0;
// Prevents the compiler from duplicating the subsequently function
// which is done when the function is called from both the main and ISR
// contexts
#pragma interrupt_level 2 
void error_overflow()  { PORTB |= 0x01; err_str = "IO buffer overflow!";}
#pragma interrupt_level 2 // Prevents duplication of function
void error_underflow() { PORTB |= 0x02; err_str = "IO buffer underflow!";}
void error_packet()    { PORTB |= 0x04; err_str = "Packet format error!";}
void error_syntax()    { PORTB |= 0x08; err_str = "Syntax error!";}
void error_stack()     { PORTB |= 0x10; err_str = "Stack error!";}
void error_variable()  { PORTB |= 0x20; err_str = "Variable error!";}
void error_clear()     { PORTB &= 0xC0; err_str = 0; }

/* **** Ring-buffers for incoming and outgoing data **** */
// These buffer functions are modularized to handle both the input and
// output buffers with an input argument.
typedef enum {INBUF = 0, OUTBUF = 1} buf_t;

#define BUFSIZE 128        /* Static buffer size. Maximum amount of data */
uint8_t inbuf[BUFSIZE];   /* Preallocated buffer for incoming data */
uint8_t outbuf[BUFSIZE];  /* Preallocated buffer for outgoing data  */
uint8_t head[2] = {0, 0}; /* head for pushing, tail for popping */
uint8_t tail[2] = {0, 0};

/* Check if a buffer had data or not */
#pragma interrupt_level 2 // Prevents duplication of function
uint8_t buf_isempty( buf_t buf ) { return (head[buf] == tail[buf])?1:0; }
/* Place new data in buffer */
#pragma interrupt_level 2 // Prevents duplication of function
void buf_push( uint8_t v, buf_t buf) {
    if (buf == INBUF) inbuf[head[buf]] = v;
    else outbuf[head[buf]] = v;
    head[buf]++;
    if (head[buf] == BUFSIZE) head[buf] = 0;
    if (head[buf] == tail[buf]) { error_overflow(); }
}
/* Retrieve data from buffer */
#pragma interrupt_level 2 // Prevents duplication of function
uint8_t buf_pop( buf_t buf ) {
    uint8_t v;
    if (buf_isempty(buf)) { 
        error_underflow(); return 0; 
    } else {
        if (buf == INBUF) v = inbuf[tail[buf]];
        else v = outbuf[tail[buf]];
        tail[buf]++;
        if (tail[buf] == BUFSIZE) tail[buf] = 0;
        return v;
    }
}

/* **** ISR functions **** */
void receive_isr() {
    PIR1bits.RC1IF = 0;      // Acknowledge interrupt
    buf_push(RCREG1, INBUF); // Buffer incoming byte
}
void transmit_isr() {
    PIR1bits.TX1IF = 0;    // Acknowledge interrupt
    // If all bytes are transmitted, turn off transmission
    if (buf_isempty(OUTBUF)) TXSTA1bits.TXEN = 0; 
    // Otherwise, send next byte
    else TXREG1 = buf_pop(OUTBUF);
}

void __interrupt(high_priority) highPriorityISR(void) {
    if (PIR1bits.RC1IF) receive_isr();
    if (PIR1bits.TX1IF) transmit_isr();
}
void __interrupt(low_priority) lowPriorityISR(void) {}

/* **** Initialization functions **** */
void init_ports() {
     //Port B pin 0: buffer overflow, pin1: buffer underflow, pin2: syntax err
    TRISB = 0xC0;
    PORTB = 0x00;
    // PORTC pin 7 is input, pin 6 is output, rest is input
    TRISC = 0xBF;
}

// Choose SPBRG from Table 20.3
#define SPBRG_VAL (42)
void init_serial() {
    // We will configure EUSART1 for 57600 baud
    // SYNC = 0, BRGH = 0, BRG16 = 1. Simulator does not seem to work 
    // very well with BRGH=1
    
    TXSTA1bits.TX9 = 0;    // No 9th bit
    TXSTA1bits.TXEN = 0;   // Transmission is disabled for the time being
    TXSTA1bits.SYNC = 0; 
    TXSTA1bits.BRGH = 0;
    RCSTA1bits.SPEN = 1;   // Enable serial port
    RCSTA1bits.RX9 = 0;    // No 9th bit
    RCSTA1bits.CREN = 1;   // Continuous reception
    BAUDCON1bits.BRG16 = 1;

    SPBRGH1 = (SPBRG_VAL >> 8) & 0xff;
    SPBRG1 = SPBRG_VAL & 0xff;
}

void init_interrupts() {
    // Enable reception and transmission interrupts
    enable_rxtx();
    INTCONbits.PEIE = 1;
}

void start_system() { INTCONbits.GIE = 1; }

/* **** Packet task **** */
#define PKT_MAX_SIZE 128 // Maximum packet size. Syntax errors can be large!
#define PKT_HEADER 0x00  // Marker for start-of-packet
#define PKT_END 0xff     // Marker for end-of-packet

// State machine states for packet reception. 
typedef enum {PKT_WAIT_HDR, PKT_GET_BODY, PKT_WAIT_ACK} pkt_state_t;
pkt_state_t pkt_state = PKT_WAIT_HDR;
uint8_t pkt_body[PKT_MAX_SIZE]; // Packet data
uint8_t pkt_bodysize;           // Size of the current packet
// Set to 1 when packet has been received. Must be set to 0 once packet is processed
uint8_t pkt_valid = 0;
uint8_t pkt_id = 0; // Incremented for every valid packet reception

/* The packet task is responsible from monitoring the input buffer, identify
 * the start marker 0x00, and retrieve all subsequent bytes until the end marker
 * 0xff is encountered. This packet will then be processed by the calc_task()
 * to parse and execute the arithmetic expression. */
void packet_task() {
    disable_rxtx();
    // Wait until new bytes arrive
    if (!buf_isempty(INBUF)) {
        uint8_t v;
        switch(pkt_state) {
        case PKT_WAIT_HDR:
            v = buf_pop(INBUF);
            if (v == PKT_HEADER) {
                // Packet header is encountered, retrieve the rest of the packet
                pkt_state = PKT_GET_BODY;
                pkt_bodysize = 0;
            }
            break;
        case PKT_GET_BODY:
            v = buf_pop(INBUF);
            if (v == PKT_END) {
                // End of packet is encountered, signal calc_task())
                pkt_state = PKT_WAIT_ACK;
                pkt_valid = 1;
            } else if (v == PKT_HEADER) {
                // Unexpected packet start. Abort current packet and restart
                error_packet();
                pkt_bodysize = 0;
            } else 
                pkt_body[pkt_bodysize++] = v;
            break;
        case PKT_WAIT_ACK:
            if (pkt_valid == 0) {
                // Packet processing seems to be finished, continue monitoring
                pkt_state = PKT_WAIT_HDR;
                pkt_id++;
            }
            break;
        }
    }
    enable_rxtx();
}

/* **** Tokenizer tools **** */

/* "Tokens" are strings of non-whitespace characters separated by whitespaces.
 * They are essential for parsing packet contents, which consist of an initial
 * command character, followed by whitespace-separated (positive) integers and
 * arithmetic operators. */
uint8_t tk_start; // Start index for the token
uint8_t tk_size;  // Size of the token (0 if no token found)

// Resets the tokenizer to start at the beginning pf packet
void tk_reset() {
    tk_start = 0;
    tk_size = 0;
}
// Finds the next token in the packet data body
void tk_next() {
    if (tk_start > pkt_bodysize) return; // Return if no more data
    tk_start = tk_start + tk_size; // Adjust starting location
    tk_size = 0;
    // Skip trailing whitespace, return if no data left in packet
    while (pkt_body[tk_start]==' ' && tk_start < pkt_bodysize) tk_start++;
    if (tk_start > pkt_bodysize) return;
    // Search for the next whitespace or end of packet
    while (pkt_body[tk_start+tk_size]!=' ' 
            && tk_start+tk_size<pkt_bodysize) tk_size++;
}

/* ***** Serial output task **** */

/* Output the current packet contents, marking the current token location */
void output_packet( void ) {
    uint8_t ind = 0;
    while (ind < pkt_bodysize) {
        disable_rxtx();
        if (ind == tk_start) buf_push('>', OUTBUF);
        else if (ind == tk_start+tk_size) buf_push('<', OUTBUF);
        buf_push(pkt_body[ind++], OUTBUF);
        enable_rxtx();
    }
}
/* Output a string to the outgoing buffer */
void output_str( char *str ) {
    uint8_t ind = 0;    
    while (str[ind] != 0) {
        disable_rxtx();
        buf_push(str[ind++], OUTBUF);
        enable_rxtx();
    }
}
/* Output an integer to the outgoing buffer */
void output_int( int32_t v ) {
    
    if (v < 0) { 
        disable_rxtx(); buf_push('-', OUTBUF); enable_rxtx();
        v = -v; 
    }
    char vstr[16];
    uint8_t str_ptr = 0, m;
    if (v == 0) vstr[str_ptr++] = '0';
    else
        while (v != 0) {
            vstr[str_ptr++] = (v % 10)+'0';
            v = v / 10;
        }
    while (str_ptr != 0) {
        disable_rxtx(); buf_push(vstr[--str_ptr], OUTBUF); enable_rxtx();
    }
}

typedef enum {OUTPUT_INIT, OUTPUT_RUN} output_st_t;
output_st_t output_st = OUTPUT_INIT;
/* Output task function */
void output_task() {
    switch (output_st) {
    case OUTPUT_INIT:
        output_str("*** CENG 336 Serial Calculator V1 ***\n");
        output_st = OUTPUT_RUN;
        break;
    case OUTPUT_RUN:
        disable_rxtx();
        // Check if there is any buffered output or ongoing transmission
        if (!buf_isempty(OUTBUF)&& TXSTA1bits.TXEN == 0) { 
            // If transmission is already ongoing, do nothing, 
            // the ISR will send the next char. Otherwise, send the 
            // first char and enable transmission
            TXSTA1bits.TXEN = 1;
            TXREG1 = buf_pop(OUTBUF);
        }
        enable_rxtx();
        break;
    }
}

/* **** Calculator task and tools **** */

/* *** Calculator stack *** */
#define MAX_STACK_DEPTH 16
int32_t calc_stack[MAX_STACK_DEPTH];
int8_t calc_stack_ptr = 0;
/* Flush all data in stack */
void calc_stack_reset() { calc_stack_ptr = 0; }
/* Push new data onto the top of the stack */
void calc_stack_push(int32_t v) { 
    if (calc_stack_ptr == MAX_STACK_DEPTH) {error_stack(); return;} 
    calc_stack[calc_stack_ptr++] = v;
}
/* Pop data from the top of the stack */
int32_t calc_stack_pop() { 
    if (calc_stack_ptr == 0) {error_stack(); return 0;}
    return calc_stack[--calc_stack_ptr];
}

/* *** Variable database and functions *** */
#define MAX_VARIABLE_COUNT 16
char calc_variables[MAX_VARIABLE_COUNT];
int32_t calc_values[MAX_VARIABLE_COUNT];
int8_t calc_var_count = 0;

// Finds the index of the named variable. Returns -1 if not found
int8_t calc_find_variable(char name) {
    int8_t ind;
    for (ind = 0; ind < calc_var_count; ind++)
        if (calc_variables[ind] == name) return ind;
    return -1;
}
// Sets the named variable to the supplied value
void calc_set_variable(char name, int32_t val) {
    if (calc_var_count == MAX_VARIABLE_COUNT) {
        error_variable(); return;
    }
    int8_t ind = calc_find_variable(name);
    if (ind == -1) ind = calc_var_count++;
    calc_variables[ind] = name;
    calc_values[ind] = val;
}
// Finds and returns the value of the named variable. Returns 0 if not found
int32_t calc_get_variable(char name) {
    int8_t ind = calc_find_variable(name);
    if (ind < 0) return 0;
    else return calc_values[ind];
}

/* Calculator task function */
void calc_task() {
    if (pkt_valid == 0) return; // Wait until there is a valid packet to process

    // Reset calculator stack and tokenizer
    calc_stack_reset();
    tk_reset();
    
    // Retrieve first token
    tk_next();

    // ***** Process command token
    uint8_t cmd = pkt_body[tk_start];
    // Validate the command, issue syntax error if invalid
    if (tk_size != 1 || (cmd != 'C' && cmd != 'c') ) goto syntax_error;

    // ***** Process command parameter tokens
    uint8_t ch, ptr;
    int32_t v1, v2;
    switch (cmd) {
        case 'C':
        case 'c':
            // "COMPUTE" command
            // Go through and process all additional tokens
            while (1) {
                // Retrieve next token
                tk_next(); if (tk_size == 0) break;
                ptr = tk_start;
                ch = pkt_body[ptr++];
                if (ch >= '0' && ch <= '9') {
                    // Numerical token. Extract positive integer and push
                    int32_t val = (int8_t) (ch - '0');
                    while (ptr < tk_start+tk_size) {
                        ch = pkt_body[ptr++];
                        if (ch < '0' || ch > '9') goto syntax_error;
                        val = val*10 + ((int8_t) (ch - '0'));
                    }
                    calc_stack_push(val);
                } else if ((ch == '+' || ch == '-' || ch == '*' || ch == '/')
                        && tk_size == 1) {
                    // Operator tokens, check stack, pop 2 elements
                    if (calc_stack_ptr < 2) goto stack_error;
                    v1 = calc_stack_pop();
                    v2 = calc_stack_pop();
                    switch (ch) {
                        case '+':
                            calc_stack_push(v2+v1); break;
                        case '-':
                            calc_stack_push(v2-v1); break;
                        case '*':
                            calc_stack_push(v2*v1); break;
                        case '/':
                            if (v1 != 0) calc_stack_push(v2/v1);
                            else // Division by zero!
                                goto syntax_error;
                    }
                } else if (tk_size == 2 && (ch == 'S' || ch == 's')) {
                    // Set variable to value
                    if (calc_stack_ptr < 1) goto stack_error;
                    char varname = pkt_body[ptr++];
                    if (varname >= 'a' && varname <= 'z') {
                        // Valid lowercase variable name, pop value and set                        
                        calc_set_variable( varname, calc_stack_pop());
                    } else goto syntax_error;
                } else if (tk_size == 1 && ch >= 'a' && ch <= 'z') {
                    int8_t ind = calc_find_variable(ch);
                    if (ind < 0) goto variable_error;
                    calc_stack_push(calc_values[ind]);
                } else 
                    // Unrecognized token
                    goto syntax_error;                      
            }
            // Finished with packet without any errors
            pkt_valid = 0;
            goto generate_output;
    }
    return;
syntax_error: // Common entry for all syntax errors
            error_syntax();
            pkt_valid = 0;
            goto generate_output;
stack_error: // Common entry point for all stack errors.
            error_stack();
            pkt_valid = 0;
            goto generate_output;

variable_error: // Common entry point for all variable errors.
            error_variable();
            pkt_valid = 0;
            goto generate_output;

generate_output: // Common entry point for output generation
            output_str("p");
            output_int(pkt_id);
            output_str(".");
            if (err_str != 0) {
                output_str(err_str);
                if (PORTBbits.RB3 || PORTBbits.RB5){
                    // syntax or variable error. Output offending token
                    output_str(", \"");
                    output_packet();
                    output_str("\"");
                }
            } else {
                // Output stack contents
                output_str("st: (t) ");
                while (calc_stack_ptr != 0) {
                    output_int(calc_stack_pop());
                    output_str(" ");
                }
                output_str("(b)");
            }
            output_str("\n");
            error_clear();
            return;
}

void main(void) {
    init_ports();
    init_serial();
    init_interrupts();
    start_system();
    
    while(1) {
        packet_task();
        output_task();
        calc_task();
    }
    return;
}

