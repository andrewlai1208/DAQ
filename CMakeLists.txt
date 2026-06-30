cmake_minimum_required(VERSION 3.10)
project(DAQNeo_GUI CXX)

set(CMAKE_CXX_STANDARD 11)

# 1. 收集 ImGui 與 ImPlot 原始碼
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

# 4. 手動設定 GLFW 函式庫路徑 (注意：改成 lib-mingw)
target_include_directories(DAQNeo_GUI PRIVATE 
    "3rdparty/glfw/include"
)
target_link_directories(DAQNeo_GUI PRIVATE 
    "3rdparty/glfw/lib-mingw"
)

# 5. 連結圖形加速庫 (強制綁定絕對路徑，保證抓得到)
target_link_libraries(DAQNeo_GUI PRIVATE 
    "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/glfw/lib-mingw-w32/libglfw3.a"
    opengl32        
    gdi32           
)
# 6. 舊版 MinGW 時間函式相容性補丁 (透過強制包含標頭檔注入)
if(MINGW)
    target_compile_options(DAQNeo_GUI PRIVATE
        "-include" "${CMAKE_CURRENT_SOURCE_DIR}/mingw_time_patch.h"
    )
endif()