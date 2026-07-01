cmake_minimum_required(VERSION 3.10)
project(DAQNeo_GUI CXX)

set(CMAKE_CXX_STANDARD 11)

# 1. 收集 ImGui 與 ImPlot 原始碼
# 注意：官方 CMake 建議將源代碼手動列出，但若專案不大，使用 GLOB 較為方便
file(GLOB IMGUI_SRCS "imgui-master/*.cpp" "imgui-master/backends/imgui_impl_glfw.cpp" "imgui-master/backends/imgui_impl_opengl3.cpp")
file(GLOB IMPLOT_SRCS "implot-master/*.cpp")

# 2. 建立執行檔
add_executable(DAQNeo_GUI 
    main.cpp 
    ${IMGUI_SRCS} 
    ${IMPLOT_SRCS}
)

# 3. 設定內部專案標頭檔路徑
target_include_directories(DAQNeo_GUI PRIVATE 
    .
    imgui-master
    imgui-master/backends
    implot-master
)

# 4. 手動設定 GLFW 函式庫路徑
target_include_directories(DAQNeo_GUI PRIVATE 
    "3rdparty/glfw/include"
)

# 5. 連結圖形加速庫與 GLFW
# 提示：因為這裡已經綁定絕對路徑，所以原本的 target_link_directories 可以省略，避免路徑衝突
target_link_libraries(DAQNeo_GUI PRIVATE 
    "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/glfw/lib-mingw-w32/libglfw3.a"
    opengl32        
    gdi32           
)

# 6. MinGW 專屬設定與編譯補丁
if(MINGW)
    # (A) 舊版 MinGW 時間函式相容性補丁
    target_compile_options(DAQNeo_GUI PRIVATE
        "-include" "${CMAKE_CURRENT_SOURCE_DIR}/mingw_time_patch.h"
    )
    
    # (B) 加入靜態編譯參數：讓程式打包後可獨立執行，不再跳出缺少 libstdc++-6.dll 錯誤
    target_link_options(DAQNeo_GUI PRIVATE 
        -static -static-libgcc -static-libstdc++
    )
endif()