
#include <xc.h>
#include <stdint.h>
#include "pragmas.h"

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

typedef enum {
    GOO, // go defined before, use goo
    END,
    SPEED,
    ALTITUDE,
    MANUAL,
    LED,
    DISTANCE,
    PRESS,
    UNDEFINED
} command_type_t;

typedef struct {
    command_type_t type;
    int value;
} command_t;

command_t cmd1 = {.type = DISTANCE, .value = 125};

void write_to_output(const command_t* cmd);
/* **** ISR functions **** */
void receive_isr() {
    PIR1bits.RC1IF = 0;      // Acknowledge interrupt
    buf_push(RCREG1, INBUF); // Buffer incoming byte
}
void transmit_isr() {
    PIR1bits.TX1IF = 0;    // Acknowledge interrupt
    // If all bytes are transmitted, turn off transmission
    if (buf_isempty(OUTBUF)) {
        TXSTA1bits.TXEN = 0;
    }
    // Otherwise, send next byte
    else TXREG1 = buf_pop(OUTBUF);
}

void handle_timer() {
    INTCONbits.TMR0IF = 0;
    
    write_to_output(&cmd1);
    
    TMR0H = 0x85;
    TMR0L = 0xEE;
}

void __interrupt(high_priority) highPriorityISR(void) {
    if (PIR1bits.RC1IF) receive_isr();
    if (PIR1bits.TX1IF) transmit_isr();
    if (INTCONbits.TMR0IF) handle_timer();
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

void init_serial() {
    // We will configure EUSART1 for 57600 baud
    // SYNC = 0, BRGH = 0, BRG16 = 1. Simulator does not seem to work 
    // very well with BRGH=1
    
    TXSTA1bits.TX9 = 0;    // No 9th bit
    TXSTA1bits.TXEN = 0;   // Transmission is disabled for the time being
    TXSTA1bits.SYNC = 0; 
    TXSTA1bits.BRGH = 1;
    RCSTA1bits.SPEN = 1;   // Enable serial port
    RCSTA1bits.RX9 = 0;    // No 9th bit
    RCSTA1bits.CREN = 1;   // Continuous reception
    BAUDCON1bits.BRG16 = 0;
    
    SPBRGH1 = 0x00;
    SPBRG1 = 0x15; // 21
}

void init_interrupts() {
    // Enable reception and transmission interrupts
    enable_rxtx();
    INTCONbits.PEIE = 1;
    INTCONbits.TMR0IE = 1;
}

void init_timer() {
    T0CON = 0x00;
    
    T0CONbits.TMR0ON = 1;
    T0CONbits.T0CS = 0;
    T0CONbits.T08BIT = 0;
    T0CONbits.PSA = 0;
    
    //TODO: set scaler
    T0CONbits.T0PS0 = 0;
    T0CONbits.T0PS1 = 0;
    T0CONbits.T0PS2 = 1;
    
    TMR0H = 0x85;
    TMR0L = 0xEE;
}

void init_adcon() {
    
}

void start_system() { INTCONbits.GIE = 1; }

/* **** Packet task **** */
#define PKT_HEADER '$'  // Marker for start-of-packet
#define PKT_END '#'    // Marker for end-of-packet

// State machine states for packet reception. 
typedef enum {PKT_WAIT_HEADER, PKT_GET_BODY, PKT_WAIT_ACK} pkt_state_t;
pkt_state_t pkt_state = PKT_WAIT_HEADER;

uint8_t pkt_bodysize;           // Size of the current packet
// Set to 1 when packet has been received. Must be set to 0 once packet is processed
uint8_t pkt_valid = 0;
uint8_t pkt_id = 0; // Incremented for every valid packet reception

command_t curr_cmd;

uint8_t cmd_data[3] = {0};
uint8_t val_data[5] = {0};
uint8_t cmd_val_len = 4;

// returns 1 if equal, 0 otherwise
// compares first three characters only, no bounds check
int string_compare_3(const uint8_t* a, const uint8_t* b)
{
    for (int i = 0; i < 3; ++i)
    {
        if (a[i] != b[i])
        {
            return 0;
        }
    }
    return 1;
}

int cmd_len(const uint8_t* cmd_data, command_t* curr_cmd) {
    if (string_compare_3(cmd_data, "GOO")) {
        curr_cmd->type = GOO;
        return 4;
    }
    else if (string_compare_3(cmd_data, "END")) {
        curr_cmd->type = END;
        return 0;
    }
    else if (string_compare_3(cmd_data, "SPD")) {
        curr_cmd->type = SPEED;
        return 4;
    }
    else if (string_compare_3(cmd_data, "ALT")) {
        curr_cmd->type = ALTITUDE;
        return 4;
    }
    else if (string_compare_3(cmd_data, "MAN")) {
        curr_cmd->type = MANUAL;
        return 2;
    }
    else if (string_compare_3(cmd_data, "LED")) {
        curr_cmd->type = LED;
        return 2;
    }
    else {
        curr_cmd->type = UNDEFINED;
        return -1;
    }
}

void process_cmd(const command_t* curr_cmd) {
    switch(curr_cmd->type) {
        case GOO:
            // write command for go
            break;
        case END:
            break;
        case SPEED:
            break;
        case ALTITUDE:
            break;
        case MANUAL:
            break;
        case LED:
            break;
        default:
            break;
    }
}

void write_to_output(const command_t* cmd) {
    disable_rxtx();
    buf_push('$', OUTBUF);
    int i = cmd->value;
    char hex[5] = {0};
    switch (cmd->type) {
        case DISTANCE:
        {
            buf_push('D', OUTBUF);
            buf_push('S', OUTBUF);
            buf_push('T', OUTBUF);
            sprintf(hex, "%04x", cmd->value);
            for (int j = 0; j < 4; ++j) {
                buf_push(hex[j], OUTBUF);
            }
            break;
        }
        case ALTITUDE:
        {
            buf_push('A', OUTBUF);
            buf_push('L', OUTBUF);
            buf_push('T', OUTBUF);
            sprintf(hex, "%04x", cmd->value);
            for (int j = 0; j < 4; ++j) {
                buf_push(hex[j], OUTBUF);
            }
            break;
        }
        case PRESS:
        {
            buf_push('P', OUTBUF);
            buf_push('R', OUTBUF);
            buf_push('S', OUTBUF);
            sprintf(hex, "%02x", cmd->value);
            for (int j = 0; j < 2; ++j) {
                buf_push(hex[j], OUTBUF);
            }
            break;
        }
    }
    buf_push('#', OUTBUF);
    buf_push('#', OUTBUF); // junk char
    enable_rxtx();
}

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

        // wait for $
        case PKT_WAIT_HEADER:
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
                if (pkt_bodysize != 3 + cmd_val_len) {
                    error_packet();
                    pkt_bodysize = 0;
                    pkt_state = PKT_WAIT_HEADER;
                }
                pkt_state = PKT_WAIT_ACK;
                pkt_valid = 1;
            } else if (v == PKT_HEADER) {
                // Unexpected packet start. Abort current packet and restart
                error_packet();
                pkt_bodysize = 0;
            } else {
                // TODO: refactor
                if (pkt_bodysize < 3) {
                    cmd_data[pkt_bodysize] = v;
                } else if (pkt_bodysize == 3) {
                    cmd_val_len = cmd_len(cmd_data, &curr_cmd);
                    val_data[0] = v;
                } else if (pkt_bodysize < 3 + cmd_val_len) {
                    val_data[pkt_bodysize - 3] = v;
                } else {
                    // error
                }
                pkt_bodysize++;
            }

            break;
        case PKT_WAIT_ACK:
            sscanf(val_data, "%04x", &curr_cmd.value);
            process_cmd(&curr_cmd);
            pkt_state = PKT_WAIT_HEADER;
            pkt_id++;
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
/*
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
*/

/* ***** Serial output task **** */

/* Output the current packet contents, marking the current token location */
/*
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
 */
/* Output a string to the outgoing buffer */
//void output_str( char *str ) {
//    uint8_t ind = 0;    
//    while (str[ind] != 0) {
//        disable_rxtx();
//        buf_push(str[ind++], OUTBUF);
//        enable_rxtx();
//    }
//}
/* Output an integer to the outgoing buffer */
//void output_int( int32_t v ) {
//    
//    if (v < 0) { 
//        disable_rxtx(); buf_push('-', OUTBUF); enable_rxtx();
//        v = -v; 
//    }
//    char vstr[16];
//    uint8_t str_ptr = 0, m;
//    if (v == 0) vstr[str_ptr++] = '0';
//    else
//        while (v != 0) {
//            vstr[str_ptr++] = (v % 10)+'0';
//            v = v / 10;
//        }
//    while (str_ptr != 0) {
//        disable_rxtx(); buf_push(vstr[--str_ptr], OUTBUF); enable_rxtx();
//    }
//}



typedef enum {OUTPUT_INIT, OUTPUT_RUN} output_st_t;
output_st_t output_st = OUTPUT_INIT;
/* Output task function */
void output_task() {
    disable_rxtx();   
    if (!buf_isempty(OUTBUF) && TXSTA1bits.TXEN == 0) {
         // If transmission is already ongoing, do nothing, 
         // the ISR will send the next char. Otherwise, send the 
         // first char and enable transmission
         TXSTA1bits.TXEN = 1;
         TXREG1 = buf_pop(OUTBUF);
     }
     enable_rxtx();
}


void main(void) {
    init_ports();
    init_serial();
    init_interrupts();
    init_timer();
    init_adcon();
    start_system();
    
    while(1) {
        packet_task();
        output_task();
    }
    return;
}