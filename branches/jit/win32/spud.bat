@rem Super Penn Universal Development environment.
@echo off

echo %~nx0%
if "%1%"=="init" (
  @rem Minimal checks that we don't try to clobber an existing checkout.
  if exist %cd%\win32\%~nx0% (
    echo ERROR: %cd% appears to already have a PennMUSH checkout!
    exit /b
  )

  echo Setting up a PennMUSH checkout in %cd%...
  git clone https://github.com/zetafunction/pennmush.git .
  git clone https://chromium.googlesource.com/chromium/deps/python_26 third_party/python_26
  third_party\python_26\python spud.py init
  exit /b
)

python %~dp0%\spud.py %*

if errorlevel 1 (
  echo ERROR: spud environment not initialized?
  exit /b
)
