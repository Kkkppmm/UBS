; USBForge Windows Installer (NSIS)
; Build: makensis packaging/windows/usbforge.nsi

!define APP_NAME "USBForge"
!define APP_VERSION "1.5.0"
!define APP_PUBLISHER "USBForge Contributors"
!define APP_EXE "USBForge.exe"
!define APP_UNINST "Uninstall.exe"

Name "${APP_NAME}"
OutFile "..\..\build\release\USBForge-${APP_VERSION}-windows-x64-setup.exe"
InstallDir "$PROGRAMFILES64\USBForge"
InstallDirRegKey HKLM "Software\USBForge" "InstallDir"
RequestExecutionLevel admin
SetCompressor /SOLID lzma
Unicode true

!include "MUI2.nsh"

!define MUI_ABORTWARNING
!define MUI_ICON "${NSISDIR}\Contrib\Graphics\Icons\modern-install.ico"
!define MUI_UNICON "${NSISDIR}\Contrib\Graphics\Icons\modern-uninstall.ico"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "..\..\LICENSE"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

Section "USBForge (required)" SecMain
  SectionIn RO
  SetOutPath "$INSTDIR"

  File "..\..\build\windows\USBForge.exe"
  File "..\..\scripts\write-iso.ps1"
  File "..\..\LICENSE"
  File "..\..\README.md"

  SetOutPath "$INSTDIR\docs"
  File /r "..\..\docs\*.*"

  SetOutPath "$INSTDIR\scripts"
  File "..\..\scripts\write-iso.ps1"

  ; Also place helper next to EXE for easy discovery
  SetOutPath "$INSTDIR"
  File "..\..\scripts\write-iso.ps1"

  WriteRegStr HKLM "Software\USBForge" "InstallDir" "$INSTDIR"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\USBForge" \
                   "DisplayName" "${APP_NAME} ${APP_VERSION}"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\USBForge" \
                   "DisplayVersion" "${APP_VERSION}"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\USBForge" \
                   "Publisher" "${APP_PUBLISHER}"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\USBForge" \
                   "UninstallString" "$INSTDIR\${APP_UNINST}"
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\USBForge" \
                   "NoModify" 1
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\USBForge" \
                   "NoRepair" 1

  WriteUninstaller "$INSTDIR\${APP_UNINST}"

  CreateDirectory "$SMPROGRAMS\USBForge"
  CreateShortCut "$SMPROGRAMS\USBForge\USBForge.lnk" "$INSTDIR\${APP_EXE}"
  CreateShortCut "$SMPROGRAMS\USBForge\USBForge Builder.lnk" "$INSTDIR\${APP_EXE}" "" "$INSTDIR\${APP_EXE}" 0
  CreateShortCut "$SMPROGRAMS\USBForge\Check for Updates.lnk" "https://github.com/Kkkppmm/UBS/releases"
  CreateShortCut "$SMPROGRAMS\USBForge\Docs.lnk" "$INSTDIR\docs"
  CreateShortCut "$SMPROGRAMS\USBForge\Uninstall.lnk" "$INSTDIR\${APP_UNINST}"
  CreateShortCut "$DESKTOP\USBForge.lnk" "$INSTDIR\${APP_EXE}"
SectionEnd

Section "Uninstall"
  Delete "$INSTDIR\${APP_EXE}"
  Delete "$INSTDIR\write-iso.ps1"
  Delete "$INSTDIR\LICENSE"
  Delete "$INSTDIR\README.md"
  Delete "$INSTDIR\${APP_UNINST}"
  RMDir /r "$INSTDIR\docs"
  RMDir /r "$INSTDIR\scripts"
  RMDir "$INSTDIR"

  Delete "$SMPROGRAMS\USBForge\USBForge.lnk"
  Delete "$SMPROGRAMS\USBForge\Docs.lnk"
  Delete "$SMPROGRAMS\USBForge\Uninstall.lnk"
  RMDir "$SMPROGRAMS\USBForge"
  Delete "$DESKTOP\USBForge.lnk"

  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\USBForge"
  DeleteRegKey HKLM "Software\USBForge"
SectionEnd
