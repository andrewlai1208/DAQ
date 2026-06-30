// ===================================================
// DAQNeo 穩定字串版：8通道 CSV 方波 + 硬體心跳指示燈
// ===================================================

unsigned long lastSendTime = 0;
unsigned long lastLedTime = 0;
bool ledState = false;

void setup() {
  Serial.begin(115200); 
  pinMode(LED_BUILTIN, OUTPUT);
  while (!Serial) { ; }
}

void loop() {
  unsigned long current_ms = millis();

  // ---------------------------------------------------------
  // 每 10 毫秒 (100Hz) 發送一次。
  // (字串資料量較大，100Hz 是 115200 鮑率下最安全的極限)
  // ---------------------------------------------------------
  if (current_ms - lastSendTime >= 10) {
    lastSendTime = current_ms;

    for (int ch = 0; ch < 8; ch++) {
      // 10秒週期 (10000毫秒)，每個通道錯開 600 毫秒
      unsigned long staggeredTime = current_ms + (ch * 600); 
      
      float val = ((staggeredTime % 10000) < 5000) ? 5.0f : -5.0f;

      // 印出數值 (保留小數點後 2 位)
      Serial.print(val, 2);
      
      // 前 7 個通道加上逗號，最後一個不加
      if (ch < 7) {
        Serial.print(",");
      }
    }
    // 加上換行符號 \r\n 結束這一筆資料
    Serial.println();
  }

  // ---------------------------------------------------------
  // 心跳指示燈 (每 0.5 秒閃爍一次)
  // ---------------------------------------------------------
  if (current_ms - lastLedTime >= 500) {
    lastLedTime = current_ms;
    ledState = !ledState;
    digitalWrite(LED_BUILTIN, ledState);
  }
}