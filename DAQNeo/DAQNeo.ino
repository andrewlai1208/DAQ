// ---------------------------------------------------------
// DAQNeo 實體 AD7606 擷取版 (Arduino Mega 2560 專用)
// ⚠️ 完全依照最新電路原理圖配置：PORTC 正序 / PORTA 反序
// ---------------------------------------------------------

const int PIN_DIGITAL_OUT = 12; 

// ================== 【腳位完全對齊電路圖】 ==================
const int PIN_CVB   = 40;  
const int PIN_CVA   = 41;  
const int PIN_BUSY  = 42;  
const int PIN_RD    = 44;  
const int PIN_CS    = 45;  
const int PIN_RST   = 47;  

const int PIN_RANGE = 48; 
const int PIN_OS2   = 49; 
const int PIN_OS1   = 50; 
const int PIN_OS0   = 52; 
// ==========================================================

unsigned long lastSampleTime = 0;
unsigned long lastLedTime = 0;
bool ledState = false;

// 僅用於 PORTA 的高效位元反轉函式
inline uint8_t reverseBits(uint8_t b) {
  b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
  b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
  b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
  return b;
}

void setOversampling(int mode) {
  digitalWrite(PIN_OS0, (mode & 0x01) ? HIGH : LOW);
  digitalWrite(PIN_OS1, (mode & 0x02) ? HIGH : LOW);
  digitalWrite(PIN_OS2, (mode & 0x04) ? HIGH : LOW);
}

void setup() {
  Serial.begin(115200);         
  pinMode(LED_BUILTIN, OUTPUT); 
  pinMode(PIN_DIGITAL_OUT, OUTPUT);

  pinMode(PIN_CVA, OUTPUT);
  pinMode(PIN_CVB, OUTPUT);
  pinMode(PIN_RST, OUTPUT);
  pinMode(PIN_CS, OUTPUT);
  pinMode(PIN_RD, OUTPUT);
  pinMode(PIN_BUSY, INPUT);
  
  pinMode(PIN_OS0, OUTPUT);
  pinMode(PIN_OS1, OUTPUT);
  pinMode(PIN_OS2, OUTPUT);
  pinMode(PIN_RANGE, OUTPUT);

  DDRA = 0x00; // 設為輸入 (PORTA: DB8~DB15)
  DDRC = 0x00; // 設為輸入 (PORTC: DB0~DB7)

  setOversampling(0); 

  // 將 RANGE 拉高，設定為 ±10V 量程
  digitalWrite(PIN_RANGE, HIGH);

  // 初始狀態設定
  digitalWrite(PIN_RST, LOW);
  digitalWrite(PIN_CVA, HIGH);
  digitalWrite(PIN_CVB, HIGH);
  digitalWrite(PIN_CS, HIGH);
  digitalWrite(PIN_RD, HIGH);

  // 硬體復位
  digitalWrite(PIN_RST, HIGH);
  delayMicroseconds(5);
  digitalWrite(PIN_RST, LOW);
  
  delay(10); 
}

void loop() {
  unsigned long current_ms = millis();

  // 1. 檢查 UI 的過採樣指令
  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    if (input.startsWith("OS:")) {
      int mode = input.substring(3).toInt();
      if (mode >= 0 && mode <= 6) {
        setOversampling(mode);
      }
    }
  }

  // 2. 精準 100Hz 採樣
  if (current_ms - lastSampleTime >= 10) {
    lastSampleTime = current_ms;
    
    int16_t chData[8];

    // 同步觸發 CVA 與 CVB
    digitalWrite(PIN_CVA, LOW);
    digitalWrite(PIN_CVB, LOW);
    delayMicroseconds(2); 
    digitalWrite(PIN_CVA, HIGH);
    digitalWrite(PIN_CVB, HIGH);
    delayMicroseconds(2); 

    // 等待轉換完成
    while (digitalRead(PIN_BUSY) == HIGH) {}

    // 讀取 8 個通道
    digitalWrite(PIN_CS, LOW);
    for (int ch = 0; ch < 8; ch++) {
      digitalWrite(PIN_RD, LOW);
      asm("nop"); // 給硬體一點反應時間 (約 62ns)

      uint8_t rawLowByte  = PINC; 
      uint8_t rawHighByte = PINA; 

      // 【核心修正】：PORTC 不用反轉，PORTA 需要反轉
      uint8_t lowByte  = rawLowByte;               
      uint8_t highByte = reverseBits(rawHighByte); 

      chData[ch] = (int16_t)((highByte << 8) | lowByte);
      digitalWrite(PIN_RD, HIGH);
    }
    digitalWrite(PIN_CS, HIGH);

    // 轉換電壓並輸出
    for (int i = 0; i < 8; i++) {
      float voltage = (chData[i] * 10.0) / 32768.0;
      Serial.print(voltage, 4);
      if (i < 7) Serial.print(",");
    }
    Serial.println();
  }

  // 3. 心跳燈
  if (current_ms - lastLedTime >= 500) {
    lastLedTime = current_ms;
    ledState = !ledState;
    digitalWrite(LED_BUILTIN, ledState);
  }

  // 4. 5秒週期數位輸出 (Pin 12)
  if ((current_ms % 5000) < 2500) {
    digitalWrite(PIN_DIGITAL_OUT, HIGH);
  } else {
    digitalWrite(PIN_DIGITAL_OUT, LOW);
  }
}