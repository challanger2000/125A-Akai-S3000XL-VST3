@echo off
REM Build the S3000XL-VST VST3 against the 0.288 mame_s3000xl libs. Same toolset (14.44, x64 host).
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64 14.44.35207
set MSB="C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe"
cd /d "F:\MPC3000vsti\build_plugin_s3000\Builds\VisualStudio2022"
%MSB% "S3000XL-VST.sln" /p:Configuration=Release /p:Platform=x64 /p:VCToolsVersion=14.44.35207 /p:PreferredToolArchitecture=x64 /m:4 /v:minimal /nologo > F:\MPC3000vsti\build_plugin_s3000\msbuild_vst3.log 2>&1
echo VST3_EXIT=%errorlevel% >> F:\MPC3000vsti\build_plugin_s3000\msbuild_vst3.log
