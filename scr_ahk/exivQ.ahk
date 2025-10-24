class exivQ {
    __New(exiv2_dll?, exviQ_dll?){
        ;temporary forced assignments
        exiv2_dll ??= "C:\Projects\exivQ\build\exivQ.dll"
        exviQ_dll

        this.exiv2_dll := DllCall("LoadLibrary","Str",exiv2_dll,"Ptr")
        this.exviQ_dll := DllCall("LoadLibrary","Str",exviQ_dll,"Ptr")
    }
}