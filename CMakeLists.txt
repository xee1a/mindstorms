cmake_minimum_required(VERSION 3.21)
project(NXTStudio VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Widgets Svg SvgWidgets SerialPort LinguistTools Test)
qt_standard_project_setup(REQUIRES 6.8 I18N_SOURCE_LANGUAGE en I18N_TRANSLATED_LANGUAGES pl)

if(MSVC)
    add_compile_options(/W4 /permissive- /utf-8)
endif()

# Pure logic shared by the app and the tests.
add_library(nxtcore STATIC
    src/nxt/Protocol.cpp src/nxt/Protocol.h
    src/nxt/SystemFiles.cpp src/nxt/SystemFiles.h
    src/drive/Mixer.cpp src/drive/Mixer.h
    src/drive/PadLink.cpp src/drive/PadLink.h
    src/blocks/BlockDefs.cpp src/blocks/BlockDefs.h
    src/compile/NxcGenerator.cpp src/compile/NxcGenerator.h
)
target_include_directories(nxtcore PUBLIC src)
target_link_libraries(nxtcore PUBLIC Qt6::Core Qt6::Gui)

qt_add_executable(NXTStudio WIN32
    src/main.cpp
    src/app/FactoryResetDialog.cpp src/app/FactoryResetDialog.h
    src/app/MainWindow.cpp src/app/MainWindow.h
    src/app/Prefs.cpp src/app/Prefs.h
    src/app/SettingsDialog.cpp src/app/SettingsDialog.h
    src/app/Splash.cpp src/app/Splash.h
    src/app/Sounds.cpp src/app/Sounds.h
    src/app/Theme.cpp src/app/Theme.h
    src/app/Widgets.cpp src/app/Widgets.h
    src/blocks/Block.cpp src/blocks/Block.h
    src/blocks/Canvas.cpp src/blocks/Canvas.h
    src/blocks/Palette.cpp src/blocks/Palette.h
    src/blocks/Workspace.cpp src/blocks/Workspace.h
    src/compile/Compiler.cpp src/compile/Compiler.h
    src/drive/Gamepad.cpp src/drive/Gamepad.h
    src/nxt/Brick.cpp src/nxt/Brick.h
    src/nxt/SerialTransport.cpp src/nxt/SerialTransport.h
    src/nxt/Transport.h
    src/nxt/WinUsbTransport.cpp src/nxt/WinUsbTransport.h
    src/run/Runner.cpp src/run/Runner.h
    src/views/BrickView.cpp src/views/BrickView.h
    src/views/ControllerView.cpp src/views/ControllerView.h
    src/views/ProgramView.cpp src/views/ProgramView.h
    src/views/UploadDialog.cpp src/views/UploadDialog.h
    resources/app.rc
)
target_compile_definitions(NXTStudio PRIVATE NXTSTUDIO_VERSION="${PROJECT_VERSION}" UNICODE _UNICODE NOMINMAX)
target_link_libraries(NXTStudio PRIVATE nxtcore Qt6::Widgets Qt6::Svg Qt6::SvgWidgets Qt6::SerialPort setupapi winusb xinput dinput8 dxguid dwmapi ole32 winmm)

file(GLOB ICONS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} CONFIGURE_DEPENDS resources/icons/*.svg)
qt_add_resources(NXTStudio icons PREFIX / BASE resources FILES ${ICONS})
qt_add_resources(NXTStudio images PREFIX / BASE resources FILES resources/images/nxt.png resources/images/pad_mangan200.png)

# The bundled NXC compiler (see third_party/nbc/README.md) goes to tools/nbc.exe next to the exe.
add_custom_command(TARGET NXTStudio POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:NXTStudio>/tools
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/third_party/nbc/nbc.exe $<TARGET_FILE_DIR:NXTStudio>/tools/nbc.exe)

# Polish UI text. `cmake --build build --target update_translations` refreshes the .ts from the sources.
qt_add_translations(TARGETS NXTStudio TS_FILES i18n/nxtstudio_pl.ts RESOURCE_PREFIX /i18n)

enable_testing()
qt_add_executable(test_core tests/test_core.cpp)
target_link_libraries(test_core PRIVATE nxtcore Qt6::Test)
add_test(NAME test_core COMMAND test_core)

# Generates NXC for every block and compiles it with the bundled nbc.
qt_add_executable(test_compile tests/test_compile.cpp)
target_link_libraries(test_compile PRIVATE nxtcore Qt6::Test)
add_test(NAME test_compile COMMAND test_compile)
set_tests_properties(test_compile PROPERTIES ENVIRONMENT "NBC=${CMAKE_CURRENT_SOURCE_DIR}/third_party/nbc/nbc.exe")
