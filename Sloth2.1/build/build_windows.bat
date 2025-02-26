@echo off
set CXX=clang++

if "%1"=="" goto all
if /I "%1"=="SSE3" goto sse3
if /I "%1"=="SSE4" goto sse4
if /I "%1"=="BMI2" goto bmi2
if /I "%1"=="AVX2" goto avx2
if /I "%1"=="AVX512" goto avx512
if /I "%1"=="ARM" goto arm
echo Invalid argument. Use SSE3, SSE4, BMI2, AVX2, AVX512, ARM, or no argument for all.
goto end

:sse3
if exist "sloth_sse3.exe" del "sloth_sse3.exe"
call :build_sse3
goto end

:sse4
if exist "sloth_sse4.exe" del "sloth_sse4.exe"
call :build_sse4
goto end

:bmi2
if exist "sloth_bmi2.exe" del "sloth_bmi2.exe"
call :build_bmi2
goto end

:avx2
if exist "sloth_avx2.exe" del "sloth_avx2.exe"
call :build_avx2
goto end

:avx512
if exist "sloth_avx512.exe" del "sloth_avx512.exe"
call :build_avx512
goto end

:arm
if exist "sloth_arm.exe" del "sloth_arm.exe"
call :build_arm
goto end

:all
call :build_sse3
call :build_sse4
call :build_bmi2
call :build_avx2
call :build_avx512
call :build_arm
goto end

:build_sse3
echo Building SSE3 version...
%CXX% -o sloth ../src/glob.cpp -Ofast -flto -ftree-vectorize -funroll-loops -w ^
-static -DNDEBUG -finline-functions -pipe -std=c++23 -ffast-math -fno-rtti -fstrict-aliasing -fomit-frame-pointer -fuse-ld=lld ^
-msse3 -mssse3 -march=sandybridge -mtune=sandybridge
rename sloth sloth_sse3.exe
goto :eof

:build_sse4
echo Building SSE4 version...
%CXX% -o sloth ../src/glob.cpp -Ofast -flto -ftree-vectorize -funroll-loops -w ^
-static -DNDEBUG -finline-functions -pipe -std=c++23 -ffast-math -fno-rtti -fstrict-aliasing -fomit-frame-pointer -fuse-ld=lld ^
-msse4.1 -msse4.2 -march=nehalem -mtune=nehalem
rename sloth sloth_sse4.exe
goto :eof

:build_bmi2
echo Building BMI2 version...
%CXX% -o sloth ../src/glob.cpp -Ofast -flto -ftree-vectorize -funroll-loops -w ^
-static -DNDEBUG -finline-functions -pipe -std=c++23 -ffast-math -fno-rtti -fstrict-aliasing -fomit-frame-pointer -fuse-ld=lld ^
-march=haswell -msse4.1 -msse4.2 -mbmi -mfma -mavx2 -mbmi2 -mavx
rename sloth sloth_bmi2.exe
goto :eof

:build_avx2
echo Building AVX2 version...
%CXX% -o sloth ../src/glob.cpp -Ofast -flto -ftree-vectorize -funroll-loops -w ^
-static -DNDEBUG -finline-functions -pipe -std=c++23 -ffast-math -fno-rtti -fstrict-aliasing -fomit-frame-pointer -fuse-ld=lld ^
-march=haswell -mavx2 -mfma -mtune=haswell
rename sloth sloth_avx2.exe
goto :eof

:build_avx512
echo Building AVX512 version...
%CXX% -o sloth ../src/glob.cpp -Ofast -flto -ftree-vectorize -funroll-loops -w ^
-static -DNDEBUG -finline-functions -pipe -std=c++23 -ffast-math -fno-rtti -fstrict-aliasing -fomit-frame-pointer -fuse-ld=lld ^
-march=skylake-avx512 -mavx512f -mavx512cd -mavx512bw -mavx512dq -mtune=skylake-avx512
rename sloth sloth_avx512.exe
goto :eof

:end
echo Build process completed
exit