//  DEDICATION
//  To the memory of Dr. Leonard (Lenny) Bernstein, an inspiring chemical engineer,
//  climate scientist, and novelist, who provided tremendous guidance and resources
//  to improve the educational experience of our students.


//  OVERVIEW
/*  WET PROCESS CONTROL LABORATORY ON EACH STUDENT'S DESK (OR FLOOR).
 *
 *  Using the Arduino UNO microcontroller attached via the USB port to a laptop,
 *  this software and associated hardware controls the temperature of a water-filled
 *  cylindrical container with thin walls of high thermal conductivity (e.g., a
 *  soda can) by low frequency pulse width modulation (PWM) of a 125W beverage
 *  heater (immersion coil).  This is done by turning on or off a relay.  For safety,
 *  either a completely enclosed electromagnetic relay or an enclosed and
 *  finger-protected solid state relay should be used.  In addition, two 5V fans
 *  powered from the Arduino (peak draw current 180 mA each) are used to increase the
 *  maximum cooling rate, the fraction of time the heater is on at safe temperatures,
 *  and to provide disturbances. The fan velocity can be changed by using
 *  Arduino-provided low frequency PWM of a logic level MOSFET which controls both
 *  fans.  The temperature is measured using a DS18B20 waterproof sensor.
 *
 *  Authored by Spyros A Svoronos, Department of Chemical Engineering, University of
 *  Florida, Dec 2016 - Mar 2017
 *  Adapted by Anthony Arrowood, Jun 2018 - Aug 2018
 *
 */

//  This version of the software is for a student competition in manually controlling
//  the temperature. It is inspired by a similar competition via a CSTR simulation
//  authored by Wilbur Woo.


//  TUNING
float Kc = 0;  // controller gain (% power on / deg C)
float tauI = 3.0;  // integral time constant in min
float tauD = 0;  // derivative time constant in min
float tauF = 0; // filter time constant in min.  Rule of thumb: Choose it 0.1-0.2 * tauD
float KI;  // if (proportionalFlag) {KI = 0} else {KI = Kc / tauI}

bool noIntegralFlag = false; //set to 1 for eliminating integral action
bool positionFlag = false; //set to 1 for the position form of the PID law, to 0 for the velocity form
bool filterAll = false;
float percentRelayOnNominal = 10;  //Nominal value used in the position form of PID, 13% for 40 C
float sumForIntegral = 0;

// //PRELIMINARIES
#include <OneWire.h> //library for DS18B20
#include "com.h"

//Soft serial pins
#define rxPin 8   //connect to green wire of CP210 or to orange wire of FTDI
#define txPin 7   //connect to white wire of CP210 or to yellow wire of FTDI

//Relay pin
#define relayPin 5

//Logic level MOSFET pin
#define fetPin 9  //PWM pin 9 or 10 since we might change frequency and need millis()

//DS18S20 Signal pin on digital 2
#define DS18S20_Pin 2

byte pinState = 0;
// Temperature chip i/o
OneWire ds(DS18S20_Pin);  // on digital pin 2
//The next statements declare other global variables
unsigned long relayPeriod = 250; //relay pwm period = 0.25 sec. Increase to 2.5 sec for EMR

unsigned long stepSize = 20 * relayPeriod;  //sampling period, choose integer * relayPeriod
float Dt = stepSize/1000./60.;  //converting stepSize to min
unsigned long probeTime = 800;  //probe conversion time = 800 msec
float percentRelayOn = 0; //initial setting to 0 in case the heater is not immersed
float temperature = 25;  //reasonable beginning teperatures (deg C)
float temperaturePrev = temperature; //y(t-1)
float tempFiltPrevPrev = temperature; //yF(t-2)
float tempFiltPrev = temperature; //yF(t-1)
float tempFiltered = temperature; //yF(t)
float error = 0;
float errorPrev = error;
// Tuning parameters moved up to make changes easier
unsigned long tRelayStart;  //The next statements declare other global variables
unsigned long startConversionTime;
unsigned long tLoopStart;
bool stopTempHigh = 0;   //flag ending the run due to high temperature
bool stopTimeEnd = 0;   //flag ending the test when time is up
float elapsedTime; //min, used for test

float changeTime[] = {0., 12., 21., 30., 30., 30.}; //min, used for test
float Tsp[] = {25., 36., 40., 40., 40., 40.}; //deg C, used for test, set points UNTIL each changeTime, 2nd value is 1st implemented
byte fanSetting[] = {255, 255, 255, 0, 255, 255}; // controls fan speed, used for test, values UNTIL each changeTime

float Tmax = 60;   //used for test
float Jysum = 0;      //Needed for the y-performance measure, sum of squared errors
unsigned long nJy = 0;  //Needed for the y-performance measure, number of measurements
float Jy; //Becomes the average squared error
float inputExclusionTime = 5.0; //min, used for test, time after a change not used in the calulation of input variance
int nJu[]   = {0, 0, 0, 0, 0, 0}; //Needed for the Jui performance measure, number of values after input exclusion time
float AA[]  = {0., 0., 0., 0., 0., 0.}; //Needed for calculating the variance of u
float QQ[]  = {0., 0., 0., 0., 0., 0.}; //Needed for calculating the variance of u
float Jui[] = {0., 0., 0., 0., 0., 0.}; //input variance after exclusion time
float Ju; //Becomes the average input variance, averaging the values after changes
float J;  //Overall score, weighted average of Jy and Jx

float fanSpeed = fanSetting[1];  //range 0 or 115 -255, startup cannot be as low as when dynamically changing
float TsetPoint = Tsp[1];  // deg C
int tdelay = 3; //delay in msec used with serial interaction commands
int tGETSET = 1000;  //delay before some GET and SET commands
bool T_sensorOK = true;

/******shared between C++ code */
#define i_kc            0
#define i_tauI          1
#define i_tauD          2
#define i_tauF          3
#define i_positionForm  4
#define i_filterAll     5
#define i_pOnNominal    6
#define i_setPoint      7
#define i_percentOn     8
#define i_fanSpeed      9
#define i_temperature   10
#define i_tempFiltered  11
#define i_time          12
#define i_inputVar      13
#define i_avg_err       14
#define i_score         15
#define NUMVARS         16
#define BUFFERSIZE 500

COM com;

char buffer[BUFFERSIZE];
void check_input()
{
  // checks for input from the port and potentially changes parameters
  /*  I dont use the serial.readString becuse it will read ANY size input
  presenting the possibiliity of a buffer overflow. this will read up to a closing bracket
  or until the input buffer is at max capacity.
  */
  if (Serial.available()) {
    int j = 0;
    for (unsigned int i = 0; i < BUFFERSIZE - 1 && j < 500; i ++) {
      char c = Serial.read();
      if ( c == -1){
        i -=1;
        j +=1;
        continue;
      }
      buffer[i] = c; // place this character in the input buffer
      if (c == ']' || c == '\0' ) { // stop reading chracters if we have read the last bracket or a null charcter
        buffer[i+1] = '\0'; // this will null terminate the buffer
        break;
      }
      if (c == '!')
        shutdown();
      delay(10);
    }
    while (Serial.available())
      char _ = Serial.read(); // this throws aways any other character in the buffer after the first right bracket
    com.deserialize_array(buffer);
  }
}

void setFanPwmFrequency(int pin, int divisor) {
  //From http://playground.arduino.cc/Code/PwmFrequency?action=sourceblock&num=2
  byte mode;
  if(pin == 5 || pin == 6 || pin == 9 || pin == 10) {
    switch(divisor) {
      case 1: mode = 0x01; break;
      case 8: mode = 0x02; break;
      case 64: mode = 0x03; break;
      case 256: mode = 0x04; break;
      case 1024: mode = 0x05; break;
      default: return;
    }
    if(pin == 5 || pin == 6) {
      TCCR0B = TCCR0B & 0b11111000 | mode;
    } else {
      TCCR1B = TCCR1B & 0b11111000 | mode;
    }
  } else if(pin == 3 || pin == 11) {
    switch(divisor) {
      case 1: mode = 0x01; break;
      case 8: mode = 0x02; break;
      case 32: mode = 0x03; break;
      case 64: mode = 0x04; break;
      case 128: mode = 0x05; break;
      case 256: mode = 0x06; break;
      case 1024: mode = 0x07; break;
      default: return;
    }
    TCCR2B = TCCR2B & 0b11111000 | mode;
  }
}

void relayCare(void){         // Continues relay looping
    unsigned long timeHere = millis();
    if (timeHere - tRelayStart <0) {tRelayStart = 0;} //for runs longer than 49 days
    if (timeHere - tRelayStart <= (1.-percentRelayOn/100.)*relayPeriod) {
      digitalWrite(relayPin, LOW);
    }
    else if (timeHere - tRelayStart < relayPeriod) {
      digitalWrite(relayPin, HIGH);
    }
    else{
      digitalWrite(relayPin, LOW);
      tRelayStart = timeHere;
    }
}

void shutdown(){
    digitalWrite(relayPin,LOW); //turn heater off
    analogWrite(fetPin, 255);  //turn fans on to max speed
    while(true){
      delay(1000);
    }
}

void setup(void) {
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, LOW); //start with relay off
  pinMode(fetPin, OUTPUT);
  setFanPwmFrequency(fetPin,1024);  //64 = default divisor (use 64,256,1024)
  analogWrite(fetPin, fanSpeed);  //start fan
  Serial.begin(9600);
  delay(tdelay);
  tRelayStart = millis(); //starting relay period
  delay(1000);

}

void loop(void) {  //MAIN CODE iterates indefinitely

  //SAMPLING PROCESS STARTS - RELAY MUST CONTINUE CYCLING
  tLoopStart = millis(); //not the same as sampling interval start due to delay
  relayCare();

  //The following code calculates the temperature from one DS18B20 in deg Celsius
  //It is adapted from http://bildr.org/2011/07/ds18b20-arduino/

  byte data[12];
  byte addr[8];
  if ( !ds.search(addr)) {
       //no more sensors on chain, reset search
       ds.reset_search();
  }
  if ( OneWire::crc8( addr, 7) != addr[7]) {
       Serial.println(F("CRC is not valid!"));
  }
  if ( addr[0] != 0x10 && addr[0] != 0x28) {
      Serial.println(F("Temperature probe malfunctioning!"));
  }
  ds.reset();
  ds.select(addr);
  ds.write(0x44); // start conversion, without parasite power on at the end

  startConversionTime = millis();
  while(millis()- startConversionTime < probeTime)    //wait for probe to finish conversion
  {
    relayCare();
  }

  byte present = ds.reset();
  ds.select(addr);
  ds.write(0xBE); // Read scratchpad

  relayCare();

  for (int i = 0; i < 9; i++) { // we need 9 bytes
    data[i] = ds.read();
  }

  relayCare();

  ds.reset_search();
  byte MSB = data[1];
  byte LSB = data[0];
  float tempRead = ((MSB << 8) | LSB); //using two's compliment
  temperaturePrev = temperature;
  temperature = tempRead / 16;  //This the temperature reading in degrees C
  tempFiltPrevPrev = tempFiltPrev; //updating filtered measurements
  tempFiltPrev = tempFiltered;
  tempFiltered = Dt/(Dt+tauF)*temperature + tauF/(Dt+tauF)*tempFiltPrev;

  relayCare();

  //Process Changes
  elapsedTime = millis()/60000. ;
  for(int i = 1; i<6; i++){
    if (elapsedTime >= changeTime[i-1] & elapsedTime < changeTime[i]) {
      TsetPoint = Tsp[i];
      fanSpeed = fanSetting[i];
    }
  }
  analogWrite(fetPin, fanSpeed);

  if (elapsedTime >= changeTime[5])
  {
    stopTimeEnd = 1;
    shutdown();
  }

  relayCare();


  //AUTOMATIC CONTROL

  //The algorithm eliminates the derivative set-point kick
  //& can filter the measurement for derivative action
  // positionFlag lets the user choose between the velocity
  // and the position forms of the PID law.
  //The position form uses several elements of Brett Beauregard's
  //PID library (http://playground.arduino.cc/Code/PIDLibrary)

  errorPrev = error;
    if (filterAll)
   {
    error = TsetPoint - tempFiltered;
   } else {
    error = TsetPoint - temperature;
   }

  //CALCULATIONS FOR THE SCORE FOLLOW
  if (elapsedTime > changeTime[1]) {   //average squared error is calculated after changeTime[1]
    Jysum += error * error;
    nJy ++;
    Jy = Jysum / nJy;

  }
  //Input Variance Calculations Using Algorithm in https://en.wikipedia.org/wiki/Standard_deviation
  for (int i = 2; i < 6; i++) { // we start with 2nd change response to allow everyone to start from the same point
    if (elapsedTime > changeTime[i-1] + inputExclusionTime & elapsedTime <= changeTime[i]){  //for ith input variance
      nJu[i] ++;
      QQ[i] += (percentRelayOn-AA[i])*(percentRelayOn-AA[i])*(nJu[i]-1)/nJu[i];
      AA[i] += (percentRelayOn-AA[i])/ nJu[i];
      Jui[i] = QQ[i]/(nJu[i]-1);
    }
  }
  float sumNum = 0;
  float sumDenom = 0;
  for (int i = 2; i < 6; i++) {
    sumNum += nJu[i]*Jui[i];
    sumDenom += nJu[i];
  }
  Ju = sumNum/sumDenom;
  J = 0.65 * Jy + 0.35 * Ju;

  //CONTROL LAW CALCULATIONS

  if (!positionFlag) {
    float percentOnPrevious = percentRelayOn;
    float PropInt = percentOnPrevious + Kc *(error - errorPrev) + Dt * Kc * error / tauI ;
    //Note:  To eliminate set-point kick (at the expense of slower responses to set-point changes) use:
    //float PropInt = percentOnPrevious + Kc *(-temperature + temperaturePrev) + Dt * Kc * error / tauI ;
    float unconstrainedPercentRelayOn = PropInt + Kc*tauD/Dt*(-tempFiltered+2.*tempFiltPrev-tempFiltPrevPrev);
    if (unconstrainedPercentRelayOn < 0.) {percentRelayOn = 0.;}
    else if (unconstrainedPercentRelayOn > 100.) {percentRelayOn = 100.;}
    else {percentRelayOn = unconstrainedPercentRelayOn;}
  }
  else {
    if (noIntegralFlag) {KI = 0;}
    else {KI = Kc / tauI;}
    sumForIntegral += KI*error;
    float PropInt = percentRelayOnNominal + Kc * error + Dt * sumForIntegral;
    float unconstrainedPercentRelayOn = PropInt + Kc*tauD/Dt*(-tempFiltered + tempFiltPrev);
    if (unconstrainedPercentRelayOn < 0.) {
      float woInt = percentRelayOnNominal + Kc * error + Kc*tauD/Dt*(-tempFiltered + tempFiltPrev);
      sumForIntegral = (0-woInt)/Dt; //anti reset windup
      percentRelayOn = 0.;
    }
    else if (unconstrainedPercentRelayOn > 100.) {
      float woInt = percentRelayOnNominal + Kc * error + Kc*tauD/Dt*(-tempFiltered + tempFiltPrev);
      sumForIntegral = (100 - woInt)/Dt; //anti reset windup
      percentRelayOn = 100.;
    }
    else {percentRelayOn = unconstrainedPercentRelayOn;}
  }
  relayCare();
  //REST OF LOOP AFTER CONTROL ACTION
  //The sampling interval actually ends when a new measurement is available
  //loopPeriod = samplingPeriod, but sampling interval lags behind

    /* place current values in the output array */
  com.set(i_pOnNominal, percentRelayOnNominal);
  com.set(i_kc, Kc);
  com.set(i_tauI, tauI);
  com.set(i_tauD, tauD);
  com.set(i_tauF, tauF);
  com.set(i_positionForm, positionFlag);
  com.set(i_filterAll, filterAll);
  com.set(i_setPoint, TsetPoint);
  com.set(i_percentOn, percentRelayOn);
  com.set(i_fanSpeed, fanSpeed);
  com.set(i_temperature, temperature);
  com.set(i_tempFiltered, tempFiltered);
  com.set(i_time, millis() /60000.0);
  com.set(i_inputVar, Ju);
  com.set(i_avg_err, Jy);
  com.set(i_score, J);

  /* Check if temperature is within realistic measurement range. */
  if(temperature < 80.0 && temperature > 0.0){
    T_sensorOK = true;
    com.printCurVals(); // show current parameters if the temperature is within realistic range
  } else { // the probe is malfunctioning because the reading is unrealistic
    if( T_sensorOK ) {
      T_sensorOK = false;
    } else {
      com.printCurVals(); // show current parameters if the temperature is within realistic range
      Serial.println("Shutting down due to issue with temperature probe! Check that no wire got loose.");
      shutdown();   
    }
  }
  /* Check if temperautre outside of safe range and within realistic measurements */
  if (temperature > Tmax && T_sensorOK) {
    Serial.println("Shutting down due to overheat!");
    shutdown();
  }
  
  while ( millis() < tLoopStart + stepSize ){
    relayCare();
    check_input();
  }

  /*
  Here we will change any values that must be changed
  */
  Kc                    = com.get(i_kc);
  tauI                  = com.get(i_tauI);
  tauD                  = com.get(i_tauD);
  tauF                  = com.get(i_tauF);
  filterAll             = com.get(i_filterAll);
  positionFlag          = com.get(i_positionForm);
  percentRelayOnNominal = com.get(i_pOnNominal);
}
