@echo off

set CommonLinkerFlags=-opt:ref user32.lib gdi32.lib winmm.lib -incremental:no
set CommonCompilerFlags=-MTd -nologo -GR- -Gm- -EHa- -Od -Oi -WX -W4 -wd4201 -wd4100 -wd4189 -wd4505 -DHANDMADE_INTERNAL=1 -DHANDMADE_SLOW=1 -DHANDMADE_WIN32=1 -FC -Z7

mkdir build
pushd build
del *.pdb
cl %CommonCompilerFlags% ..\code\game\lib.cpp -Fmgame_lib.map -Fegame_lib.dll -Fogame_lib.obj  -LD /link /EXPORT:GameUpdateAndRender /EXPORT:GameGetSoundSamples -incremental:no -PDB:win32_lib_%random%.pdb
cl %CommonCompilerFlags% ..\code\win32\main.cpp -Fmwin32_handmade.map -Fewin32_main.exe -Fowin32_main.obj /link %CommonLinkerFlags%
popd

echo Done
