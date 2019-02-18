/***************************************************************
 * written for a Teensy 2.0
 * Takes 4 AI inputs from Pizo's
 * Processes as drum hits and plays a midi note
 **************************************************************/
#include "MIDIUSB.h"

#define NUM_BUTTONS  7
#define AI0 0//21
#define AI1 1//20
#define AI2 2//19
#define AI3 3//18

#define AI_RAW_HIGH 1000 //1030
#define AI_RAW_LOW 0
#define AI_EU_HIGH 127
#define AI_EU_LOW 40

#define AI_SMOOTH_CNT 4

//#define AI_HIGH_THRESHOLD 10
//#define AI_LOW_THRESHOLD 2
#define DRUM_HIT_DEADBAND 20
//#define DRUM_MIN_HIT 30
#define DRUM_HIT_ANALYZE 7 //how many values to read
#define MIDI_CHANNEL 0
#define AI_INPUTS_USED 4

#define HIT_CACHE_DURATION 20 //loops cache lasts
#define FASLE_HIT_DEADBAND 37.0 //Velocity for cached hit to cancel false hit

typedef struct {
  byte AIInputNum;
  int AIRaw;
  int AIMaxValue;
  int AILowRawTweak; //used to calibrate for different sensitivities 
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
byte AIInputs[] = {AI0, AI1, AI2, AI3};

float cachedVelocity = 0.0;
int cachedVelocityTMR = 0;


const int AI_Raw_Low_Override[] = {0,0,0,0}; //Use this for tweaking each input

const int intensityPot = 0;  //A0 input
const byte notePitches[] = {35, 38, 42, 49};
int NoteStat;
int AI_MaxCounter;
int led = 11;

int last;

void setup() {
  NoteStat = 0;
  AI_MaxCounter = 0;
  pinMode(led, OUTPUT);
  for (int i=0;i<AI_INPUTS_USED;i++){
    AIPoints[i].AIInputNum = AIInputs[i];
    AIPoints[i].AIRaw = 0;
    AIPoints[i].AIMaxValue = 0;
    //AIPoints[i].AIScaled = 0;
    AIPoints[i].AILowRawTweak = AI_Raw_Low_Override[i];
    AIPoints[i].AISmoothCntr = AI_SMOOTH_CNT;
    hitDataPoints[i].hitAnalyzeCntr = -1;
    hitDataPoints[i].isHit = false;
    hitDataPoints[i].velocity = 0;
    hitDataPoints[i].lastValue = 0;
  }
  Serial.begin(38400);
}

void loop() {
  if (readAnalogs(AIPoints, AI_EU_HIGH, AI_EU_LOW)){
    checkForHits(AIPoints, hitDataPoints);
    sendMIDIForHits(hitDataPoints);  
  }
  delay(1);
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
      digitalWrite(led, 1);
    }
    hit->lastValue = point->AIRaw;
  }
  
  if (hit->hitAnalyzeCntr == 0){
    hit->isHit = true;
    hit->velocity = scaleAI(hit->lastValue, AI_RAW_HIGH, AI_RAW_LOW + point->AILowRawTweak, AI_EU_HIGH, AI_EU_LOW);
    //check if a false hit --feedback from a different hit
    if ((cachedVelocity - hit->velocity) > FASLE_HIT_DEADBAND){
      hit->isHit = false;
    }
    else{
      digitalWrite(led, 0);
    }
  }
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
      noteOn(MIDI_CHANNEL, notePitches[i], hits[i].velocity);
      MidiUSB.flush();
      bitSet(NoteStat,i);
    }
    else if (bitRead(NoteStat,i)){
      noteOff(MIDI_CHANNEL, notePitches[i], 0);
      MidiUSB.flush();
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
  midiEventPacket_t noteOn = {0x09, 0x90 | channel, pitch, velocity};
  MidiUSB.sendMIDI(noteOn);
  digitalWrite(led, 1);
  //Serial.print("note On\n");
}
void noteOff(byte channel, byte pitch, byte velocity) {
  midiEventPacket_t noteOff = {0x08, 0x80 | channel, pitch, velocity};
  MidiUSB.sendMIDI(noteOff);
  digitalWrite(led, 0);
  //Serial.print("note Off\n");
  
}
// First parameter is the event type (0x0B = control change).
// Second parameter is the event type, combined with the channel.
// Third parameter is the control number number (0-119).
// Fourth parameter is the control value (0-127).
void controlChange(byte channel, byte control, byte value) {
  midiEventPacket_t event = {0x0B, 0xB0 | channel, control, value};
  MidiUSB.sendMIDI(event);
  
}
