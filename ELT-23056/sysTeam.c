// ELT 23056 Embedded Systems and Electronics Productization
// Project work - Electric organ
// Author: SysTeam
// ---------------------------------------------------------
// I/O lines:
// D2(PD2) = play button in
// D3(PD3) = rec button in
// D4(PD4) = stop button in
// A0,A4-A7(PC0, PC4, PC5, ADC6, ADC7) = buttons in (key1-key5)
// A3(PC3) = play led out
// A2(PC2) = rec led out
// D8-D12(PB0-PB4) = buttons out (key1-key5)

#include <avr/io.h>
#include <avr/interrupt.h>

#define MAX_SONG_LENGTH 1400

// Global variables
volatile uint8_t song[MAX_SONG_LENGTH];
volatile uint16_t songLength;
volatile uint8_t recording;
volatile uint8_t playing;
volatile uint16_t playCounter;
volatile uint8_t timer;


void initSystem() {
    // Initialize I/O lines
    // PD2-PD4 to input
    DDRD &= ~(0x1c);
    
    // PB0-PB4 to output
    DDRB |= 0x1f;
    
    // PC2, PC3 to output
    DDRC |= 0x0c;
    
    // Rec and play leds off
    PORTC &= ~(0x0c);
    
    // Initialize adc
    // Set ADC prescalar to 125KHz sample rate @ 16MHz
    ADCSRA |= (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);

    // Set ADC reference to AVCC
    ADMUX |= (1 << REFS0);
    // Left adjust ADC result
    ADMUX |= (1 << ADLAR);

    // Free-running mode selected (default value for ADCSRB)

    // Auto trigger conversion (needed to enable free-running mode)
    ADCSRA |= (1 << ADATE);
    
    // Initialize timer1
    // Timer normal port operation
    TCCR1A = 0;
    
    // Timer disabled
    TCCR1B = 0;
    
    // Timer overflow interrupt enable
    TIMSK1 = 0x01;
    
    // Generate interrupt request on any logical change with INT0 & INT1
    EICRA = 0x05;
    
    // Enable external interruption INT0 & INT1
    EIMSK = 0x03;
    
    // Initialize all necessary global variables
    songLength = 0;
    recording = 0;
    playing = 0;
    playCounter = 0;
    timer = 0;
    
    // Turn on interruptions
    sei();
}


void recordSelect() {
    if (playing) return;
    else if (!recording) {
        recording = 1;
        songLength = 0;
        
        // Rec led on
        PORTC |= 0x04;
        
        // Timer enabled with system clock and prescaled with clk/8
        // 16MHz system clock, 16-bit max value 65536, prescaler clk/8,
        // => roughly 30 overflows/sec, every other overflow used
        // => 15 samples/second
        TCCR1B = 0x02;
    }
    else {
        recording = 0;
        
        // Rec led off
        PORTC &= ~(0x04);
        
        // Timer disabled
        TCCR1B = 0;
    }
}


void playSelect() {
    if (recording) return;
    else if (!playing) {
        playing = 1;
        playCounter = 0;
        // Play led on
        PORTC |= 0x08;
        
        // Timer enabled with system clock and prescaled with clk/8
        // 16MHz system clock, 16-bit max value 65536, prescaler clk/8,
        // => roughly 30 overflows/sec, every other overflow used
        // => 15 samples/second
        TCCR1B = 0x02;
    }
    else {
        playing = 0;
        
        // Play led off
        PORTC &= ~(0x08);
        
        // Timer disabled
        TCCR1B = 0;
    }
}


uint8_t readADC(uint8_t ch) {
    // Select input channel for ADC
    if (ch == 0) {
        ADMUX = 0x60;
    }
    else if (ch == 4) {
        ADMUX = 0x64;
    }
    else if (ch == 5) {
        ADMUX = 0x65;
    }
    else if (ch == 6) {
        ADMUX = 0x66;
    }
    else if (ch == 7) {
        ADMUX = 0x67;
    }
    // Clear conversion ready flag
    ADCSRA |= (1<<ADIF);
    // Enable ADC
    ADCSRA |= (1 << ADEN);
    // Start conversion
    ADCSRA |= (1<<ADSC);
    // Wait for conversion to finish
    while(!(ADCSRA & (1 << ADIF)));
    // Disable ADC
    ADCSRA &= ~(1 << ADEN); 
    // Inspect the result and decide if the button is pressed
    if (ADCH < 50) {
        // Key not pressed
        return 1;
    }
    else {
        // Key pressed
        return 0;
    }
}


int main() {
    initSystem();
    while(1) {
        if (timer == 2) {
            timer = 0;
            if (recording) {
                // Stop recording if the max length has been reached
                if (songLength == MAX_SONG_LENGTH) {
                    recordSelect();
                }
                else {
                    // Store current key presses into an 8-bit integer
                    // Format: 0, 0, 0, key5, key4, key3, key2, key1
                    // 1 if key is pressed and 0 otherwise
                    uint8_t row = 0;
                    (readADC(0)) ? row += 0 : row += 1;
                    (readADC(4)) ? row += 0 : row += 2;
                    (readADC(5)) ? row += 0 : row += 4;
                    (readADC(6)) ? row += 0 : row += 8;
                    (readADC(7)) ? row += 0 : row += 16;
                    song[songLength] = row;
                    ++songLength;
                }
            }
            else if (playing) {
                if (songLength == 0) {
                  playSelect();
                }
                // Restart song when reaching the end
                else if (playCounter == songLength) {
                    playCounter = 0;
                }
                else {
                    // Send current song line key presses to the output pins
                    (song[playCounter] & (1 << 0)) ? PORTB |= 0x01 : PORTB &= ~(0x01);
                    (song[playCounter] & (1 << 1)) ? PORTB |= 0x02 : PORTB &= ~(0x02);
                    (song[playCounter] & (1 << 2)) ? PORTB |= 0x04 : PORTB &= ~(0x04);
                    (song[playCounter] & (1 << 3)) ? PORTB |= 0x08 : PORTB &= ~(0x08);
                    (song[playCounter] & (1 << 4)) ? PORTB |= 0x10 : PORTB &= ~(0x10);
                    ++playCounter;
                }
            }
        }
        if (!playing) {
            // Send current key presses to the output pins
            (readADC(0)) ? PORTB &= ~(0x01) : PORTB |= 0x01;
            (readADC(4)) ? PORTB &= ~(0x02) : PORTB |= 0x02;
            (readADC(5)) ? PORTB &= ~(0x04) : PORTB |= 0x04;
            (readADC(6)) ? PORTB &= ~(0x08) : PORTB |= 0x08;
            (readADC(7)) ? PORTB &= ~(0x10) : PORTB |= 0x10;
        }
        if ((PIND & (1 << PORTD4))) {
            if (playing) {
                playSelect();
            }
            else if (recording) {
                recordSelect();
            }
        }
    }
}


// Interruption for playing
ISR (INT0_vect) {
    if ((PIND & (1 << PORTD2)) == 0) {
        playSelect();
        while ((PIND & (1 << PORTD2)) == 1);
	}
}


// Interruption for recording
ISR (INT1_vect) {
    if ((PIND & (1 << PORTD3)) == 0) {
        recordSelect();
        while ((PIND & (1 << PORTD3)) == 1);
	}
}


// Timer overflow interruption used to record and play the song
ISR (TIMER1_OVF_vect) {;
    ++timer;
}

