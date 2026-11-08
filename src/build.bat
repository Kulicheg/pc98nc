@echo off
setlocal
set CC=.\ia16-elf\bin\ia16-elf-gcc.exe
if not exist "%CC%" (
  echo ia16-elf-gcc not found at %CC%
  exit /b 1
)
"%CC%" -std=gnu89 -Wall -O1 -mcmodel=small -march=i8086 -fno-strict-aliasing -Wl,-Map=pc98nc.map -o pc98nc.exe main.c vid.c kbd.c dos.c panel.c fileop.c view.c util.c crit.s -li86
exit /b %ERRORLEVEL%
