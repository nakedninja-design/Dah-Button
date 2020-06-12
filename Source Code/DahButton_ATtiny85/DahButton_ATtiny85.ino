/*
 * Sleep Program for Dah Button ATtiny85
 * 
 * Company:    Naked Ninja (c) 2020
 * Website:    https://nakedninja.cc
 * Author(s):  Caner Erdem & Harm Verbeek
 * 
 * Pin configuration:
 *                                   +-\/-+ 
 *        PCINT5/RESET/ADC0/dW/PB5  1|    |8  Vcc 
 * PCINT3/XTAL1/CLKI/OC1B/ADC3/PB3  2|    |7  PB2/SCK/USCK/SCL/ADC1/T0/INT0/PCINT2
 * PCINT4/XTAL2/CLKO/OC1B/ADC2/PB4  3|    |6  PB1/MISO/DO/AIN1/OC0B/OC1A/PCINT1
 *                             GND  4|    |5  PB0/MOSI/DI/SDA/AIN0/OC0A/OC1A/AREF/PCINT0
 *                                   +----+
 * PB0  -(I)->  Wake up switch
 * PB1  -(O)->  Wake up switch state
 * PB2  -(O)->  Enable pin of voltage regulator
 * PB3  -(I)->  Wake up switch 2
 * PB4  -(I)->  Shutdown signal to disable voltage regulator
 * PB5  -(x)->  N/C
 * 
 * ATtiny85 upload settings:
 * (In order to program the ATtint85 an Arduino UNO with the ArduinoISP code is required.)
 *  Board:      Attiny25/45/85
 *  Processor:  Attiny85
 *  Clock:      Internal 1MHz
 *  Programmer: Arduino as ISP 
 *  
*/

//Library's
#include <avr/sleep.h>

// Pin configuration
#define WAKEUP_SWITCH         0
#define WAKEUP_SWITCH_STATE   1
#define VREG_ENABLE           2
#define WAKEUP2               3
#define VREG_SHTDN            4

// Delays & timers
const unsigned long timerInterval = 240;

// Timer variables
unsigned long start_time;

// Other variables
boolean goto_sleep = true;

ISR (PCINT0_vect) {}

void setup() {
  //Inputs
  pinMode(VREG_SHTDN, INPUT);
  pinMode(WAKEUP_SWITCH, INPUT);
  pinMode(WAKEUP2, INPUT);
  
  //Internal pull-ups
  digitalWrite(WAKEUP_SWITCH, HIGH);
  digitalWrite(WAKEUP2, HIGH);
  
  //Outputs
  pinMode(VREG_ENABLE, OUTPUT);
  pinMode(WAKEUP_SWITCH_STATE, OUTPUT);
  
  //Disable AD converter
  ADCSRA &= ~(1<<ADEN);
  
  //Set sleep mode
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  
  //Enable pin change interrupts 
  GIMSK |= (1<<PCIE);
}

void loop() {
  if (goto_sleep == true) {
    digitalWrite(VREG_ENABLE, LOW);         // Eisable voltage regulator
    digitalWrite(WAKEUP_SWITCH_STATE, LOW); // Eisable switch state output
   
    PCMSK |= ((1<<PCINT0) | (1<<PCINT3));   // Enable pin change interrupt on PB0(pin5) and PB3(pin2)
    sleep_enable();   // Ready to sleep
    sleep_mode();     // Sleep
    
    /*Returns here after wakeup !!!*/
  
    sleep_disable();  // Precaution
    PCMSK &= ~((1<<PCINT0) | (1<<PCINT3));  // Disable pin change interrupt
    
    if (digitalRead(WAKEUP2) == LOW) {
      digitalWrite(VREG_ENABLE, HIGH);          // Enanle voltage regulator
      digitalWrite(WAKEUP_SWITCH_STATE, HIGH);  // ESP8266 in server mode
      goto_sleep = false;
      start_time = millis();                  // Get new start timer
    } else if (digitalRead(WAKEUP_SWITCH) == LOW) {
      digitalWrite(VREG_ENABLE, HIGH);        // Enanle voltage regulator
      digitalWrite(WAKEUP_SWITCH_STATE, LOW); // ESP8266 in client mode
      goto_sleep = false;
      start_time = millis();                  // Get new start timer
    } else {
      goto_sleep = true;
    }    
  }

  // Attiny85 time out
  if ((goto_sleep == false) && (millis()-start_time >= timerInterval*1000)) {
    goto_sleep = true;
  }
  
  if (digitalRead(VREG_SHTDN) == HIGH) {
    delay(100);
//    while (digitalRead(VREG_SHTDN) == 1) {};
    goto_sleep = true;
  }
}
