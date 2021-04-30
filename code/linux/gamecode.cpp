#include "../game/lib.hpp"
#include <dlfcn.h>

struct GameCode {
    void* game_dll;
    game_update_and_render* game_update_and_render;
};

GameCode load_game_code() {
    GameCode game_code = {};
    game_code.game_dll = dlopen("/home/khvorova/Projects/handmade-hero/build/game_lib.so", RTLD_LAZY);
    game_code.game_update_and_render =
        (game_update_and_render*)dlsym(game_code.game_dll, "GameUpdateAndRender");
    return game_code;
}
