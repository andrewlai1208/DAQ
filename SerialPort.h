#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>
#include <vector>

class SerialPort {
private:
    HANDLE hSerial;
    bool connected;

public:
    // 建構子：傳入連線埠名稱（例如 "COM3" 或 "\\\\.\\COM10"）
    SerialPort(const char* portName) {
        connected = false;
        
        // 針對大於 COM9 的連接埠，標準 Windows API 需要加上 \\.\ 前綴
        std::string targetPort = portName;
        if (targetPort.find("\\\\.\\") == std::string::npos && targetPort.length() > 4) {
            targetPort = "\\\\.\\" + targetPort;
        }

        // 開啟序列埠
        hSerial = CreateFileA(targetPort.c_str(), 
                              GENERIC_READ | GENERIC_WRITE, 
                              0, 
                              NULL, 
                              OPEN_EXISTING, 
                              FILE_ATTRIBUTE_NORMAL, 
                              NULL);

        if (hSerial != INVALID_HANDLE_VALUE) {
            DCB dcbSerialParams = { 0 };
            dcbSerialParams.DCBlength = sizeof(dcbSerialParams);

            // 取得目前的序列埠配置
            if (GetCommState(hSerial, &dcbSerialParams)) {
                dcbSerialParams.BaudRate = CBR_115200; // 設定波特率為 115200
                dcbSerialParams.ByteSize = 8;          // 8 位資料位
                 dcbSerialParams.StopBits = ONESTOPBIT;  // 1 位停止位
                dcbSerialParams.Parity = NOPARITY;      // 無校驗位

                // 寫入新配置
                if (SetCommState(hSerial, &dcbSerialParams)) {
                    connected = true;
                    
                    // 設定逾時（Timeouts），防止 ReadFile 在沒有資料時把主執行緒卡死
                    COMMTIMEOUTS timeouts = { 0 };
                    timeouts.ReadIntervalTimeout = MAXDWORD;          // 字元間最大間隔 (ms)
                    timeouts.ReadTotalTimeoutConstant = 0;     // 讀取常數逾時 (ms)
                    timeouts.ReadTotalTimeoutMultiplier = 0;    // 讀取每字元乘數 (ms)
                    timeouts.WriteTotalTimeoutConstant = 10;
                    timeouts.WriteTotalTimeoutMultiplier = 1;
                    SetCommTimeouts(hSerial, &timeouts);
                }
            }
        }
    }

    // 解構子：自動關閉序列埠連線
    ~SerialPort() {
        if (connected) {
            CloseHandle(hSerial);
        }
    }

    // 讀取資料：傳入快取區與預計讀取長度，返回實際讀到的位元組數
    int readData(char* buffer, unsigned int nbChar) {
        DWORD bytesRead = 0;
        ClearCommError(hSerial, NULL, NULL);
        
        if (ReadFile(hSerial, buffer, nbChar, &bytesRead, NULL)) {
            return bytesRead;
        }
        return 0;
    }

    // 發送資料：發送控制字串給 Arduino（例如過採樣指令）
    bool writeData(const char* buffer, unsigned int nbChar) {
        DWORD bytesSent = 0;
        if (WriteFile(hSerial, (void*)buffer, nbChar, &bytesSent, NULL)) {
            return true;
        }
        return false;
    }

    // 檢查目前連線狀態
    bool isConnected() { 
        return connected; 
    }
};