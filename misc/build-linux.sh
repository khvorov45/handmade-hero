mkdir -p build

clang -shared -fpic -g -DHANDMADE_INTERNAL=1 -DHANDMADE_SLOW=1 -DHANDMADE_LINUX=1 -Wno-null-dereference code/game/lib.cpp -o build/game_lib.so
clang -g -lX11 -DHANDMADE_INTERNAL=1 -DHANDMADE_SLOW=1 -DHANDMADE_LINUX=1 -Wno-null-dereference code/linux/main.cpp -o build/linux_main

echo Done
