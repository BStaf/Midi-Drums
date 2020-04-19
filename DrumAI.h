typedef struct {
    float AI_RAW_HIGH;
    float AI_RAW_LOW;
    float AI_EU_HIGH;
    float AI_EU_LOW;
    int AILowRawTweak;
    int AIHighRawTweak;
} ScalingValues;

/*typedef struct {
  byte AIInputNum;
  int AIRaw;
  int AIMaxValue;
  int AILowRawTweak; //used to calibrate for different sensitivities 
  int AIHighRawTweak; //used to calibrate for different sensitivities 
  //float AIScaled;
  byte AISmoothCntr;
} AnalogPoint;*/

typedef struct  {
  bool isHit;
  int lastValue;
  float velocity;
  int hitAnalyzeCntr;
} HitData;

class DrumAI  {
  private:
    byte _inputNum;
    int _rawValue;
    int _maxValue;
    //int AILowRawTweak; //used to calibrate for different sensitivities 
    //int AIHighRawTweak; //used to calibrate for different sensitivities 
    //float AIScaled;
    byte _aiSmoothCntr;
    ScalingValues _scalingValues;
  public:
    DrumAI(byte inputNum, ScalingValues scalingValues);
    HitData GetHitData();//returns currentHitData
    void Init();
};

DrumAI::DrumAI(byte inputNum, ScalingValues scalingValues){
  _inputNum = inputNum;
  _scalingValues = scalingValues;
}
HitData DrumAI::GetHitData(){
  analyzePointForHit
}
/*void checkForHits(AnalogPoint AI_Points[], HitData hits[]){
  for (int i = 0; i < AI_INPUTS_USED; i++){
    analyzePointForHit(&AI_Points[i], &hits[i]);
    //setHitVelocityCache(&hits[i]);
  }
}*/

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
  }
}