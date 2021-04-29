mkdir -p build

clang -g -lX11 code/linux/main.cpp -o build/linux_main

echo Done
