AUX_SOURCE_DIRECTORY(./src omxil_src)
AUX_SOURCE_DIRECTORY(./src/codecs/avcenc/ omxil_src_avcenc)
INCLUDE_DIRECTORIES(./inc ./src ./src/codecs/avcenc ../../os/inc
                    ../../core/inc ../../utility/inc
                    ../third_party/libavc/common/
                    ../third_party/libavc/encoder/)
ADD_LIBRARY(libomxil STATIC ${omxil_src} ${omxil_src_avcenc})
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CMAKE_PRJ_CXX_FLAGS} --std=c++11 -Wno-unused-function")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${CMAKE_PRJ_C_FLAGS}")
MESSAGE(INFO " libomxil CMAKE_CXX_FLAGS: ${CMAKE_CXX_FLAGS}")
MESSAGE(INFO " libomxil CMAKE_C_FLAGS: ${CMAKE_C_FLAGS}")
SET_TARGET_PROPERTIES(libomxil PROPERTIES OUTPUT_NAME "omxil")
TARGET_LINK_LIBRARIES(libomxil libos libavcenc)
if (NOT ANDROID)
  SET(CMAKE_CXX_FLAGS "-fPIC ${CMAKE_CXX_FLAGS} -DOS_POSIX -DOS_LINUX")
elseif (ANDROID)
  SET(CMAKE_CXX_FLAGS "-llog ${CMAKE_CXX_FLAGS} -DOS_POSIX -DOS_LINUX")
elseif (WINDOWS)
endif ()

ADD_SUBDIRECTORY(utest)
