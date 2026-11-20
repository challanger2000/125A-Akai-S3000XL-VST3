@echo off
REM Force the x64-hosted toolchain (sol.hpp / big PCH exhaust the 32-bit host compiler heap).
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64 14.44.35207
set _CL_=/U_WIN32_WINNT /UNTDDI_VERSION /D_WIN32_WINNT=0x0A00 /DNTDDI_VERSION=0x0A000000
set MSB="C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe"
cd /d F:\MPC3000vsti\References\mame\build\projects\windows\mames3000xl\vs2022
%MSB% mames3000xl.sln /p:Configuration=Release /p:Platform=x64 /p:VCToolsVersion=14.44.35207 /p:PreferredToolArchitecture=x64 /m:2 /v:minimal /nologo > F:\MPC3000vsti\References\mame\msbuild_mame_s3000xl.log 2>&1
echo MSBUILD_EXIT=%errorlevel% >> F:\MPC3000vsti\References\mame\msbuild_mame_s3000xl.log
