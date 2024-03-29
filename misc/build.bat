@echo off

set CommonLinkerFlags=-opt:ref user32.lib gdi32.lib winmm.lib -incremental:no
set CommonCompilerFlags=-Od -MTd -nologo -GR- -Gm- -EHa- -Oi -fp:fast -fp:except- -WX -W4 -wd4201 -wd4100 -wd4189 -wd4505 -FC -Zo -Z7
set CommonCompilerFlags=-DHANDMADE_PROFILE=1 -DHANDMADE_INTERNAL=1 -DHANDMADE_SLOW=1 -DHANDMADE_WIN32=1 %CommonCompilerFlags%

mkdir build
pushd build
del *.pdb
cl %CommonCompilerFlags% ..\code\test_asset_builder.cpp -Fetest_asset_builder.exe -D_CRT_SECURE_NO_WARNINGS /link %CommonLinkerFlags%
cl %CommonCompilerFlags% -DTRANSLATION_UNIT_INDEX=0 ..\code\game\lib.cpp -Fmgame_lib.map -Fegame_lib.dll -Fogame_lib.obj  -LD /link /EXPORT:GameUpdateAndRender /EXPORT:GameGetSoundSamples /EXPORT:DEBUGGameFrameEnd -incremental:no -PDB:win32_lib_%random%.pdb
cl %CommonCompilerFlags% -DTRANSLATION_UNIT_INDEX=1 ..\code\win32\main.cpp -Fmwin32_handmade.map -Fewin32_main.exe -Fowin32_main.obj /link %CommonLinkerFlags%
popd

echo Done
