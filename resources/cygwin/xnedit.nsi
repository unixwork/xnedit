; xnedit.nsi: Copyright 2021 Valerio Messina GNU GPL v2+
; xnedit.nsi is part of XNEdit multi-purpose text editor:
; https://github.com/unixwork/xnedit a fork of Nedit http://www.nedit.org
; XNEdit is free software: you can redistribute it and/or modify
; it under the terms of the GNU General Public License as published by
; the Free Software Foundation, either version 2 of the License, or
; (at your option) any later version.
;
; XNEdit is distributed in the hope that it will be useful,
; but WITHOUT ANY WARRANTY; without even the implied warranty of
; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
; GNU General Public License for more details.
;
; You should have received a copy of the GNU General Public License
; along with XNEdit. If not, see <http://www.gnu.org/licenses/>.

; xnedit.nsi: setup.exe installer generator for Win systems
; This configuration file is used by NSIS to generate a Win setup package
; It will install XNEdit into a fixed directory %ProgramFiles%\xnedit
; International version
; It is based on example2.nsi, so it remember the installation directory,
; has uninstall support and (optionally) installs start menu shortcuts.
; Limitations: work only installing to $ProgramFiles\xnedit
;              so change: pkg="$PROGRAMFILES/xnedit" in 'xnedit_pkg'
; ToDo: let's choose at least the destination drive letter
;       unistaller should let choose to keep custom settings in ~\.xnedit\
; V.0.01.00 2021/08/05

;--------------------------------
; Compiler Compression options
SetCompress force
SetCompressor /SOLID lzma

; Name shown in the installer and uninstaller
Name "XNEdit multi-purpose text editor"

; The file to write
OutFile "XNEdit1.3.2win_setup.exe"

; The default installation directory
InstallDir $PROGRAMFILES64\xnedit

; Registry key to check for directory (so if you install again, it will
; overwrite the old one automatically)
InstallDirRegKey HKLM "Software\Xnedit" "Install_Dir"

; Request application privileges for Windows Vista
RequestExecutionLevel admin

;--------------------------------
; Pages
Page components
;Page directory
Page instfiles

UninstPage uninstConfirm
UninstPage instfiles

;--------------------------------
; The stuff to install actions
Section "Xnedit (required)"
  SectionIn RO

  ; Set output path to the installation directory.
  SetOutPath "$INSTDIR"

  ; Put files to install there
  File fonts.conf
  File hide.vbs
  File xnc.sh
  File xnedit.bat
  File xnedit.ico
  File LICENSE
  File README
  File ReleaseNotes
  File CHANGELOG

  ; Subdirectories
  File /r ".xnedit"
  File /r "cygroot" ; cygwin3.2.0 released Mar 29 2021

  ; SendTo context menu
  CreateShortCut "$SENDTO\XNEdit.lnk" "$INSTDIR\xnedit.bat" "" "$INSTDIR\xnedit.ico" 0

  ; Write the installation path into the registry
  WriteRegStr HKLM SOFTWARE\Xnedit "Install_Dir" "$INSTDIR"

  ; Write the uninstall keys for Windows
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Xnedit" "DisplayName" "XNEdit"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Xnedit" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Xnedit" "NoModify" 1
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Xnedit" "NoRepair" 1
  WriteUninstaller "uninstall.exe"
SectionEnd

; Optional section (can be disabled by the user)
Section "Start Menu Shortcuts"
  CreateDirectory "$SMPROGRAMS\XNEdit"
  CreateShortCut "$SMPROGRAMS\XNEdit\Uninstall.lnk" "$INSTDIR\uninstall.exe" "" "$INSTDIR\uninstall.exe" 0
  CreateShortCut "$SMPROGRAMS\XNEdit\XNEdit.lnk" "$INSTDIR\xnedit.bat" "" "$INSTDIR\xnedit.ico" 0
SectionEnd

;--------------------------------
; Uninstaller actions
Section "Uninstall"
  ; Remove files in subdirectories
  Delete "$INSTDIR\cygroot\bin\*.*"
  Delete "$INSTDIR\cygroot\tmp\*.*"
  Delete "$INSTDIR\cygroot\usr\share\fonts\dejavu\*.*"
  Delete "$INSTDIR\cygroot\usr\share\fonts\*.*"
  Delete "$INSTDIR\cygroot\usr\share\x11\locale\*.*"
  Delete "$INSTDIR\cygroot\*.*"

  ; Remove files
  Delete "$INSTDIR\fonts.conf"
  Delete "$INSTDIR\hide.vbs"
  Delete "$INSTDIR\xnc.sh"
  Delete "$INSTDIR\xnedit.bat"
  Delete "$INSTDIR\xnedit.ico"
  Delete "$INSTDIR\LICENSE"
  Delete "$INSTDIR\README"
  Delete "$INSTDIR\ReleaseNotes"
  Delete "$INSTDIR\CHANGELOG"

  ; Remove user custom settings
  Delete "$INSTDIR\.xnedit\autoload.nm"   ; comment to keep user configurations
  Delete "$INSTDIR\.xnedit\cygspecial.nm" ; comment to keep user configurations
  Delete "$INSTDIR\.xnedit\nedit.rc"      ; comment to keep user configurations
  Delete "$INSTDIR\.xnedit\nedit.history" ; comment to keep user configurations

  ; Remove uninstaller
  Delete "$INSTDIR\uninstall.exe"

  ; Remove directories used
  RMDir /r "$INSTDIR\cygroot"
  RMDir "$INSTDIR\.xnedit"
  RMDir "$INSTDIR"
  RMDir "$SMPROGRAMS\Xnedit"

  ; Remove shortcuts in start Menu, if any
  Delete "$SMPROGRAMS\XNEdit\*.*"

  ; Remove shortcut in SendTo context menu
  Delete "$SENDTO\XNEdit.lnk"

  ; Remove registry keys
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Xnedit"
  DeleteRegKey HKLM SOFTWARE\Xnedit
SectionEnd
