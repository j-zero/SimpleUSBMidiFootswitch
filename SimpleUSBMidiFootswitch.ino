#include "MIDIUSB.h" // Download it here: https://github.com/arduino-libraries/MIDIUSB
#include "Keyboard.h"
#include "Mouse.h"

#include <EEPROM.h>

#define LED_1 8
#define LED_2 9

#define MAX_MODE 1

const int NButtons = 4; 
const int buttonPin[NButtons] = {2, 3, 4, 5}; 

                      // BCKSPC 
int letter[NButtons] = {KEY_BACKSPACE,  ' ', KEY_LEFT_ARROW, KEY_RIGHT_ARROW}; // key codes
int buttonCState[NButtons] = {0,0,0,0};
int buttonPState[NButtons] = {0,0,0,0};

unsigned long lastDebounceTime[NButtons] = {0,0,0,0};
unsigned long debounceDelay = 13;

/////////////////////////////////////////////
// potentiometers

const int NPots = 2; //*
int potPin[NPots] = {A0, A1}; //* Pin where the potentiometer is
int potCState[NPots] = {0,0}; // Current state of the pot
int potPState[NPots] = {0,0}; // Previous state of the pot
int potVar = 0; // Difference between the current and previous state of the pot

int midiCState[NPots] = {0,0}; // Current state of the midi value
int midiPState[NPots] = {0,0}; // Previous state of the midi value

int TIMEOUT = 300; //* Amount of time the potentiometer will be read after it exceeds the varThreshold
int varThreshold = 10; //* Threshold for the potentiometer signal variation
boolean potMoving = true; // If the potentiometer is moving
unsigned long PTime[NPots] = {0,0}; // Previously stored time
unsigned long timer[NPots] = {0,0}; // Stores the time that has elapsed since the timer was reset

/////////////////////////////////////////////

const byte midiCh = 0; //* MIDI channel to be used
const byte note = 1; //* Lowest note to be used
const byte notes[NButtons] = {1, 2, 3, 4};
const byte cc = 1; //* Lowest MIDI CC to be used

const byte prog = 0; //* MIDI channel to be used



int cur_bank = 0;
byte cur_mode = 0; // 0 = MIDI, 1 = Keyboard, 2 = Mouse


void setup() {

  cur_mode = EEPROM.read(0);
  Keyboard.begin();
 

  pinMode(LED_1, OUTPUT);
  pinMode(LED_2, OUTPUT);

  LEDGreen();
  delay(250);
  LEDRed();
  delay(250);
  LEDOff();
  delay(250);

  LEDGreen();


  for (int i = 0; i < NButtons; i++) {
    pinMode(buttonPin[i], INPUT_PULLUP);
  }


}

void loop() {

  buttons();
  potentiometers();
  modes();

}

void modes() {
  switch(cur_mode){
    case 0: // MIDI
      LEDGreen();
      break;
    case 1: // Keyboard
      LEDRed();
      break;  
    case 2:
    default:

      break;
  }
}

/////////////////////////////////////////////
// BUTTONS
void buttons() {

  for (int i = 0; i < NButtons; i++) {

    buttonCState[i] = digitalRead(buttonPin[i]);
    if ((millis() - lastDebounceTime[i]) > debounceDelay) {

      if (buttonPState[i] != buttonCState[i]) {
        lastDebounceTime[i] = millis();

        if(buttonCState[0] == LOW && buttonCState[2] == LOW && buttonCState[3] == LOW && buttonCState[4] == LOW){
          if(cur_mode < MAX_MODE)
            cur_mode++;
          else
            cur_mode = 0;
          Keyboard.releaseAll();


          while(buttonCState[0] == HIGH && buttonCState[2] == HIGH && buttonCState[3] == HIGH && buttonCState[4] == HIGH)
            delay(50);

          EEPROM.write(0, cur_mode);
        }

        else if (buttonCState[i] == LOW) {     // Button Pressed

          if(cur_mode == 0){ // MIDI
            noteOn(midiCh, notes[i]+(cur_bank * NButtons), 127); 
            MidiUSB.flush();
          }
          else if(cur_mode == 1){ // Keyboard
            Keyboard.press(letter[i]);
          }
          else if(cur_mode == 2){ // Mouse
              Mouse.move(0, 0, -1);
            }
        }
        else {                            // Button Released
          if(i < NButtons){
            if(cur_mode == 0){ // MIDI
              noteOff(midiCh, notes[i]+(cur_bank * NButtons), 0);
              MidiUSB.flush();
            }
            else if(cur_mode == 1){ // Keyboard
              Keyboard.release(letter[i]);
            }
            else if(cur_mode == 2){ // Keyboard
              //Keyboard.release(letter[i]);
            }
          }

        }
        buttonPState[i] = buttonCState[i];
      }
    }
  }
}

/////////////////////////////////////////////
// POTENTIOMETERS
void potentiometers() {

  for (int i = 0; i < NPots; i++) { // Loops through all the potentiometers

    potCState[i] = analogRead(potPin[i]); // Reads the pot and stores it in the potCState variable
    midiCState[i] = map(potCState[i], 0, 1023, 0, 127); // Maps the reading of the potCState to a value usable in midi


    potVar = abs(potCState[i] - potPState[i]); // Calculates the absolute value between the difference between the current and previous state of the pot

    if (potVar > varThreshold) { // Opens the gate if the potentiometer variation is greater than the threshold
      PTime[i] = millis(); // Stores the previous time
    }

    timer[i] = millis() - PTime[i]; // Resets the timer 11000 - 11000 = 0ms

    if (timer[i] < TIMEOUT) { // If the timer is less than the maximum allowed time it means that the potentiometer is still moving
      potMoving = true;
    }
    else {
      potMoving = false;
    }

    if (potMoving == true) { // If the potentiometer is still moving, send the change control
      if (midiPState[i] != midiCState[i]) {

        // use if using with ATmega328 (uno, mega, nano...)
        //MIDI.sendControlChange(cc+i, midiCState[i], midiCh);

        // use if using with ATmega32U4 (micro, pro micro, leonardo...)
        controlChange(midiCh, cc + i, midiCState[i]); // manda control change (channel, CC, value)
        MidiUSB.flush();


        //Serial.println(midiCState);
        potPState[i] = potCState[i]; // Stores the current reading of the potentiometer to compare with the next
        midiPState[i] = midiCState[i];
      }
    }
  }

}


void noteOn(byte channel, byte pitch, byte velocity) {
  midiEventPacket_t noteOn = {0x09, 0x90 | channel, pitch, velocity};
  MidiUSB.sendMIDI(noteOn);
}

void noteOff(byte channel, byte pitch, byte velocity) {
  midiEventPacket_t noteOff = {0x08, 0x80 | channel, pitch, velocity};
  MidiUSB.sendMIDI(noteOff);
}

void controlChange(byte channel, byte control, byte value) {
  midiEventPacket_t event = {0x0B, 0xB0 | channel, control, value};
  MidiUSB.sendMIDI(event);
}

void programChange(byte channel, byte program) {
  midiEventPacket_t pc = {0x0C, 0xC0 | channel, program, 0};
  MidiUSB.sendMIDI(pc);
}


void LEDGreen(){
  digitalWrite(LED_1, HIGH);
  digitalWrite(LED_2, LOW);
}
void LEDRed(){
  digitalWrite(LED_1, LOW);
  digitalWrite(LED_2, HIGH);
}

void LEDOff(){
  digitalWrite(LED_1, LOW);
  digitalWrite(LED_2, LOW);
}



