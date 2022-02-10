@echo off
cls
type BATCHLST.TXT
choice /C:12345670 /N Please select what to build:
echo.
if ERRORLEVEL 8 goto end
if ERRORLEVEL 1 set TARGET=HEXRBETA
if ERRORLEVEL 2 set TARGET=HEXDEMOA
if ERRORLEVEL 3 set TARGET=HEX10A
if ERRORLEVEL 4 set TARGET=HEX10B
if ERRORLEVEL 5 set TARGET=HEXDEMOB
if ERRORLEVEL 6 set TARGET=HEX11A
if ERRORLEVEL 7 set TARGET=HEX11B

if "%1" == "USE_APODMX" goto apodmx

if not exist ..\dmx\dmx33gs goto dmxerror
if not exist ..\dmx\dmx34a goto dmxerror
set USE_APODMX=0
goto task

:dmxerror
echo Can't recreate Hexen EXE, you need a compatible version of the
echo DMX headers under ..\dmx. You also need a compatible version of
echo the DMX library, again under ..\dmx.
echo Alternatively, run "DOBUILD.BAT USE_APODMX" to use APODMX instead.
goto end

:apodmx
if not exist ..\apodmx\apodmx.lib goto apodmxerror
REM AUDIO_WF.LIB is copied as a workaround for too long path
set USE_APODMX=1
goto task

:apodmxerror
echo Can't recreate Hexen EXE, you need the APODMX headers
echo and the APODMX.LIB file under ..\apodmx ready, as well
echo as ..\audiolib\origexes\109\AUDIO_WF.LIB.
goto end

:task
REM Since environment variables may actually impact the compiler output,
REM use a helper script in order to try and refrain from them.
REM There was also an attempt to pass an additional argument
REM to wmake, EXTERNOPT=/dNORANGECHECKING, but this
REM leaded to a too long argument list for wcc386.

mkdir %TARGET%
echo wmake.exe %TARGET%\hex.exe "appver_exedef = %TARGET%" "use_apodmx = %USE_APODMX%" > BUILDTMP.BAT
echo del BUILDTMP.BAT >> BUILDTMP.BAT
set TARGET=
set USE_APODMX=
BUILDTMP.BAT

:end
set TARGET=
