AUX_SOURCE_DIRECTORY(./src omxil_src)
AUX_SOURCE_DIRECTORY(./src/codecs/avcenc/ omxil_src_avcenc)
AUX_SOURCE_DIRECTORY(./src/codecs/avcdec/ omxil_src_avcdec)
AUX_SOURCE_DIRECTORY(./src/codecs/aacenc/ omxil_src_aacenc)
AUX_SOURCE_DIRECTORY(./src/codecs/aacdec/ omxil_src_aacdec)
INCLUDE_DIRECTORIES(./inc ./src ../../os/inc
                    ../../core/inc ../../utility/inc
                    ./src/codecs/avcenc
                    ./src/codecs/avcdec
                    ../third_party/libavc/common/
                    ../third_party/libavc/encoder/
                    ../third_party/libavc/decoder/
                    ./src/codecs/aacenc
                    ./src/codecs/aacdec
                    "../third_party/aac/libAACdec/include"
                    "../third_party/aac/libAACenc/include"
                    "../third_party/aac/libPCMutils/include"
                    "../third_party/aac/libFDK/include"
                    "../third_party/aac/libSYS/include"
                    "../third_party/aac/libMpegTPDec/include"
                    "../third_party/aac/libMpegTPEnc/include"
                    "../third_party/aac/libSBRdec/include"
                    "../third_party/aac/libSBRenc/include")
ADD_LIBRARY(libomxil STATIC ${omxil_src} ${omxil_src_avcenc} ${omxil_src_avcdec} ${omxil_src_aacenc} ${omxil_src_aacdec})
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CMAKE_PRJ_CXX_FLAGS} --std=c++11 -Wno-unused-function")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${CMAKE_PRJ_C_FLAGS}")
MESSAGE(INFO " libomxil CMAKE_CXX_FLAGS: ${CMAKE_CXX_FLAGS}")
MESSAGE(INFO " libomxil CMAKE_C_FLAGS: ${CMAKE_C_FLAGS}")
SET_TARGET_PROPERTIES(libomxil PROPERTIES OUTPUT_NAME "omxil")
TARGET_LINK_LIBRARIES(libomxil libos libutility libavcenc libavcdec libFraunhoferAAC)
if (NOT ANDROID)
  SET(CMAKE_CXX_FLAGS "-fPIC ${CMAKE_CXX_FLAGS} -DOS_POSIX -DOS_LINUX")
elseif (ANDROID)
  SET(CMAKE_CXX_FLAGS "-llog ${CMAKE_CXX_FLAGS} -DOS_POSIX -DOS_LINUX")
elseif (WINDOWS)
endif ()

ADD_SUBDIRECTORY(utest)
