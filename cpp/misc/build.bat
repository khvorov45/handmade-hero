@echo off

set CommonLinkerFlags=-opt:ref user32.lib gdi32.lib winmm.lib -incremental:no
set CommonCompilerFlags=-MTd -nologo -GR- -Gm- -EHa- -Od -Oi -WX -W4 -wd4201 -wd4100 -wd4189 -DHANDMADE_INTERNAL=1 -DHANDMADE_SLOW=1 -DHANDMADE_WIN32=1 -FC -Z7

mkdir build
pushd build
del *.pdb
cl %CommonCompilerFlags% ..\code\handmade.cpp -Fmhandmade.map  -LD /link /EXPORT:GameUpdateAndRender /EXPORT:GameGetSoundSamples -incremental:no -PDB:handmade_%random%.pdb
cl %CommonCompilerFlags% ..\code\win32_handmade.cpp -Fmwin32_handmade.map /link %CommonLinkerFlags%
popd
