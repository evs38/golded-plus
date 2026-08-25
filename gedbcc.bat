@echo off
rem  -----------------------------------------------------------------
rem  Build GoldED+ with a Borland C++ toolkit.
rem
rem      gedbcc              Win32, Borland C++ 5.5.1   (the usual one)
rem      gedbcc 5            Win32, Borland C++ 5.02
rem      gedbcc dos          32-bit DOS (DPMI), Borland C++ 5.02
rem      gedbcc clean        remove what the 5.5.1 Win32 build made
rem      gedbcc 5 clean      likewise for 5.02, `gedbcc dos clean' for DOS
rem
rem  5.5.1 is the default: it is the later compiler and the one with the
rem  usable C++ library. 5.02 is kept because it is what builds the DOS
rem  target - 5.5.1 is Win32 only - and because it is the compiler this
rem  program was written for.
rem
rem  Each variant keeps its own object and library directories, so they
rem  do not tread on each other and none needs cleaning before the next.
rem
rem  BCROOT is where the compiler is. Left unset, each installer's own
rem  place is tried first - C:\BC55 for 5.5.1, C:\BC5 for 5.02 - and then
rem  C:\BORLAND\BCC55 and C:\BORLAND\BC5, which is where they land when
rem  both are installed under one root.
rem
rem  This wrapper exists because MAKE 5.0 gives up with "Command
rem  arguments too long" as soon as it inherits a present-day Windows
rem  PATH, so the PATH is cut down to what the build needs.
rem  -----------------------------------------------------------------
setlocal
set DOSOPT=
set BC5OPT=
set TARGETS=
set "BCTRY1=C:\BC55"
set "BCTRY2=C:\BORLAND\BCC55"
:parse
if "%1"=="" goto findroot
if /i "%1"=="dos" ( set "DOSOPT=-DDOS32=1" & set "BCTRY1=C:\BC5" & set "BCTRY2=C:\BORLAND\BC5" & shift & goto parse )
if /i "%1"=="5"   ( set "BC5OPT=-DBC5=1" & set "BCTRY1=C:\BC5" & set "BCTRY2=C:\BORLAND\BC5" & shift & goto parse )
if /i "%1"=="502" ( set "BC5OPT=-DBC5=1" & set "BCTRY1=C:\BC5" & set "BCTRY2=C:\BORLAND\BC5" & shift & goto parse )
if /i "%1"=="55"  ( shift & goto parse )
set TARGETS=%TARGETS% %1
shift
goto parse

:findroot
if not "%BCROOT%"=="" goto haveroot
if exist "%BCTRY1%\BIN\bcc32.exe" set "BCROOT=%BCTRY1%"
if "%BCROOT%"=="" if exist "%BCTRY2%\BIN\bcc32.exe" set "BCROOT=%BCTRY2%"
if "%BCROOT%"=="" (
    echo Borland C++ not found in %BCTRY1% or %BCTRY2%.
    echo Set BCROOT to where it is, for example:  set BCROOT=D:\BC55
    exit /b 1
)
:haveroot
if not exist "%BCROOT%\BIN\bcc32.exe" (
    echo Borland C++ not found in %BCROOT% - set BCROOT to where it is.
    exit /b 1
)
set PATH=%BCROOT%\BIN;%SystemRoot%\system32;%SystemRoot%
make -f gedbcc.mak -DBCROOT=%BCROOT% %DOSOPT% %BC5OPT% %TARGETS%
