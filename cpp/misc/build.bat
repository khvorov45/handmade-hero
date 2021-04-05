@echo off

mkdir build
pushd build
cl -MT -nologo -GR- -Gm- -EHa- -Oi -WX -W4 -wd4100 -wd4189 -DHANDMADE_INTERNAL=1 -DHANDMADE_SLOW=1 -DHANDMADE_WIN32=1 -FC -Z7 -Fmwin32_handmade.map ..\code\win32_handmade.cpp /link -opt:ref user32.lib gdi32.lib
popd
