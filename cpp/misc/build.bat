@echo off

set CommonLinkerFlags=-opt:ref user32.lib gdi32.lib winmm.lib -incremental:no
set CommonCompilerFlags=-MT -nologo -GR- -Gm- -EHa- -Od -Oi -WX -W4 -wd4201 -wd4100 -wd4189 -DHANDMADE_INTERNAL=1 -DHANDMADE_SLOW=1 -DHANDMADE_WIN32=1 -FC -Z7

mkdir build
pushd build
del *.pdb
cl %CommonCompilerFlags% ..\code\handmade.cpp -Fmhandmade.map  -LD /link /EXPORT:GameUpdateAndRender /EXPORT:GameGetSoundSamples -incremental:no -PDB:handmade_"%date%-%time:~0,2%-%time:~3,2%-%time:~6,2%-%time:~9,2%".pdb
cl %CommonCompilerFlags% ..\code\win32_handmade.cpp -Fmwin32_handmade.map /link %CommonLinkerFlags%
popd
