/*
Project: CanFiller
Created: Primoz Flander 8.9.2019

v5: deleted flowrate, flow volume step decreased to 1.

Arduino pin   /      I/O:
DI2           ->     Water flow meter (INT0)
DO3           ->     LCD D7
DO4           ->     LCD D6
DO5           ->     LCD D5
DO6           ->     LCD D4
DI7           ->     IR receiver
DO8           ->     Relay
DO9           ->     US Reserved
DO10          ->     US Reserved
DO11          ->     LCD Enable
DO12          ->     LCD RS
*/


/*=======================================================================================
                                    Includes
========================================================================================*/

#include <LiquidCrystal.h>
#include <EEPROM.h>
#include <IRremote.h>

/*=======================================================================================
                                    Definitions
========================================================================================*/

#define FLOWSENSORPIN 2
#define relay         8

/*=======================================================================================
                                User Configurarations
========================================================================================*/

LiquidCrystal lcd(12, 11, 6, 5, 4, 3);  // initialize the library with the numbers of the interface pins
IRrecv irrecv(7);
decode_results results;

int address = 1;                        // EEPROM address
int pMode = 0;
int timerVal = 0;
int flowVol = 0;
int heightHigh = 0;
int heightLow = 0;
int timeHigh = 0;
int timeLow = 0;
int loopTimeVal = 0;                        // delay loop
int loopFlowVal = 0;                        // delay loop
bool socketStat = false;                    // socket status ON/OFF
volatile uint16_t pulses = 0;               // count how many pulses
volatile uint8_t lastflowpinstate;          // track the state of the pulse pin
volatile uint32_t lastflowratetimer = 0;    // keep time of how long it is between pulses
volatile float flowrate;                    // to calculate a flow rate

SIGNAL(TIMER0_COMPA_vect) {
  uint8_t x = digitalRead(FLOWSENSORPIN);
  if (x == lastflowpinstate) {
    lastflowratetimer++;
    return; // nothing changed!
  } 
  if (x == HIGH) {
    pulses++; //low to high transition!
  }
  lastflowpinstate = x;
  flowrate = 1000.0;
  flowrate /= lastflowratetimer;  // in hertz
  lastflowratetimer = 0;
}

void useInterrupt(boolean v) {
  if (v) {
    OCR0A = 0xAF;
    TIMSK0 |= _BV(OCIE0A);
  } else {
    TIMSK0 &= ~_BV(OCIE0A);
  }
}

/*=======================================================================================
                                   Setup function
========================================================================================*/
  
void setup() {

  Serial.begin(115200);                     // enable serial
  Serial.println("Init starting...");
  irrecv.enableIRIn();                      // start the receiver
  pinMode(LED_BUILTIN, OUTPUT);             // onboard LED as output
  digitalWrite(LED_BUILTIN, LOW);
  pinMode(FLOWSENSORPIN, INPUT);
  pinMode(relay, OUTPUT);                       // relay
  digitalWrite(relay, HIGH);
  digitalWrite(FLOWSENSORPIN, HIGH);
  lastflowpinstate = digitalRead(FLOWSENSORPIN);
  useInterrupt(true);
  
  lcd.begin(16, 2);  //enable LCD display
  Serial.println("LCD display OK");
  versionDelay();
  lcd.clear();
  
  EEPROM.get(address,timerVal);
  EEPROM.get(address+10,pMode);
  EEPROM.get(address+20,flowVol);
  
  Serial.print("EEPROM timerVal=:");
  Serial.println(timerVal);
  Serial.print("EEPROM flowVol=:");
  Serial.println(flowVol);
  Serial.print("EEPROM mode=:");
  Serial.println(pMode);

  switch (pMode) {
    case 1:
      mode1();
      break;
    default:
      mode0();
    break;
  }

  socketStat = false;
  lcd.setCursor(0,1);
  lcd.print("Socket OFF      ");
  Serial.println("Init complete");
}

/*=======================================================================================
                                            Loop
========================================================================================*/

void loop() {

  if (irrecv.decode(&results)) {

      Serial.print("Received:");
      Serial.println(results.value);
      
      //Remote button next (ON)
      if (results.value == 1120 || results.value == 3168 || results.value == 1312 || results.value == 3360)  {

        if (pMode == 0) {
          digitalWrite(relay, LOW);
          digitalWrite(LED_BUILTIN, HIGH);
          Serial.println("Socket ON");
          lcd.setCursor(0,1);
          lcd.print("Socket ON       ");
          loopTimeVal = timerVal;
          socketStat = true;
        }
        
        else if (pMode == 1) {
          digitalWrite(relay, LOW);
          digitalWrite(LED_BUILTIN, HIGH);
          Serial.println("Socket ON");
          lcd.setCursor(0,1);
          //lcd.print("Socket ON       ");
          loopFlowVal = 1;
          pulses = 0;
          socketStat = true;
        }
      }

      //Remote button prev. (OFF)
      else if (results.value == 1121 || results.value == 3169 || results.value == 3361 || results.value == 1313)  {

        if (pMode == 0 || pMode == 1 || pMode == 2) {
          digitalWrite(relay, HIGH);
          digitalWrite(LED_BUILTIN, LOW);
          Serial.println("Socket OFF");
          lcd.setCursor(0,1);
          lcd.println("Socket OFF      ");
          loopTimeVal = 0;
          loopFlowVal = 0;
          socketStat = false;
        }
      }
        
      //Remote button volume up
      else if (results.value == 1040 || results.value == 3088)  {
          
        if (pMode == 0) {
          timerVal++;
          
          if (timerVal >= 32000)  {
            timerVal = 32000;
            Serial.println("Timer value +limit");
          }
          lcd.setCursor(6,0);
          lcd.print(timerVal/10.0,1);
          lcd.print("s   ");
          //EEPROM.update(address, timerVal);
          EEPROM.put(address,timerVal);
          Serial.println("Timer value increased and saved");
        }
        
        else if (pMode == 1) {
          flowVol ++;

          if (flowVol >= 32000)  {
            flowVol = 32000;
            Serial.println("Flow value +limit");
          }     
          lcd.setCursor(4,0);
          lcd.print(flowVol);
          lcd.print("ml   ");
          //EEPROM.update(address+6, flowVol / 10);
          EEPROM.put(address+20,flowVol);
          Serial.println("Flow volume value increased and saved");
        }     
      }
        
      //Remote button volume down
      else if (results.value == 1041 || results.value == 3089)  {
        if (pMode == 0) {
          timerVal--;
          if (timerVal < 0)  {
            timerVal = 0;
            Serial.println("Timer value -limit");
          }
          lcd.setCursor(6,0);
          lcd.print(timerVal/10.0,1);
          lcd.print("s   ");
          //EEPROM.update(address, timerVal);
          EEPROM.put(address,timerVal);
          Serial.println("Timer value decreased and saved");
        }
        
        else if (pMode == 1) {
          flowVol --;
          if (flowVol < 0)  {
            flowVol = 0;
            Serial.println("Timer value -limit");
          }
          lcd.setCursor(4,0);
          lcd.print(flowVol);
          lcd.print("ml   ");
          //EEPROM.update(address+6, flowVol / 10);
          EEPROM.put(address+20,flowVol);
          Serial.println("Flow volume value decreased and saved");
        } 
      }
      //Remote button 1
      else if (results.value == 1335 || results.value == 3383)  {
        mode0();
      }
      //Remote button 2
      else if (results.value == 1336 || results.value == 3384)  {
        mode1();
      }  
      irrecv.resume();  //receive the next value
    } 
    outputSet();
}

/*=======================================================================================
                                         Functions
========================================================================================*/

void versionDelay() {

  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Odori d.o.o.  v5");  //display version
  lcd.setCursor(0,1);
  for(int i=0; i<16; i++)  {   
    lcd.print(">");
    delay(50);
  }
}

void outputSet()  {

  if (loopTimeVal > 0) {
    Serial.print("delayVal:");
    Serial.println(loopTimeVal);
    lcd.setCursor(9,1);
    lcd.print("(");
    //lcd.setCursor(10,1);
    lcd.print(loopTimeVal/10.0,1);
    //lcd.setCursor(14,1);
    lcd.print(") ");
    loopTimeVal--;
    delay(100);      
  }
       
  else if (loopFlowVal > 0) {
    
    float liters = pulses / 617.0;
    Serial.print("Flow volume:");
    Serial.println(liters);
    lcd.setCursor(0,1);
    Serial.print("Flow rate:");
    lcd.print("FR:");
    if (flowrate < 200)  {
      Serial.println(0.0);
      lcd.print(0);
    }
    else  {
      Serial.println(flowrate);
      lcd.print(flowrate,0);
    }
    lcd.print(" FV:");
    lcd.print(liters * 1000,1);
    if (liters * 1000 >= flowVol) {
      loopFlowVal = 0;
    }
    delay(100);
  }

  else if  (socketStat == true) {
    digitalWrite(relay, HIGH);
    digitalWrite(LED_BUILTIN, LOW);
    Serial.println("Socket OFF");
    lcd.setCursor(0,1);
    lcd.println("Socket OFF      ");
    socketStat = false;
  }
}

void mode0() {

    Serial.println("Mode 0");
    pMode = 0;
    //EEPROM.update(address+5, pMode);
    EEPROM.put(address+10,pMode);
    loopFlowVal = 0;
    loopTimeVal = 0;
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Timer:");
    lcd.print(timerVal/10.0,1);
    lcd.print("s   ");
    lcd.setCursor(14,0);
    lcd.print("M1");
    lcd.setCursor(0,1);
    lcd.println("Socket OFF      ");
}

void mode1() {

  Serial.println("Mode 2");
    pMode = 1;
    //EEPROM.update(address+5, pMode);
    EEPROM.put(address+10,pMode);
    loopFlowVal = 0;
    loopTimeVal = 0;
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Vol:");
    lcd.print(flowVol);
    lcd.print("ml   ");
    lcd.setCursor(14,0);
    lcd.print("M2");
    lcd.setCursor(0,1);
    lcd.println("Socket OFF      ");
}
