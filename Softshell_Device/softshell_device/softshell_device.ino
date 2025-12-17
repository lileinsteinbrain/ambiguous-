// Softshell Device v1.3 — 更强的“松手平静 / 乱按混乱”触感版
// ESP32 + FSR(analog) + DRV2605 (ERM 小圆马达)

#include <Wire.h>
#include <Adafruit_DRV2605.h>

const int FSR_PIN = 34;   // FSR AO 接 34
Adafruit_DRV2605 drv;

// --- 状态变量 ---
float R = 0.0f;   // 输入强度 0..1  （你按多重）
float U = 0.0f;   // 不确定性 0..1 （你按得多乱）
float S = 0.4f;   // 系统熵态 0..1  初始偏平静一点
float T = 0.8f;   // 呼吸 Hz (0.6..2.0)

// EMA 参数
const float alphaR = 0.30f;  // R 平滑
const float alphaU = 0.30f;  // U 平滑

// 动力系统参数（比之前更敏感、更快回落）
const float S0      = 0.4f;   // 平静基线下移
const float k_in    = 1.1f;   // U → S 注入熵（乱按时 S 更容易飙高）
const float k_out   = 0.5f;   // R → S 导出熵（稳按时更容易把 S 压低）
const float k_relax = 0.8f;   // 回到 S0 更快
const float Tmin    = 0.6f;
const float Tmax    = 2.0f;
const float gammaT  = 0.2f;   // T 对 S 变化更敏感一些

// 时间相关
unsigned long lastMillis = 0;
float lastNorm = 0.0f;

// 马达相位
float phaseMotor = 0.0f;

void setup() {
  Serial.begin(115200);
  delay(200);

  // FSR
  pinMode(FSR_PIN, INPUT);

  // DRV2605 初始化
  Wire.begin();
  if (!drv.begin()) {
    Serial.println("DRV2605 not found");
    while (1) delay(10);
  }
  drv.selectLibrary(1);
  drv.useERM();                       // 我们用小饼 ERM
  drv.setMode(DRV2605_MODE_REALTIME); // 实时模式
}

void loop() {
  unsigned long now = millis();
  float dt = (now - lastMillis) / 1000.0f;
  if (dt <= 0) dt = 0.016f;
  lastMillis = now;

  // ---- 1. 读 FSR ----
  int raw = analogRead(FSR_PIN);   // ESP32 0..4095
  float norm = raw / 4095.0f;
  if (norm < 0.0f) norm = 0.0f;
  if (norm > 1.0f) norm = 1.0f;

 
  R = (1.0f - alphaR) * R + alphaR * norm;

  float delta = fabs(norm - lastNorm);
  lastNorm = norm;
  float uRaw = delta * 4.0f;  
  if (uRaw > 1.0f) uRaw = 1.0f;
  U = (1.0f - alphaU) * U + alphaU * uRaw;

  // ---- 3.5 没按的时候，让 R / U 更快衰减，让系统更快冷静 ----
  if (norm < 0.02f) {   // 基本没压到
    R *= 0.9f;
    U *= 0.8f;
  }

  // ---- 4. 动力系统：更新 S ----
  float dS = k_in * U - k_out * R - k_relax * (S - S0);
  S += dS * dt;
  if (S < 0.0f) S = 0.0f;
  if (S > 1.0f) S = 1.0f;


  float targetT = Tmin + (Tmax - Tmin) * S;  
  T += gammaT * (targetT - T);
  if (T < Tmin) T = Tmin;
  if (T > Tmax) T = Tmax;

  // ---- 6. 更新“呼吸式”马达 + 映射 AMP ----
  float amp = updateMotor(dt);

  // ---- 7. 串口输出（给你看 & 可以喂 TD）----
  Serial.print("RAW:"); Serial.print(raw);
  Serial.print(",R:");  Serial.print(R, 3);
  Serial.print(",U:");  Serial.print(U, 3);
  Serial.print(",S:");  Serial.print(S, 3);
  Serial.print(",T:");  Serial.print(T, 3);
  Serial.print(",AMP:"); Serial.println(amp, 3);

  delay(20); // ~50Hz
}

// 用 R / U / S / T 生成三种手感：
// 1) 松手 ≈ baseline：极轻微、慢呼吸
// 2) 稳稳按住（R 高、U 低）：中等力度、顺滑呼吸
// 3) 乱按（U 高）：高潮起伏 + jitter
float updateMotor(float dt) {
  // 1. 相位：用 T 控制一整次吸+呼
  phaseMotor += 2.0f * 3.14159f * T * dt;
  if (phaseMotor > 2.0f * 3.14159f) {
    phaseMotor -= 2.0f * 3.14159f;
  }

  // 2. 基础呼吸波形（0..1）
  float base = 0.5f + 0.5f * sin(phaseMotor);

  // 3. 按得越重/越乱 → 呼吸幅度越大
  float ampScale = 0.1f + 0.6f * R + 0.3f * U;
  if (ampScale > 1.0f) ampScale = 1.0f;

  // 4. 抖动感主要跟 U 走，S 辅助一点
  float jitterMag = 0.25f * U + 0.10f * S;
  float jitter    = (random(-100, 101) / 100.0f) * jitterMag;

  float amp = base * ampScale + jitter;

  // ⭐ 特殊处理：完全没按的时候，给一个很轻的 idle 呼吸
  if (R < 0.03f && U < 0.03f) {
    // 小幅度、慢悠悠：0.05 ~ 0.20 之间轻轻起伏
    float idleBase = 0.125f + 0.075f * sin(phaseMotor);
    amp = idleBase;
  }

  // 限幅
  if (amp < 0.0f) amp = 0.0f;
  if (amp > 1.0f) amp = 1.0f;

  // 写入 DRV2605 实时值（0..127）
  uint8_t mag = (uint8_t)(amp * 127.0f);
  drv.setRealtimeValue(mag);

  return amp;
}