@echo off
rem  -----------------------------------------------------------------
rem  Build GoldED+ with the Open Watcom toolkit.
rem
rem      gedwcc              Win32
rem      gedwcc dos          32-bit DOS, DOS/4GW
rem      gedwcc os2          OS/2 32-bit
rem      gedwcc linux        Linux 32-bit (cross)
rem      gedwcc clean        remove what the Win32 build made
rem      gedwcc dos clean    likewise for DOS, and so on
rem
rem  Each target keeps its own object and library directories, so they
rem  do not tread on each other.
rem
rem  Set WATCOM beforehand to override where the compiler is found.
rem
rem  INCLUDE is set here per target rather than left to owsetenv: on a
rem  machine that also has Visual C++, INCLUDE is left pointing at that
rem  one's headers machine-wide, and Watcom would compile against them.
rem  -----------------------------------------------------------------
setlocal
if "%WATCOM%"=="" set WATCOM=C:\WATCOM
if not exist "%WATCOM%\binnt\wmake.exe" (
    echo Open Watcom not found in %WATCOM% - set WATCOM to where it is.
    exit /b 1
)
set TGT=
set INC=%WATCOM%\h;%WATCOM%\h\nt
set TARGETS=
:parse
if "%1"=="" goto run
if /i "%1"=="dos"   ( set "TGT=DOS32=1" & set "INC=%WATCOM%\h" & shift & goto parse )
if /i "%1"=="os2"   ( set "TGT=OS2=1"   & set "INC=%WATCOM%\h;%WATCOM%\h\os2" & shift & goto parse )
if /i "%1"=="linux" ( set "TGT=LINUX=1" & set "INC=%WATCOM%\lh" & shift & goto parse )
set TARGETS=%TARGETS% %1
shift
goto parse
:run
set PATH=%WATCOM%\binnt;%SystemRoot%\system32;%SystemRoot%
set INCLUDE=%INC%
set EDPATH=%WATCOM%\eddat
"%WATCOM%\binnt\wmake" -f gedwcc.mak -h WATCOM=%WATCOM% %TGT% %TARGETS%
