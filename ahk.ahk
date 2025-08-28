#SingleInstance Force

F1::LButton

#HotIf WinActive("ahk_exe code.exe") || WinActive("ahk_exe vivaldi.exe")
    MButton:: {
        SendText("::")
    }
#HotIf

#HotIf WinActive("ahk_exe javaw.exe") || WinActive("ahk_exe VALORANT-Win64-Shipping.exe") || WinActive(
    "ahk_exe cs2.exe")
    z:: {
        Send("{w down}")
    }
#HotIf
#HotIf WinActive("ahk_exe Brawlhalla.exe")
    RAlt::Space
    o:: {
        Send("+;")
    }
#HotIf

b := false

#HotIf WinActive("ahk_exe javaw.exe")
    XButton2:: {
        global b
        b := !b
        if (b) {
            SetTimer(AutoClick, 55)
        } else {
            SetTimer(AutoClick, 0)
        }
    }
#HotIf

AutoClick() {
    MouseClick()
}

#HotIf WinActive("ahk_exe code.exe")
    !d:: {
        SendText("D3D11")
    }
#Hotif