cmake_minimum_required(VERSION 3.5)

project(sdr_gnu_radio_app LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# QtCreator supports the following variables for Android, which are identical to qmake Android variables.
# Check http://doc.qt.io/qt-5/deployment-android.html for more information.
# They need to be set before the find_package(Qt5 ...) call.

#if(ANDROID)
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
#    if (ANDROID_ABI STREQUAL "armeabi-v7a")
#        set(ANDROID_EXTRA_LIBS
#            ${CMAKE_CURRENT_SOURCE_DIR}/path/to/libcrypto.so
#            ${CMAKE_CURRENT_SOURCE_DIR}/path/to/libssl.so)
#    endif()
#endif()

find_package(QT NAMES Qt6 Qt5 COMPONENTS Widgets LinguistTools REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Widgets LinguistTools REQUIRED)

set(TS_FILES sdr_gnu_radio_app_en_US.ts)

if(ANDROID)
  add_library(sdr_gnu_radio_app SHARED
    main.cpp
    mainwindow.cpp
    mainwindow.h
    radio.cpp
    radio.h
    waterfall.cpp
    waterfall.h
    mainwindow.ui
    ${TS_FILES}
  )
else()
  add_executable(sdr_gnu_radio_app
    main.cpp
    mainwindow.cpp
    mainwindow.h
    radio.cpp
    radio.h
    waterfall.cpp
    waterfall.h
    mainwindow.ui
    ${TS_FILES}
  )
endif()

target_include_directories(sdr_gnu_radio_app PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/amqpcpp>
    $<INSTALL_INTERFACE:amqpcpp>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/amqpcpp/rabbitmq-c/librabbitmq/>
    $<INSTALL_INTERFACE:amqpcpp/rabbitmq-c/librabbitmq/>
)

add_subdirectory(amqpcpp)
target_include_directories(amqpcpp PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/amqpcpp>
    $<INSTALL_INTERFACE:amqpcpp>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/amqpcpp/rabbitmq-c/librabbitmq/>
    $<INSTALL_INTERFACE:amqpcpp/rabbitmq-c/librabbitmq/>
)

target_link_libraries(sdr_gnu_radio_app PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    amqpcpp
)


qt5_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
