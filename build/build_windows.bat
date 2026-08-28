@echo off
setlocal enabledelayedexpansion
set CXX=clang++
set PY=python

set ARCH_ARG=

:parse_args
if "%~1"=="" goto args_done
if /I "%~1"=="EVALFILE" (
    shift
    goto take_evalfile_value
)
set "ARG=%~1"
if /I "!ARG:~0,9!"=="EVALFILE=" (
    set "EVALFILE=!ARG:~9!"
    shift
    goto parse_args
)
set "ARCH_ARG=%~1"
shift
goto parse_args

:take_evalfile_value
set "EVALFILE=%~1"
shift
goto parse_args
:args_done

if "%ARCH_ARG%"=="" goto all
if /I "%ARCH_ARG%"=="SSE3" goto sse3
if /I "%ARCH_ARG%"=="SSE4" goto sse4
if /I "%ARCH_ARG%"=="BMI2" goto bmi2
if /I "%ARCH_ARG%"=="AVX2" goto avx2
if /I "%ARCH_ARG%"=="AVX512" goto avx512
echo Invalid argument. Use SSE3, SSE4, BMI2, AVX2, AVX512, or no argument for all.
goto end

:sse3
if exist "sloth_sse3.exe" del "sloth_sse3.exe"
call :build sse3 "-msse3 -mssse3 -march=sandybridge -mtune=sandybridge"
goto end

:sse4
if exist "sloth_sse4.exe" del "sloth_sse4.exe"
call :build sse4 "-msse4.1 -msse4.2 -march=sandybridge -mtune=sandybridge -mssse3 -mno-avx"
goto end

:bmi2
if exist "sloth_bmi2.exe" del "sloth_bmi2.exe"
call :build bmi2 "-march=haswell -msse4.1 -msse4.2 -mbmi -mfma -mavx2 -mbmi2 -mavx"
goto end

:avx2
if exist "sloth_avx2.exe" del "sloth_avx2.exe"
call :build avx2 "-march=haswell -mavx2 -mfma -mtune=haswell -DNN_WITH_AVX2"
goto end

:avx512
if exist "sloth_avx512.exe" del "sloth_avx512.exe"
call :build avx512 "-march=skylake-avx512 -mavx512f -mavx512cd -mavx512bw -mavx512dq -mtune=skylake-avx512"
goto end

:all
call :build sse3 "-msse3 -mssse3 -march=sandybridge -mtune=sandybridge"
call :build sse4 "-msse4.1 -msse4.2 -march=sandybridge -mtune=sandybridge -mssse3 -mno-avx"
call :build bmi2 "-march=haswell -msse4.1 -msse4.2 -mbmi -mfma -mavx2 -mbmi2 -mavx"
call :build avx2 "-march=haswell -mavx2 -mfma -mtune=haswell -DNN_WITH_AVX2"
call :build avx512 "-march=skylake-avx512 -mavx512f -mavx512cd -mavx512bw -mavx512dq -mtune=skylake-avx512"
goto end

:build
echo Building %~1 version...
set ARCH_FLAGS=%~2
set EXTRA_FLAGS=
set NNUE_SRC=

if not "%EVALFILE%"=="" (
    echo Embedding NNUE: %EVALFILE%
    %PY% ../tools/embed_net.py "%EVALFILE%" ../src/embedded_net.cpp
    if errorlevel 1 (
        echo Failed to generate NNUE header.
        exit /b 1
    )
    set EXTRA_FLAGS=-DEVALFILE_EMBEDDED
)

%CXX% -o sloth ../src/glob.cpp ^
    -Ofast -flto -ftree-vectorize -funroll-loops -w ^
    -static -DNDEBUG -finline-functions -pipe -std=c++23 -ffast-math ^
    -fno-rtti -fstrict-aliasing -fomit-frame-pointer -fuse-ld=lld ^
    %ARCH_FLAGS% %EXTRA_FLAGS%

rename sloth sloth_%~1.exe
goto :eof

:end
echo Build process completed
exit