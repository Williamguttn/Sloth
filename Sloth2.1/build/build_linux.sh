#!/bin/bash

CXX=${CXX:-clang++}

build_sse3() {
    echo "Building SSE3 version..."
    rm -f sloth_sse3
    $CXX -o sloth ../src/glob.cpp -Ofast -flto -ftree-vectorize -funroll-loops -w \
        -static -DNDEBUG -finline-functions -pipe -std=c++23 -ffast-math -fno-rtti \
        -fstrict-aliasing -fomit-frame-pointer -fuse-ld=lld \
        -msse3 -mssse3 -march=sandybridge -mtune=sandybridge
    mv sloth sloth_sse3
}

build_sse4() {
    echo "Building SSE4 version..."
    rm -f sloth_sse4
    $CXX -o sloth ../src/glob.cpp -Ofast -flto -ftree-vectorize -funroll-loops -w \
        -static -DNDEBUG -finline-functions -pipe -std=c++23 -ffast-math -fno-rtti \
        -fstrict-aliasing -fomit-frame-pointer -fuse-ld=lld \
        -msse4.1 -msse4.2 -march=nehalem -mtune=nehalem
    mv sloth sloth_sse4
}

build_bmi2() {
    echo "Building BMI2 version..."
    rm -f sloth_bmi2
    $CXX -o sloth ../src/glob.cpp -Ofast -flto -ftree-vectorize -funroll-loops -w \
        -static -DNDEBUG -finline-functions -pipe -std=c++23 -ffast-math -fno-rtti \
        -fstrict-aliasing -fomit-frame-pointer -fuse-ld=lld \
        -march=haswell -msse4.1 -msse4.2 -mbmi -mfma -mavx2 -mbmi2 -mavx
    mv sloth sloth_bmi2
}

build_avx2() {
    echo "Building AVX2 version..."
    rm -f sloth_avx2
    $CXX -o sloth ../src/glob.cpp -Ofast -flto -ftree-vectorize -funroll-loops -w \
        -static -DNDEBUG -finline-functions -pipe -std=c++23 -ffast-math -fno-rtti \
        -fstrict-aliasing -fomit-frame-pointer -fuse-ld=lld \
        -mavx2 -march=haswell -mtune=haswell
    mv sloth sloth_avx2
}

build_avx512() {
    echo "Building AVX512 version..."
    rm -f sloth_avx512
    $CXX -o sloth ../src/glob.cpp -Ofast -flto -ftree-vectorize -funroll-loops -w \
        -static -DNDEBUG -finline-functions -pipe -std=c++23 -ffast-math -fno-rtti \
        -fstrict-aliasing -fomit-frame-pointer -fuse-ld=lld \
        -mavx512f -mavx512cd -mavx512bw -mavx512dq -march=skylake-avx512 -mtune=skylake-avx512
    mv sloth sloth_avx512
}

if [ -z "$1" ]; then
    build_sse3
    build_sse4
    build_bmi2
    build_avx2
    build_avx512
else
    case "$1" in
    SSE3|sse3)
        build_sse3
        ;;
    SSE4|sse4)
        build_sse4
        ;;
    BMI2|bmi2)
        build_bmi2
        ;;
    AVX2|avx2)
        build_avx2
        ;;
    AVX512|avx512)
        build_avx512
        ;;
    *)
        echo "Invalid argument. Use SSE3, SSE4, BMI2, AVX2, AVX512, or no argument for all."
        exit 1
        ;;
    esac
fi

echo "Build process completed"