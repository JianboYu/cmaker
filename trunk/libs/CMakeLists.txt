SET(Src_Lib lib.cpp)
INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/inc)
ADD_LIBRARY(libcmake STATIC ${Src_Lib})
MESSAGE(INFO " lib CMAKE_EXE_LINKER_FLAGS: ${CMAKE_EXE_LINKER_FLAGS}")
SET_TARGET_PROPERTIES(libcmake PROPERTIES OUTPUT_NAME "libcamke")
