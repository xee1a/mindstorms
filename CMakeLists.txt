cmake_minimum_required(VERSION 3.21)
project(NXTStudio VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Widgets Svg SvgWidgets LinguistTools Test)
qt_standard_project_setup(REQUIRES 6.8 I18N_SOURCE_LANGUAGE en I18N_TRANSLATED_LANGUAGES pl)

if(MSVC)
    add_compile_options(/W4 /permissive- /utf-8)
endif()

# Pure logic shared by the app and the tests.
add_library(nxtcore STATIC
    src/nxt/Protocol.cpp src/nxt/Protocol.h
    src/drive/Mixer.cpp src/drive/Mixer.h
)
target_include_directories(nxtcore PUBLIC src)
target_link_libraries(nxtcore PUBLIC Qt6::Core)

qt_add_executable(NXTStudio WIN32
    src/main.cpp
    src/app/MainWindow.cpp src/app/MainWindow.h
    src/app/Theme.cpp src/app/Theme.h
    src/app/Widgets.cpp src/app/Widgets.h
    src/blocks/Block.cpp src/blocks/Block.h
    src/blocks/BlockDefs.cpp src/blocks/BlockDefs.h
    src/blocks/Canvas.cpp src/blocks/Canvas.h
    src/blocks/Palette.cpp src/blocks/Palette.h
    src/blocks/Workspace.cpp src/blocks/Workspace.h
    src/drive/Gamepad.cpp src/drive/Gamepad.h
    src/nxt/Brick.cpp src/nxt/Brick.h
    src/nxt/DemoTransport.cpp src/nxt/DemoTransport.h
    src/nxt/Transport.h
    src/nxt/WinUsbTransport.cpp src/nxt/WinUsbTransport.h
    src/views/BrickView.cpp src/views/BrickView.h
    src/views/DriveView.cpp src/views/DriveView.h
    src/views/ProgramView.cpp src/views/ProgramView.h
    resources/app.rc
)
target_compile_definitions(NXTStudio PRIVATE NXTSTUDIO_VERSION="${PROJECT_VERSION}" UNICODE _UNICODE NOMINMAX)
target_link_libraries(NXTStudio PRIVATE nxtcore Qt6::Widgets Qt6::Svg Qt6::SvgWidgets setupapi winusb xinput dwmapi ole32)

file(GLOB ICONS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} CONFIGURE_DEPENDS resources/icons/*.svg)
qt_add_resources(NXTStudio icons PREFIX / BASE resources FILES ${ICONS} resources/images/nxt.png)

# Polish UI text. `cmake --build build --target update_translations` refreshes the .ts from the sources.
qt_add_translations(TARGETS NXTStudio TS_FILES i18n/nxtstudio_pl.ts RESOURCE_PREFIX /i18n)

# One-off asset tool (not built by default): turns a photo on white into a transparent PNG.
#   cmake --build build --config Release --target cutout
add_executable(cutout EXCLUDE_FROM_ALL tools/cutout.cpp)
target_link_libraries(cutout PRIVATE Qt6::Gui)

enable_testing()
qt_add_executable(test_core tests/test_core.cpp)
target_link_libraries(test_core PRIVATE nxtcore Qt6::Test)
add_test(NAME test_core COMMAND test_core)
