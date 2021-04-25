@echo off

REM
REM  To run this at startup, use this as your shortcut target:
REM  %windir%\system32\cmd.exe /k w:\handmade\misc\shell.bat
REM

call "G:\Programs\VisualStudio\VC\Auxiliary\Build\vcvarsall.bat" x64
set path="G:\Projects\handmade-hero\misc";%path%
