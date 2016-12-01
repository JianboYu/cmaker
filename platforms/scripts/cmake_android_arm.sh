#!/bin/sh
cd `dirname $0`/../../

mkdir -p build_android_arm
cd build_android_arm

cmake -DCMAKE_BUILD_WITH_INSTALL_RPATH=ON -DANDROID_ABI=armeabi-v7a -DANDROID_NATIVE_API_LEVEL=android-21 -DCMAKE_TOOLCHAIN_FILE=../platforms/android/android.toolchain.cmake $@ ../
