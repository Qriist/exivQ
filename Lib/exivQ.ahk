class exivQ {
    __New(exivQ_dll?){
        If IsSet(exivQ_dll){
            SplitPath(exivQ_dll,,&dllDir)
            this.exivQ_dll := exivQ_dll
            this.exiv2_dll := dllDir "\exiv2.dll"
        } else {
            this.aris := dllDir := this._findArisInstallDir("Qriist","exivQ") "\bin" 
            this.exivQ_dll := dllDir "\exivQ.dll"
            this.exiv2_dll := dllDir "\exiv2.dll"
        }
        Critical("On")
        oldWorkingDir := A_WorkingDir 
        A_WorkingDir := dllDir
        this.exiv2_handle := DllCall("LoadLibrary","Str",this.exiv2_dll,"Ptr")
        this.exivQ_handle := DllCall("LoadLibrary","Str",this.exivQ_dll,"Ptr")
        A_WorkingDir := oldWorkingDir
        Critical("Off")
    }

    Exiv2VersionW(){
        ret := DllCall(this.exivQ_dll "\Exiv2VersionW","WStr")
        return ret
    }
    ReadXmpFile(inputFile,&ErrorCode){
        static ReadXmpFile := this._getDllAddress(this.exivQ_dll,"ReadXmpFile") 
        
        ErrorCode := DllCall(ReadXmpFile,"WStr",inputFile,"Ptr*",&retBuf := 0)
        ret := StrGet(retBuf,"UTF-16")
        this.FreeXmpFile(retBuf)
        return ret
    }
    FreeXmpFile(bufToFree){
        static FreeXmpFile := this._getDllAddress(this.exivQ_dll,"FreeXmpFile")
        DllCall(FreeXmpFile,"Ptr",bufToFree)
    }
    ExportXmpSingleKey(inputFile,&ErrorCode){
        static ExportXmpSingleKey := this._getDllAddress(this.exivQ_dll,"ExportXmpSingleKey") 
        
        ErrorCode := DllCall(ExportXmpSingleKey,"WStr",inputFile,"Ptr*",&retBuf := 999,"Int")
        msgbox retbuf ErrorCode
        ret := StrGet(retBuf,"UTF-16")
        ; this.ExportXmpSingleKey(retBuf)
        return ret
    }
    LoadXMP(inputFile){
        static ReadXmpPropertiesJSON := this._getDllAddress(this.exivQ_dll,"ReadXmpPropertiesJSON") 
        ret := DllCall(ReadXmpPropertiesJSON, "AStr", inputFile, "Cdecl Ptr")
        return JSON.Load(StrGet(ret, "UTF-8"))
    }
    SaveXMP(){

    }
    GetXMPValue(xmpObj,key){
        If !xmpObj.has(key)
            return Null()
        
        switch xmpObj[key]["type"] {
            case "XmpText":
                return xmpObj[key]["value"]
            case "XmpBag":
                retObj := Map()
                for k,v in xmpObj[key]["value"]
                    retObj[k] := v["value"]
                return retObj
            case "XmpSeq":
                retObj := []
                for k,v in xmpObj[key]["value"]
                    retObj.Push(v["value"])
                return retObj
            default:
                return xmpObj[key]
        }
    }
    SetXMPValue(xmpObj,xmpKey,keyValue){
        ;check to see if key exists
        If !xmpObj.has(xmpKey){
            xmpObj[xmpKey] := Map()
        }
        keyObj := xmpObj[xmpKey]
        Switch (Type(keyValue)){
            case "String","Integer":    ;Exiv2 lumps numbers in here
                keyObj["type"] := "XmpText"
                keyObj["value"] := keyValue . "" ;empty string append to force string type
            case "Array":
                keyObj["type"] := "XmpSeq"
                keyObj["value"] := sub := Map()
                for k,v in keyValue{
                    s := sub[a_index] := Map()
                    s["type"] := "XmpText"
                    s["value"] := v
                }
            case "Map":
                keyObj["type"] := "XmpBag"
                keyObj["value"] := sub := Map()
                for k,v in keyValue{
                    s := sub[a_index] := Map()
                    s["type"] := "XmpText"
                    s["value"] := v
                }
            case "Null":
                keyObj["type"] := "XmpNull"
                keyObj["value"] := Null()
        }
    }

    _getDllAddress(dllPath,dllfunction){
        return DllCall("GetProcAddress", "Ptr", DllCall("GetModuleHandle", "Str", dllPath, "Ptr"), "AStr", dllfunction, "Ptr")
    }
    _findArisInstallDir(user,packageName){ ;dynamically finds a local versioned Aris installation
      If DirExist(A_ScriptDir "\lib\Aris\" user) ;"top level" install
         packageDir := A_ScriptDir "\lib\Aris\" user
      else if DirExist(A_ScriptDir "\..\lib\Aris\" user) ;script one level down
         packageDir := A_ScriptDir "\..\lib\Aris\" user
      else
         return ""
      loop files (packageDir "\" packageName "@*") , "D"{
         ;should end up with the latest installation
         ArisDir := packageDir "\" A_LoopFileName
      }
      return ArisDir
    }

}