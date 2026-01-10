#include <Wire.h>
#include <Adafruit_DRV2605.h>

const int FSR_PIN = 34;   
Adafruit_DRV2605 drv;


float R = 0.0f;   
float U = 0.0f;   
float S = 0.4f;  
float T = 0.8f;   


const float alphaR = 0.30f; 
const float alphaU = 0.30f;  


const float S0      = 0.4f;   
const float k_in    = 1.1f;   
const float k_out   = 0.5f;   
const float k_relax = 0.8f;  
const float Tmin    = 0.6f;
const float Tmax    = 2.0f;
const float gammaT  = 0.2f;   


unsigned long lastMillis = 0;
float lastNorm = 0.0f;


float phaseMotor = 0.0f;

void setup() {
  Serial.begin(115200);
  delay(200);


  pinMode(FSR_PIN, INPUT);


  Wire.begin();
  if (!drv.begin()) {
    Serial.println("DRV2605 not found");
    while (1) delay(10);
  }
  drv.selectLibrary(1);
  drv.useERM();                       
  drv.setMode(DRV2605_MODE_REALTIME);
}

void loop() {
  unsigned long now = millis();
  float dt = (now - lastMillis) / 1000.0f;
  if (dt <= 0) dt = 0.016f;
  lastMillis = now;


  int raw = analogRead(FSR_PIN);  
  float norm = raw / 4095.0f;
  if (norm < 0.0f) norm = 0.0f;
  if (norm > 1.0f) norm = 1.0f;

 
  R = (1.0f - alphaR) * R + alphaR * norm;

  float delta = fabs(norm - lastNorm);
  lastNorm = norm;
  float uRaw = delta * 4.0f;  
  if (uRaw > 1.0f) uRaw = 1.0f;
  U = (1.0f - alphaU) * U + alphaU * uRaw;


  if (norm < 0.02f) {  
    R *= 0.9f;
    U *= 0.8f;
  }


  float dS = k_in * U - k_out * R - k_relax * (S - S0);
  S += dS * dt;
  if (S < 0.0f) S = 0.0f;
  if (S > 1.0f) S = 1.0f;


  float targetT = Tmin + (Tmax - Tmin) * S;  
  T += gammaT * (targetT - T);
  if (T < Tmin) T = Tmin;
  if (T > Tmax) T = Tmax;

 
  float amp = updateMotor(dt);

  
  Serial.print("RAW:"); Serial.print(raw);
  Serial.print(",R:");  Serial.print(R, 3);
  Serial.print(",U:");  Serial.print(U, 3);
  Serial.print(",S:");  Serial.print(S, 3);
  Serial.print(",T:");  Serial.print(T, 3);
  Serial.print(",AMP:"); Serial.println(amp, 3);

  delay(20);
}


float updateMotor(float dt) {
  
  phaseMotor += 2.0f * 3.14159f * T * dt;
  if (phaseMotor > 2.0f * 3.14159f) {
    phaseMotor -= 2.0f * 3.14159f;
  }


  float base = 0.5f + 0.5f * sin(phaseMotor);


  float ampScale = 0.1f + 0.6f * R + 0.3f * U;
  if (ampScale > 1.0f) ampScale = 1.0f;


  float jitterMag = 0.25f * U + 0.10f * S;
  float jitter    = (random(-100, 101) / 100.0f) * jitterMag;

  float amp = base * ampScale + jitter;

  if (R < 0.03f && U < 0.03f) {

    float idleBase = 0.125f + 0.075f * sin(phaseMotor);
    amp = idleBase;
  }


  if (amp < 0.0f) amp = 0.0f;
  if (amp > 1.0f) amp = 1.0f;

 
  uint8_t mag = (uint8_t)(amp * 127.0f);
  drv.setRealtimeValue(mag);

  return amp;
}
