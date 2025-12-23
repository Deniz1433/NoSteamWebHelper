@echo off
cd "%~dp0/src"

rd /q /s "bin"
md "bin"

gcc.exe -Oz -Wl,--gc-sections,--exclude-all-symbols -municode -shared -nostdlib -s "Library.c" -lntdll -lwtsapi32 -lkernel32 -luser32 -ladvapi32 -lshell32 -o "bin\umpdc.dll"