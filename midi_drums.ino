#include <Bounce.h>

//#include <frequencyToNote.h>
//#include <MIDIUSB.h>
//#include <MIDIUSB_Defs.h>
//#include <pitchToFrequency.h>
//#include <pitchToNote.h>

/***************************************************************
 * written for a Teensy 2.0
 * Takes 4 AI inputs from Pizo's
 * Processes as drum hits and plays a midi note
 **************************************************************/
//#include "MIDIUSB.h"

#define NUM_BUTTONS  7
#define AI0 0//21 pad
#define AI1 1//20 pad
#define AI2 2//19 pad
#define AI3 3//18 pad
#define AI4 4//17 kick drum

#define AI_RAW_HIGH 1023 //1030
#define AI_RAW_LOW 0
#define AI_EU_HIGH 127
#define AI_EU_LOW 20

#define AI_HARD_HIT_MODE_VAL 120.0
#define AI_SOFT_HIT_MODE_VAL 80.0
#define HARD_HIT_MODE 1
#define SOFT_HIT_MODE 2
#define VEL_HIT_MODE 0

#define AI_SMOOTH_CNT 4

//#define AI_HIGH_THRESHOLD 10
//#define AI_LOW_THRESHOLD 2
#define DRUM_HIT_DEADBAND 40
//#define DRUM_MIN_HIT 30
#define DRUM_HIT_ANALYZE 4   //how many values to read
#define MIDI_CHANNEL 10
#define AI_INPUTS_USED 5

#define HIT_CACHE_DURATION 5 //loops cache lasts
#define FASLE_HIT_DEADBAND 20.0 //Velocity for cached hit to cancel false hit



typedef struct {
  byte AIInputNum;
  int AIRaw;
  int AIMaxValue;
  int AILowRawTweak; //used to calibrate for different sensitivities 
  int AIHighRawTweak; //used to calibrate for different sensitivities 
  //float AIScaled;
  byte AISmoothCntr;
} AnalogPoint;

typedef struct  {
  bool isHit;
  int lastValue;
  float velocity;
  int hitAnalyzeCntr;
} HitData;

AnalogPoint AIPoints[AI_INPUTS_USED];
HitData hitDataPoints[AI_INPUTS_USED];
byte AIInputs[] = {AI0, AI1, AI2, AI3, AI4};

float cachedVelocity = 0.0;
int cachedVelocityTMR = 0;


const int AI_Raw_Low_Override[] = {0,0,0,0,400}; //Increases the RawLow Value
const int AI_Raw_High_Override[] = {0,200,0,0,0}; //Decreases the HighRaw Value by this much

const int intensityPot = 0;  //A0 input
const byte notePitches[] = {35, 38, 42, 49, 50};
int NoteStat;
int AI_MaxCounter;
int led = 11;
int BTN_IN = 10;
int LED_OUT_1 = 7;
int LED_OUT_2 = 8;
int LED_OUT_3 = 9;
Bounce button10 = Bounce(10, 10); //button 10 for 10 ms

byte hitMode = VEL_HIT_MODE;


int last;

void setup() {
  NoteStat = 0;
  AI_MaxCounter = 0;
  pinMode(led, OUTPUT);
  pinMode(LED_OUT_1, OUTPUT);
  pinMode(LED_OUT_2, OUTPUT);
  pinMode(LED_OUT_3, OUTPUT);
  pinMode(BTN_IN, INPUT_PULLUP); 
  for (int i=0;i<AI_INPUTS_USED;i++){
    AIPoints[i].AIInputNum = AIInputs[i];
    AIPoints[i].AIRaw = 0;
    AIPoints[i].AIMaxValue = 0;
    //AIPoints[i].AIScaled = 0;
    AIPoints[i].AILowRawTweak = AI_Raw_Low_Override[i];
    AIPoints[i].AIHighRawTweak = AI_Raw_High_Override[i];
    AIPoints[i].AISmoothCntr = AI_SMOOTH_CNT;
    hitDataPoints[i].hitAnalyzeCntr = -1;
    hitDataPoints[i].isHit = false;
    hitDataPoints[i].velocity = 0;
    hitDataPoints[i].lastValue = 0;
  }
  //Serial.begin(9600);
  updateHitModeLights();
}

void loop() {
  if (readAnalogs(AIPoints, AI_EU_HIGH, AI_EU_LOW)){
    checkForHits(AIPoints, hitDataPoints);
    sendMIDIForHits(hitDataPoints);  
  }
  //checkHitModeBTN();
  delay(1);
}

void checkHitModeBTN(){
  button10.update();
  if (button10.fallingEdge()) {
    changeHitMode();
  }
}

void changeHitMode(){
  hitMode++;
  if (hitMode >= 3){
    hitMode = 0;
  }
  updateHitModeLights();
}

void updateHitModeLights(){
  digitalWrite(LED_OUT_1, 0);
  digitalWrite(LED_OUT_2, 0);
  digitalWrite(LED_OUT_3, 0);
  switch (hitMode){
    case 0:
      digitalWrite(LED_OUT_1, 1);
      break;
    case 1:
      digitalWrite(LED_OUT_2, 1);
      break;
    case 2:
      digitalWrite(LED_OUT_3, 1);
      break;
  }
}
//reads AI and scales input to passed low / high range values
//processed values go to AI_Processed[]
bool readAnalogs(AnalogPoint AI_Points[], float highEU, float lowEU){
  bool analogsRead = false;
  for (int i = 0; i < AI_INPUTS_USED; i++){
    analogsRead = getAIValueWithSmoothing(&AI_Points[i]);//, highEU, lowEU);
  }
  return analogsRead;
}

void checkForHits(AnalogPoint AI_Points[], HitData hits[]){
  for (int i = 0; i < AI_INPUTS_USED; i++){
    analyzePointForHit(&AI_Points[i], &hits[i]);
    setHitVelocityCache(&hits[i]);
  }
}
//caches all hits for short period of time
//used to clear false hits
void setHitVelocityCache(HitData *hit){
  if (hit->isHit == true){
    if (hit->velocity > cachedVelocity){
      cachedVelocity = hit->velocity;
      cachedVelocityTMR = 0;
    }
  }
  //cache expired. clear cache
  if (cachedVelocityTMR > HIT_CACHE_DURATION){
    cachedVelocityTMR = -1;
    cachedVelocity = 0.0;
  }
  if (cachedVelocity >= 0){
    cachedVelocityTMR++;
  }
}


void analyzePointForHit(AnalogPoint *point, HitData *hit){
  //clear a true hit
  hit->isHit = false;
  
  //if counter active, then a hit was found and we are now analyzing
  if (hit->hitAnalyzeCntr >= 0){
    //get the highest read value for hit
    if (point->AIRaw > hit->lastValue){
      hit->lastValue = point->AIRaw;
    }
    //I use -1 to kind of hold the counter until it is needed again
    hit->hitAnalyzeCntr--;
  }
  else {   
    //hit found, begin analyze
    if (point->AIRaw > (hit->lastValue + point->AILowRawTweak + DRUM_HIT_DEADBAND)){
      hit->hitAnalyzeCntr = DRUM_HIT_ANALYZE;
      //digitalWrite(led, 1);
    }
    hit->lastValue = point->AIRaw;
  }
  
  if (hit->hitAnalyzeCntr == 0){
    hit->isHit = true;

    hit->velocity = scaleAI(hit->lastValue, AI_RAW_HIGH - point->AIHighRawTweak, AI_RAW_LOW + point->AILowRawTweak, AI_EU_HIGH, AI_EU_LOW);
    
    //check if a false hit --feedback from a different hit
    if ((cachedVelocity - hit->velocity) > FASLE_HIT_DEADBAND){
      hit->isHit = false;
    }
    else{
      hit->velocity = getVeloictyForMode(hit->velocity);
      //digitalWrite(led, 0);
    }
  }
}

float getVeloictyForMode(float velocity){
  switch(hitMode){
    case VEL_HIT_MODE:
      return velocity;
    case HARD_HIT_MODE:
      return AI_HARD_HIT_MODE_VAL;
    case SOFT_HIT_MODE:
      return AI_SOFT_HIT_MODE_VAL;
  }
  return 0;
}

//returns true if point is updated
//point is passed by reference as it will be modified
bool getAIValueWithSmoothing(AnalogPoint *point){//, float highEU, float lowEU){
  int val;
  
  val = analogRead(point->AIInputNum);
  if (val > point->AIMaxValue){
    point->AIMaxValue = val;
  }
  point->AISmoothCntr--;

  if (point->AISmoothCntr <= 0){
    point->AIRaw = point->AIMaxValue;
    //point->AIScaled = scaleAI(point->AIMaxValue, AI_RAW_HIGH, AI_RAW_LOW, highEU, lowEU);
    point->AISmoothCntr = AI_SMOOTH_CNT;
    point->AIMaxValue = 0;
    return true;
  }
  return false;
}


void sendMIDIForHits(HitData hits[]){

  for (int i = 0; i < AI_INPUTS_USED; i++){
    if (hits[i].isHit){
      /*Serial.print(i);
      Serial.print(" - ");
      Serial.print(hits[i].lastValue);
      Serial.print(" - ");
      Serial.println(hits[i].velocity);*/
      noteOn(MIDI_CHANNEL, notePitches[i], hits[i].velocity);
     // usbMIDI.flush();
      bitSet(NoteStat,i);
    }
    else if (bitRead(NoteStat,i)){
      noteOff(MIDI_CHANNEL, notePitches[i], 0);
    //  MidiUSB.flush();
      bitClear(NoteStat,i);
    }
  }
}

/***********************************************************************************
AI handling functions
***********************************************************************************/

float scaleAI(int rawVal, int highRaw, int lowRaw, float highEU, float lowEU){
  float retVal = (float)((((float)(rawVal - lowRaw)/(float)(highRaw - lowRaw)) * (highEU - lowEU)) + lowEU);
  if (retVal > highEU)
    retVal = highEU;
  if (retVal < lowEU)
    retVal = lowEU;
  return retVal;
}

/***********************************************************************************
MIDI handling functions
***********************************************************************************/

// First parameter is the event type (0x09 = note on, 0x08 = note off).
// Second parameter is note-on/note-off, combined with the channel.
// Channel can be anything between 0-15. Typically reported to the user as 1-16.
// Third parameter is the note number (48 = middle C).
// Fourth parameter is the velocity (64 = normal, 127 = fastest).
void noteOn(byte channel, byte pitch, byte velocity) {
  usbMIDI.sendNoteOn(pitch, velocity, channel); 
  //midiEventPacket_t noteOn = {0x09, 0x90 | channel, pitch, velocity};
  //MidiUSB.sendMIDI(noteOn);
  digitalWrite(led, 1);
  //Serial.print("note On\n");
}
void noteOff(byte channel, byte pitch, byte velocity) {
  usbMIDI.sendNoteOff(pitch, velocity, channel);
  //midiEventPacket_t noteOff = {0x08, 0x80 | channel, pitch, velocity};
  //MidiUSB.sendMIDI(noteOff);
  digitalWrite(led, 0);
  //Serial.print("note Off\n");
  
}
// First parameter is the event type (0x0B = control change).
// Second parameter is the event type, combined with the channel.
// Third parameter is the control number number (0-119).
// Fourth parameter is the control value (0-127).
void controlChange(byte channel, byte control, byte value) {
  usbMIDI.sendControlChange(control, value, channel);
  //midiEventPacket_t event = {0x0B, 0xB0 | channel, control, value};
  //MidiUSB.sendMIDI(event);
  
}
