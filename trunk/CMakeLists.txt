cmake_minimum_required(VERSION 3.6)

PROJECT(TestCMAKE)

MESSAGE(INFO " PROJECT_BINARY_DIR: ${PROJECT_BINARY_DIR}")
MESSAGE(INFO " PROJECT_SOURCE_DIR: ${PROJECT_SOURCE_DIR}")

ADD_SUBDIRECTORY(src)
ADD_SUBDIRECTORY(libs)

MESSAGE(INFO " CMAKE_SYSTEM: ${CMAKE_SYSTEM}")
MESSAGE(INFO " CMAKE_SYSTEM_NAME: ${CMAKE_SYSTEM_NAME}")
MESSAGE(INFO " CMAKE_SYSTEM_PROCESSOR: ${CMAKE_SYSTEM_PROCESSOR}")
MESSAGE(INFO " IS ANDROID: ${ANDROID}")
