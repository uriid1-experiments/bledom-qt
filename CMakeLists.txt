cmake_minimum_required(VERSION 3.21)
project(bledom-qt VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Bluetooth LinguistTools)
qt_standard_project_setup(I18N_TRANSLATED_LANGUAGES ru)

qt_add_executable(bledom-qt
    src/main.cpp
    src/BledomDevice.h
    src/BledomDevice.cpp
    src/Language.h
    src/Language.cpp
    src/MainWindow.h
    src/MainWindow.cpp
)

# Translations: i18n/bledom-qt_ru.ts -> embedded as :/i18n/bledom-qt_ru.qm.
# Run `cmake --build build --target update_translations` after changing tr() strings.
qt_add_translations(bledom-qt
    TS_FILE_DIR i18n
    RESOURCE_PREFIX "/i18n"
    NO_GENERATE_PLURALS_TS_FILE
)

target_link_libraries(bledom-qt PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Bluetooth)

install(TARGETS bledom-qt RUNTIME DESTINATION bin)
