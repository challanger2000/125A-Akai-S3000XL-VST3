@echo off
REM Generate VS2022 project for s3000xl on the 0.288 tree (References/mame).
REM s3000.cpp contains s2000/s3000/cd3000i/s3000xl/cd3000xl; the plugin driver_list picks s3000xl.
set MSYSTEM=MINGW64
set OS=Windows_NT
set MINGW_PREFIX=/mingw64
"C:\Users\Public\msys64\usr\bin\bash.exe" -lc "export PATH=/mingw64/bin:$PATH; export OS=Windows_NT; export MINGW_PREFIX=/mingw64; cd /f/MPC3000vsti/References/mame && make vs2022 SUBTARGET=s3000xl SOURCES=src/mame/akai/s3000.cpp NOWERROR=1" > F:\MPC3000vsti\References\mame\genie_s3000xl.log 2>&1
echo GENIE_EXIT=%errorlevel% >> F:\MPC3000vsti\References\mame\genie_s3000xl.log
