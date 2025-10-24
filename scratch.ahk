#Requires AutoHotkey v2.0 
#Include <exivQ>
#include <aris/g33kdude/cjson>
#include <Aris/Qriist/Null> ; github:qriist/null@v1.0.0 --main Null.ahk
;temporary forced assignments
exivQ_dll := A_ScriptDir "\bin\exivQ.dll"
exiv2_dll := A_ScriptDir "\bin\exiv2.dll"

exiv := exivQ(exiv2_dll,exivQ_dll)
testfile := A_ScriptDir "\samples\elsie_and_cory.jpg.xmp"

; MsgBox exiv.ExportXmpSingleKey(testfile, &e := 0)
; msgbox A_Clipboard := JSON.dump(exiv.LoadXMP(testfile))
xmpObj := exiv.LoadXMP(testfile)

; msgbox Type(exiv.GetXMPValue(xmpObj,"xmp.null.lol"))
; msgbox exiv.GetXMPValue(xmpObj,"Xmp.dc.subject")
; msgbox exiv.GetXMPValue(xmpObj,"Xmp.tiff.DateTime")
; for k,v in exiv.GetXMPValue(xmpObj,"Xmp.dc.subject")
; for k,v in exiv.GetXMPValue(xmpObj,"Xmp.digiKam.TagsList")
    ; msgbox v
; MsgBox exiv.Exiv2VersionW()
start := A_TickCount
count := 0
loop{
    exiv.LoadXMP(testfile)
    count += 1
} until A_TickCount > (start + 1000)
msgbox count

; xmpKey := "Xmp.tiff.DateTime"
; exiv.SetXMPValue(xmpObj,xmpKey,"lol")

; xmpKey := "Xmp.dc.subject"
; exiv.SetXMPValue(xmpObj,xmpKey,Map("1","blah","2","blech"))

; xmpKey := "Xmp.digiKam.TagsList"
; exiv.SetXMPValue(xmpObj,xmpKey,Map("1","blah","2","blech"))

; xmpKey := "Xmp.sparklegleam.helloworld"
; exiv.SetXMPValue(xmpObj,xmpKey,"yo")


; msgbox xmpValue := JSON.dump(exiv.GetXMPValue(xmpObj,xmpKey))
MsgBox A_Clipboard := JSON.Dump(xmpObj)