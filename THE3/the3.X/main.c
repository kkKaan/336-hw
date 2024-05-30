
#include <xc.h>
#include <stdint.h>
#include "pragmas.h"

inline void disable_rxtx( void ) { PIE1bits.RC1IE = 0;PIE1bits.TX1IE = 0;}
inline void enable_rxtx( void )  { PIE1bits.RC1IE = 1;PIE1bits.TX1IE = 1;}

/* **** Ring-buffers for incoming and outgoing data **** */
// These buffer functions are modularized to handle both the input and
// output buffers with an input argument.
typedef enum {INBUF = 0, OUTBUF = 1} buf_t;

#define BUFSIZE 128       /* Static buffer size. Maximum amount of data */
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
    if (head[buf] == tail[buf]) { /*error_overflow();*/ }
}
/* Retrieve data from buffer */
#pragma interrupt_level 2 // Prevents duplication of function
uint8_t buf_pop( buf_t buf ) {
    uint8_t v;
    if (buf_isempty(buf)) { 
        /*error_underflow();*/ return 0;
    } else {
        if (buf == INBUF) v = inbuf[tail[buf]];
        else v = outbuf[tail[buf]];
        tail[buf]++;
        if (tail[buf] == BUFSIZE) tail[buf] = 0;
        return v;
    }
}

void enable_portb();
void disable_portb();

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

volatile int remaining_distance = -1;
volatile int speed = 0;
volatile int should_send = 0;
volatile int alt_period = 0;
volatile int adc_val = 9000;
volatile int timer_counter = 1;
volatile int manual_on = 0;
volatile uint8_t last_portb = 0;
volatile int write_prs = 0;
volatile int prs_led = 0;
volatile int set_godone = 0;

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
    else {
        TXREG1 = buf_pop(OUTBUF);
    }
    while (TXSTA1bits.TRMT == 0);
}

void handle_timer() {
    INTCONbits.TMR0IF = 0;
       
    cmd1.type = DISTANCE;
    cmd1.value = remaining_distance;
       
    if (alt_period != 0) {
        if (timer_counter % alt_period == 0) {
            cmd1.type = ALTITUDE;
            cmd1.value = adc_val;
            GODONE = 1;
           
        }
    }
    
    if (manual_on && write_prs) {
        cmd1.type = PRESS;
        cmd1.value = prs_led;
        write_prs = 0;
    }
    
    if (should_send) {
        write_to_output(&cmd1);
    }

    timer_counter++;
    
    TMR0H = 0x85;
    TMR0L = 0xEE;
}

void handle_adc() {
    PIR1bits.ADIF = 0;
    
    unsigned int adcResult = (ADRESH << 8) + ADRESL;

    if (adcResult < 256) {
        adc_val = 9000;
    } else if (adcResult < 512) {
        adc_val = 10000;
    } else if (adcResult < 768) {
        adc_val = 11000;
    } else {
        adc_val = 12000;
    }
    
}

void handle_portb() {
    INTCONbits.RBIF = 0;
    //__delay_ms(2);
    char current_portb = PORTB; // Read the current state of Port B
    char changed_bits = current_portb ^ last_portb; // Determine which bits have changed

    // Specifically check for changes in bits 4, 5, 6, and 7
    if (changed_bits & (1 << 4)) { // RB4 
        if (current_portb & (1 << 4)) {
            prs_led = 4;
            write_prs = 1;
        }
    }

    if (changed_bits & (1 << 5)) { // RB5
        if (current_portb & (1 << 5)) {
            prs_led = 5;
            write_prs = 1;
        }
    }

    if (changed_bits & (1 << 6)) { // RB6
        if (current_portb & (1 << 6)) {
            prs_led = 6;
            write_prs = 1;
        }
    }

    if (changed_bits & (1 << 7)) { // RB7
        if (current_portb & (1 << 7)) {
            prs_led = 7;
            write_prs = 1;
        }
    }

    last_portb = current_portb; // Update last known state of Port B

}

void __interrupt(high_priority) highPriorityISR(void) {
    if (PIR1bits.RC1IF) receive_isr();
    if (PIR1bits.TX1IF) transmit_isr();
    if (PIR1bits.ADIF) handle_adc();
    if (INTCONbits.RBIF) handle_portb();
    if (INTCONbits.TMR0IF) handle_timer();
}
void __interrupt(low_priority) lowPriorityISR(void) {}

/* **** Initialization functions **** */
void init_ports() {
    TRISA = 0x00;
    TRISB = 0xF0;
    TRISC = 0xB0;
    TRISD = 0x00;

    PORTA = 0;
    PORTB = 0;
    PORTC = 0;
    PORTD = 0;
}

void init_serial() {
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
    INTCON = 0x00;
    enable_rxtx();
    INTCONbits.PEIE = 1;
    INTCONbits.TMR0IE = 1;
    disable_portb();
}

void init_timer() {
    T0CON = 0x00;

    T0CONbits.TMR0ON = 1;
    T0CONbits.T0CS = 0;
    T0CONbits.T08BIT = 0;
    T0CONbits.PSA = 0;

    T0CONbits.T0PS0 = 0;
    T0CONbits.T0PS1 = 0;
    T0CONbits.T0PS2 = 1;

    TMR0H = 0x85;
    TMR0L = 0xEE;
}

void init_adcon() {
    TRISH = 0x10;
    ADCON0 = 0x31;
    
    ADCON1 = 0x00;
    ADCON2 = 0x00;
    
    ADCON2bits.ADFM = 1;
    ADCON2 = 0xAA;
}

void enable_adc() {
    ADCON0bits.ADON = 1;
    PIE1bits.ADIE = 1;
    GODONE = 1;
}

void disable_adc() {
    ADCON0bits.ADON = 0;
    PIE1bits.ADIE = 0;
}

void enable_portb() {
    INTCONbits.RBIE = 1;
    INTCON2bits.RBIP = 1;
    //INTCONbits.INT0IE = 1;
    last_portb = PORTB;
}

void disable_portb() {
    INTCONbits.RBIF = 0;
    INTCONbits.RBIE = 0;
}

void start_system() { INTCONbits.GIE = 1; }

/* **** Packet task **** */
#define PKT_HEADER '$'  // Marker for start-of-packet
#define PKT_END '#'    // Marker for end-of-packet

// State machine states for packet reception. 
typedef enum {PKT_WAIT_HEADER, PKT_GET_BODY, PKT_WAIT_ACK} pkt_state_t;
pkt_state_t pkt_state = PKT_WAIT_HEADER;

uint8_t pkt_bodysize; // Size of the current packet

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

/*
 * Calculates the required amount of number characters for a given command
 * prefix.
 */
int cmd_len(const uint8_t* cmd_data, command_t* cmd) {
    if (string_compare_3(cmd_data, "GOO")) {
        cmd->type = GOO;
        return 4;
    }
    else if (string_compare_3(cmd_data, "END")) {
        cmd->type = END;
        return 0;
    }
    else if (string_compare_3(cmd_data, "SPD")) {
        cmd->type = SPEED;
        return 4;
    }
    else if (string_compare_3(cmd_data, "ALT")) {
        cmd->type = ALTITUDE;
        return 4;
    }
    else if (string_compare_3(cmd_data, "MAN")) {
        cmd->type = MANUAL;
        return 2;
    }
    else if (string_compare_3(cmd_data, "LED")) {
        cmd->type = LED;
        return 2;
    }
    else {
        cmd->type = UNDEFINED;
        return -1;
    }
}

/*
 * Executes the command logic.
 */
void process_cmd(const command_t* cmd) {
    switch(cmd->type) {
        case GOO:
            remaining_distance = cmd->value - speed;
            should_send = 1;
            break;
        case END:
            INTCONbits.GIE = 0;
            break;
        case SPEED:
            speed = cmd->value;
            remaining_distance -= speed;
            break;
        case ALTITUDE:
            alt_period = cmd->value / 100;
            timer_counter = 1;
            if (alt_period != 0) {
                enable_adc();
            } else {
                disable_adc();
            }
            break;
        case MANUAL:
            manual_on = cmd->value;
            if (manual_on) enable_portb();
            else disable_portb();
            break;
        case LED:
            switch (cmd->value) {
                case 0:
                    PORTA = 0; PORTB = 0; PORTC = 0; PORTD = 0;
                    break;
                case 1:
                    PORTD = 0x01;
                    break;
                case 2:
                    PORTC = 0x01;
                    break;
                case 3:
                    PORTB = 0x01;
                    break;
                case 4:
                    PORTA = 0x01;
                    break;
            }
            break;
        default:
            break;
    }
}

/*
 * Writes the command to the output buffer, which will later be sent via
 * serial communication protocols.
 */
void write_to_output(const command_t* cmd) {
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

    // buf_push('#', OUTBUF); // junk char
}

void packet_task() {
    disable_rxtx();
    uint8_t v;
    /*
     * The finite state machine has three states. The first state is waiting for
     * dollar sign. If it receives the dollar sign, then it receives the body.
     * After recognizing the character and getting enough characters to end the
     * input, it goes into acknowledge state, and processes the command by
     * executing its logic.
     */
    switch(pkt_state) {
    // wait for $
    case PKT_WAIT_HEADER:
        if (buf_isempty(INBUF)) break;
        v = buf_pop(INBUF);
        enable_rxtx();
        if (v == PKT_HEADER) {
            pkt_state = PKT_GET_BODY;
            pkt_bodysize = 0;
        }
        break;
    case PKT_GET_BODY:
        if (buf_isempty(INBUF)) break;
        v = buf_pop(INBUF);
        enable_rxtx();
        if (v == PKT_END) {
            if (pkt_bodysize != 3 + cmd_val_len) {
                /*error_packet();*/
                pkt_bodysize = 0;
                pkt_state = PKT_WAIT_HEADER;
                break;
            }
            pkt_state = PKT_WAIT_ACK;
        } else if (v == PKT_HEADER) {
            // Unexpected packet start. Abort current packet and restart
            /*error_packet();*/
            pkt_bodysize = 0;
        } else {
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
        enable_rxtx();
        if (cmd_val_len == 2) {
            sscanf(val_data, "%02x", &curr_cmd.value);
        } else if (cmd_val_len == 4) {
            sscanf(val_data, "%04x", &curr_cmd.value);
        }
        process_cmd(&curr_cmd);
        pkt_state = PKT_WAIT_HEADER;
        break;
    }
}

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

/*
void adc_task() {
    if (alt_period == 0) return;
    
    GODONE = 1;
    while (GODONE);
    unsigned int result = (ADRES << 8) + ADRESL;
    
    if (result < 256) {
        adc_val = 9000;
    } else if (result < 512) {
        adc_val = 10000;
    } else if (result < 768) {
        adc_val = 11000;
    } else {
        adc_val = 12000;
    }
}
 */

void main(void) {
    init_ports();
    init_serial();
    init_interrupts();
    init_timer();
    init_adcon();
    start_system();
    
    while(1) {
        //adc_task();
        packet_task();
        output_task();
    }

    return;
}