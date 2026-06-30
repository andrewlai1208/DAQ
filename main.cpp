#ifdef __MINGW32__
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
extern "C" int __mingw_vsscanf(const char* s, const char* format, va_list arg) {
    return vsscanf(s, format, arg);
}
#endif

#include <iostream>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "implot.h"
#include "SerialPort.h"
#include <GLFW/glfw3.h>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <ctime>
#include <windows.h> 

// ---- 全域變數與緩衝區設定 ----
const int MAX_POINTS = 1500; 
std::vector<float> x_data(MAX_POINTS);
std::vector<std::vector<float>> y_data(8, std::vector<float>(MAX_POINTS, 0.0f));
bool channel_visible[8] = { true, true, true, true, true, true, true, true };
int current_os_mode = 0;
std::string serial_rx_buffer = "";

double connection_start_time = -1.0; 
int debug_bytes_received = 0; 
int debug_frames_parsed = 0;  

bool y_auto_fit = true;
float custom_y_min = -10.0f;
float custom_y_max = 10.0f;

// ---- 獨立線性校正參數 (直接作為方程式參數) ----
float cal_slope[8] = { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };   // 方程式的 a (預設 1)
float cal_offset[8] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f }; // 方程式的 b (預設 0)
char cal_csv_path[128] = "calibration/cal_params.csv"; 

// 移除原本複雜的迴歸演算法，直接保留載入與儲存功能
void SaveCalibrationCSV(const char* filename) {
    std::ofstream f(filename);
    if (!f.is_open()) return;
    f << "Channel,Slope(a),Offset(b)\n";
    for (int i = 0; i < 8; ++i) {
        f << i + 1 << "," << cal_slope[i] << "," << cal_offset[i] << "\n";
    }
    f.close();
}

void LoadCalibrationCSV(const char* filename) {
    std::ifstream f(filename);
    if (!f.is_open()) return;
    std::string line;
    std::getline(f, line); 
    int ch = 0;
    while (std::getline(f, line) && ch < 8) {
        std::stringstream ss(line);
        std::string part;
        std::getline(ss, part, ','); 
        if (std::getline(ss, part, ',')) cal_slope[ch] = std::stof(part);
        if (std::getline(ss, part, ',')) cal_offset[ch] = std::stof(part);
        ch++;
    }
    f.close();
}

bool is_logging = false;
std::ofstream log_file;
char log_filename[128] = "daq_output.csv"; 

SerialPort* serial = nullptr;
char port_input_buffer[64] = "COM3";

static void glfw_error_callback(int error, const char* description) {
    std::cerr << "Glfw Error " << error << ": " << description << std::endl;
}

void RenderAdcUI() {
    // 1. 底層序列埠通訊與解析邏輯
    if (serial && serial->isConnected()) {
        char readBuffer[4096];
        int bytesRead = 0;

        while ((bytesRead = serial->readData(readBuffer, sizeof(readBuffer) - 1)) > 0) {
            if (is_logging) {
                serial_rx_buffer.append(readBuffer, bytesRead);
                debug_bytes_received += bytesRead;
            }
        }

        if (is_logging) {
            if (connection_start_time < 0) {
                connection_start_time = glfwGetTime();
            }

            size_t next_line_pos;
            while ((next_line_pos = serial_rx_buffer.find('\n')) != std::string::npos) {
                std::string line = serial_rx_buffer.substr(0, next_line_pos);
                serial_rx_buffer.erase(0, next_line_pos + 1);

                if (!line.empty() && line.back() == '\r') line.pop_back();
                if (line.empty()) continue;

                float raw_channels[8];
                int ch_count = 0;
                char* start_ptr = &line[0];
                char* end_ptr = nullptr;

                while (ch_count < 8) {
                    while (*start_ptr == ',' || *start_ptr == ' ' || *start_ptr == '\t') start_ptr++;
                    if (*start_ptr == '\0') break;

                    raw_channels[ch_count] = strtof(start_ptr, &end_ptr);
                    if (start_ptr == end_ptr) break; 
                    
                    ch_count++;
                    start_ptr = end_ptr;
                }

                if (ch_count == 8) {
                    debug_frames_parsed++; 

                    float calibrated_channels[8];
                    for (int i = 0; i < 8; ++i) {
                        // 【核心轉換】：Y = a * X + b
                        calibrated_channels[i] = (raw_channels[i] * cal_slope[i]) + cal_offset[i];
                    }

                    float current_pc_time = (float)(glfwGetTime() - connection_start_time);

                    x_data.erase(x_data.begin());
                    x_data.push_back(current_pc_time);

                    for (int i = 0; i < 8; ++i) {
                        y_data[i].erase(y_data[i].begin());
                        y_data[i].push_back(calibrated_channels[i]);
                    }

                    if (log_file.is_open()) {
                        SYSTEMTIME st;
                        GetLocalTime(&st); 
                        char time_str[64];
                        sprintf(time_str, "%04d-%02d-%02d %02d:%02d:%02d.%03d",
                                st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

                        log_file << time_str;
                        for (int i = 0; i < 8; ++i) log_file << "," << calibrated_channels[i];
                        log_file << "\n";
                    }
                }
            }
            if (serial_rx_buffer.length() > 8192) serial_rx_buffer.clear();
        }
    } else {
        if (is_logging) {
            if (log_file.is_open()) log_file.close();
            is_logging = false;
        }
        connection_start_time = -1.0;
    }

    // 2. 開始繪製 UI
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize, ImGuiCond_Always);
    ImGui::Begin("DAQ", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

    // ---- 左側控制區 ----
    ImGui::BeginChild("ControlPanel", ImVec2(280, 0), true); 
    
    if (ImGui::CollapsingHeader("連線與通道設定", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::InputText("通訊埠", port_input_buffer, IM_ARRAYSIZE(port_input_buffer));
        if (serial == nullptr) {
            if (ImGui::Button("連接", ImVec2(-1, 0))) {
                serial = new SerialPort(port_input_buffer);
                if (!serial->isConnected()) { delete serial; serial = nullptr; }
            }
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
            if (ImGui::Button("中斷連線", ImVec2(-1, 0))) { delete serial; serial = nullptr; }
            ImGui::PopStyleColor();
            
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "[連線狀態診斷]");
            ImGui::Text("累積接收: %d bytes", debug_bytes_received);
            ImGui::Text("成功解碼: %d 筆波形", debug_frames_parsed);
            if (debug_bytes_received > 0 && debug_frames_parsed == 0) {
                ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "⚠️ 收到資料但無法解碼！\n請確認 Arduino 是輸出字串格式。");
            }
        }

        ImGui::Spacing();
        const char* os_modes[] = { "1", "2", "4", "8", "16", "32", "64" };
        if (ImGui::Combo("過採樣", &current_os_mode, os_modes, IM_ARRAYSIZE(os_modes))) {
            if (serial && serial->isConnected()) {
                std::string cmd = "OS:" + std::to_string(current_os_mode) + "\n";
                serial->writeData(cmd.c_str(), cmd.length());
            }
        }

        ImGui::Text("通道顯示:");
        for (int i = 0; i < 8; ++i) {
            if (i % 2 != 0) ImGui::SameLine();
            ImGui::Checkbox((std::string("Ch") + std::to_string(i + 1)).c_str(), &channel_visible[i]);
        }
    }

    if (ImGui::CollapsingHeader("圖表顯示設定", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Y 軸自動縮放 (Auto-Fit)", &y_auto_fit);
        if (!y_auto_fit) {
            ImGui::SetNextItemWidth(80); 
            ImGui::InputFloat("Y最小值", &custom_y_min, 0.0f, 0.0f, "%.1f"); 
            ImGui::SameLine();
            ImGui::SetNextItemWidth(80); 
            ImGui::InputFloat("Y最大值", &custom_y_max, 0.0f, 0.0f, "%.1f");
        }
    }

    // ================== 【全新替換的方程式輸入區】 ==================
    if (ImGui::CollapsingHeader("通道校正方程式 (Y = aX + b)")) {
        static int cal_ch = 0;
        
        ImGui::Combo("設定通道", &cal_ch, "Ch 1\0Ch 2\0Ch 3\0Ch 4\0Ch 5\0Ch 6\0Ch 7\0Ch 8\0");
        
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "X = 校正前原始讀值 (例如: 原始電壓)");
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Y = 校正後最終輸出 (圖表顯示/Log儲存)");
        
        ImGui::Spacing();
        ImGui::SetNextItemWidth(120);
        // 輸入方程式的 a (斜率)
        ImGui::InputFloat("斜率 (a)", &cal_slope[cal_ch], 0.0001f, 0.001f, "%.4f"); 
        
        ImGui::SetNextItemWidth(120);
        // 輸入方程式的 b (平移截距)
        ImGui::InputFloat("平移 (b)", &cal_offset[cal_ch], 0.0001f, 0.001f, "%.4f");

        ImGui::Spacing();
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), ">> 目前: Y = %.4f * X + (%.4f)", cal_slope[cal_ch], cal_offset[cal_ch]);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::InputText("參數檔名", cal_csv_path, sizeof(cal_csv_path));
        if (ImGui::Button("載入參數", ImVec2(100, 0))) { LoadCalibrationCSV(cal_csv_path); }
        ImGui::SameLine();
        if (ImGui::Button("儲存參數", ImVec2(100, 0))) { SaveCalibrationCSV(cal_csv_path); }
    }
    // ==============================================================

    if (ImGui::CollapsingHeader("系統執行與資料儲存", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::InputText("Log 檔名", log_filename, sizeof(log_filename)); 
        
        if (!is_logging) {
            if (ImGui::Button("開始接收與記錄 (Start)", ImVec2(-1, 0))) {
                is_logging = true;
                
                std::string full_log_path = "log/" + std::string(log_filename);
                log_file.open(full_log_path, std::ios::out | std::ios::app);
                if (log_file.is_open()) {
                    log_file << "Time,Ch1,Ch2,Ch3,Ch4,Ch5,Ch6,Ch7,Ch8\n";
                }

                connection_start_time = -1.0;
                serial_rx_buffer.clear();
                debug_bytes_received = 0;
                debug_frames_parsed = 0;

                for (int i = 0; i < MAX_POINTS; ++i) {
                    x_data[i] = (float)(i - MAX_POINTS) * 0.01f; 
                }
                for (int ch = 0; ch < 8; ++ch) {
                    std::fill(y_data[ch].begin(), y_data[ch].end(), 0.0f);
                }
            }
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
            if (ImGui::Button("停止接收與記錄 (Stop)", ImVec2(-1, 0))) {
                if (log_file.is_open()) log_file.close();
                is_logging = false;
            }
            ImGui::PopStyleColor();
            ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "● 系統執行中 (同步寫入 Log...)");
        }
    }

    ImGui::EndChild();
    ImGui::SameLine();

    // ---- 右側疊圖區 ----
    ImGui::BeginChild("PlotPlot", ImVec2(0, 0), false);
    if (ImPlot::BeginPlot("8-Channel 高速實時數據疊圖", ImVec2(-1, -1))) {
        
        ImPlotAxisFlags y_flags = y_auto_fit ? ImPlotAxisFlags_AutoFit : ImPlotAxisFlags_None;
        ImPlot::SetupAxes("時間 (秒)", "數值 (Y)", ImPlotAxisFlags_None, y_flags);
        
        if (!y_auto_fit) {
            ImPlot::SetupAxisLimits(ImAxis_Y1, custom_y_min, custom_y_max, ImGuiCond_Always);
        }

        float latest_x = x_data.back();
        float x_min = (latest_x < 15.0f || !is_logging) ? 0.0f : (latest_x - 15.0f);
        float x_max = (latest_x < 15.0f || !is_logging) ? 15.0f : latest_x;
        
        ImPlot::SetupAxisLimits(ImAxis_X1, x_min, x_max, ImGuiCond_Always);

        for (int i = 0; i < 8; ++i) {
            if (channel_visible[i]) {
                std::string name = "Ch " + std::to_string(i + 1);
                ImPlot::PlotLine(name.c_str(), x_data.data(), y_data[i].data(), MAX_POINTS);
            }
        }
        ImPlot::EndPlot();
    }
    ImGui::EndChild();

    ImGui::End();
}

int main(int argc, char** argv) {
    CreateDirectoryA("calibration", NULL);
    CreateDirectoryA("log", NULL);

    for (int i = 0; i < MAX_POINTS; ++i) {
        x_data[i] = (float)(i - MAX_POINTS) * 0.01f; 
    }

    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) return 1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "DAQNeo 高速資料擷取系統", NULL, NULL);
    if (window == NULL) return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); 

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    
    ImPlot::GetStyle().FitPadding = ImVec2(0.0f, 0.1f); 

    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImFontConfig font_config;
    font_config.MergeMode = false;
    io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\msjh.ttc", 16.0f, &font_config, io.Fonts->GetGlyphRangesChineseFull());

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        RenderAdcUI();

        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.08f, 0.08f, 0.08f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    if (log_file.is_open()) log_file.close();
    if (serial) delete serial;

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}