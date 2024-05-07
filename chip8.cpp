#include <cstdint>


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


int main()
{
    return 0;
}