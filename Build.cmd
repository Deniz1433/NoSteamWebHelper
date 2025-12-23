@echo off
if not exist "bin" md "bin"
gcc.exe -Oz -Wl,--gc-sections,--exclude-all-symbols -municode -shared -nostdlib -s "DllMain.c" -lntdll -lwtsapi32 -lkernel32 -luser32 -ladvapi32 -lshell32 -o "bin\umpdc.dll"
