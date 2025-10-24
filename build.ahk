#Requires AutoHotkey v2.0 
#include <Aris/chunjee/adash> ; chunjee/adash@v0.6.0
#include <Aris/skan/runcmd> ; skan/runcmd@aadeb56

;keeps processes locked to one window
DllCall("AllocConsole")

FileDelete(A_ScriptDir "\bin\*.dll")
DirDelete(A_ScriptDir "\build",1)
RunWait("vcpkg install exiv2[bmff,nls,png,xmp] --x-install-root=build --recurse")
RunWait("vcpkg install nlohmann-json --x-install-root=build")
RunWait A_ScriptDir "\build.bat"

FileMove(A_ScriptDir "\build\exivQ.dll",A_ScriptDir "\bin")
FileMove(A_ScriptDir "\build\x64-windows\bin\*.dll",A_ScriptDir "\bin")
; 