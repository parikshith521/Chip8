#include <cstdint>
#include "SDL.h"


const unsigned int MEMORY_SIZE = 4096;
const unsigned int REGISTER_COUNT = 16;
const unsigned int STACK_SIZE = 16;
const unsigned int DISPLAY_WIDTH = 64;
const unsigned int DISPLAY_HEIGHT = 32;
const unsigned int KEY_COUNT = 16;

class Chip8
{
    private:

        uint8_t memory[MEMORY_SIZE]{};
        uint8_t registers[REGISTER_COUNT]{};
        uint16_t stack[STACK_SIZE]{};
        uint32_t display[DISPLAY_WIDTH * DISPLAY_HEIGHT]{};
        uint8_t keypad[KEY_COUNT]{};
        uint16_t *stack_ptr;
        uint16_t index{};
        uint16_t pc{};
        uint8_t delay_timer{};
        uint8_t sound_timer{};

};


bool initialize_SDL(SDL_Window** window, SDL_Renderer** renderer)
{
    const char* window_title = "CHIP8 Emulator";

    if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0) 
    {
        SDL_Log("Failed To Initialize SDL: %s\n", SDL_GetError());
        return false;
    }

    *window = SDL_CreateWindow(window_title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 320, 0);

    if(!(*window)) 
    {
        SDL_Log("Failed To Create SDL Window %s\n", SDL_GetError());
        return false;
    }

    *renderer = SDL_CreateRenderer((*window), -1, SDL_RENDERER_ACCELERATED);

    if(!(*renderer)) 
    {
        SDL_Log("Failed To Create SDL Renderer %s\n", SDL_GetError());
        return false;
    }

    return true;
}

void set_screen(SDL_Renderer** renderer)
{
    SDL_SetRenderDrawColor(*renderer, 0, 0, 0, 255);
    SDL_RenderClear(*renderer);
    SDL_RenderPresent(*renderer);
}

void cleanup(SDL_Window** window, SDL_Renderer** renderer)
{
    SDL_DestroyRenderer(*renderer);
    SDL_DestroyWindow(*window);
    SDL_Quit();
}

int main()
{
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;

    if(initialize_SDL(&window, &renderer) == false)
    {
        exit(EXIT_FAILURE);
    }

    set_screen(&renderer);

    SDL_Delay(3000);

    cleanup(&window, &renderer);

    return 0;
}