@echo off
REM See Bazel's workspace status command
REM (https://bazel.build/docs/user-manual#workspace-status).
for /f %%i in ('git describe --tags --long --always --dirty 2^>nul') do (
  echo Stamping build info: BUILD_CHANGELIST=%%i >&2
  echo BUILD_CHANGELIST %%i
  exit /b 0
)
echo Stamping build info: BUILD_CHANGELIST=unknown >&2
echo BUILD_CHANGELIST
