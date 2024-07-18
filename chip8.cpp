#include <cstdint>
#include <time.h>
#include "SDL.h"

const uint32_t WAVE_FREQ = 440;
const uint32_t AUDIO_SAMPLE_RATE = 44100;

const unsigned int FPS = 60;
const unsigned int CLOCK_RATE = 700;

const unsigned int WINDOW_WIDTH = 64;
const unsigned int WINDOW_HEIGHT = 32;
const unsigned int SCALE_FACTOR = 20;

const unsigned int MEMORY_SIZE = 4096;
const unsigned int REGISTER_COUNT = 16;
const unsigned int STACK_SIZE = 16;
const unsigned int DISPLAY_WIDTH = 64;
const unsigned int DISPLAY_HEIGHT = 32;
const unsigned int KEY_COUNT = 16;

const uint32_t START_ADDRESS = 0x200;

const char RUNNING = 'R';
const char QUIT = 'Q';
const char PAUSED = 'P';

int16_t VOLUME = 3000;

float lerp_rate = 0.5;

class Chip8
{
public:
    uint8_t memory[MEMORY_SIZE]{};
    uint8_t registers[REGISTER_COUNT]{};
    uint16_t stack[STACK_SIZE]{};
    bool display[DISPLAY_WIDTH * DISPLAY_HEIGHT]{};
    uint32_t pixel_color[DISPLAY_WIDTH * DISPLAY_HEIGHT]{};
    bool keypad[KEY_COUNT]{};
    uint16_t *stack_ptr;
    uint16_t index{};
    uint16_t pc{};
    uint8_t delay_timer{};
    uint8_t sound_timer{};
    uint16_t opcode;
    Chip8(char *rom_file_name);
    char state;
};

void audio_callback(void *config, uint8_t *stream, int len)
{
    static uint32_t running_sample_index = 0;
    const int32_t wave_period = AUDIO_SAMPLE_RATE / WAVE_FREQ;
    const int32_t half_wave_period = wave_period / 2;

    int16_t *buffer = (int16_t *)stream;

    for (int i = 0; i < len / 2; ++i)
    {
        if ((running_sample_index / half_wave_period) % 2)
        {
            buffer[i] = VOLUME;
        }
        else
        {
            buffer[i] = -VOLUME;
        }
        running_sample_index++;
    }
}

bool initialize_SDL(SDL_Window **window, SDL_Renderer **renderer, SDL_AudioSpec &want, SDL_AudioSpec &have, SDL_AudioDeviceID &dev)
{
    const char *window_title = "CHIP8 Emulator";

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0)
    {
        SDL_Log("Failed To Initialize SDL: %s\n", SDL_GetError());
        return false;
    }

    *window = SDL_CreateWindow(window_title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH * SCALE_FACTOR, WINDOW_HEIGHT * SCALE_FACTOR, 0);

    if (!(*window))
    {
        SDL_Log("Failed To Create SDL Window %s\n", SDL_GetError());
        return false;
    }

    *renderer = SDL_CreateRenderer((*window), -1, SDL_RENDERER_ACCELERATED);

    if (!(*renderer))
    {
        SDL_Log("Failed To Create SDL Renderer %s\n", SDL_GetError());
        return false;
    }

    want.freq = 44100, want.format = AUDIO_S16LSB, want.channels = 1, want.samples = 512, want.callback = audio_callback, want.userdata = nullptr;

    dev = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);

    if (!dev)
    {
        SDL_Log("Failed to initialize audio devices %s\n", SDL_GetError());
        return false;
    }

    if (want.format != have.format || want.channels != have.channels)
    {
        SDL_Log("Failed to load desired audio spec\n");
        return false;
    }

    return true;
}

void set_screen(SDL_Renderer **renderer)
{

    SDL_SetRenderDrawColor(*renderer, 0, 0, 0, 255);
    SDL_RenderClear(*renderer);
}

void cleanup(SDL_Window **window, SDL_Renderer **renderer, SDL_AudioDeviceID &dev)
{
    SDL_DestroyRenderer(*renderer);
    SDL_DestroyWindow(*window);
    SDL_CloseAudioDevice(dev);
    SDL_Quit();
}

Chip8::Chip8(char *rom_file_name)
{
    // load font
    const unsigned int FONTSET_SIZE = 80;

    const uint8_t font[] =
        {
            0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
            0x20, 0x60, 0x20, 0x20, 0x70, // 1
            0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
            0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
            0x90, 0x90, 0xF0, 0x10, 0x10, // 4
            0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
            0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
            0xF0, 0x10, 0x20, 0x40, 0x40, // 7
            0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
            0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
            0xF0, 0x90, 0xF0, 0x90, 0x90, // A
            0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
            0xF0, 0x80, 0x80, 0x80, 0xF0, // C
            0xE0, 0x90, 0x90, 0x90, 0xE0, // D
            0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
            0xF0, 0x80, 0xF0, 0x80, 0x80  // F

        };

    memcpy(&memory[0], font, sizeof(font));

    // open rom file
    FILE *rom = fopen(rom_file_name, "rb");
    if (!rom)
    {
        SDL_Log("Rom File Could Not Opened\n");
        return;
    }

    fseek(rom, 0, SEEK_END);
    const size_t rom_size = ftell(rom);
    const size_t max_size = sizeof memory - START_ADDRESS;
    rewind(rom);

    if (rom_size > max_size)
    {
        SDL_Log("Rom File Size Too Large\n");
        return;
    }

    // load rom contents into memory
    if (fread(&memory[START_ADDRESS], rom_size, 1, rom) != 1)
    {
        SDL_Log("Could Not Load Rom Content Into Memory\n");
        return;
    }

    fclose(rom);

    stack_ptr = &stack[0];
    // set pc to start address
    pc = START_ADDRESS;
    // everything successful, set state to running
    state = 'R';
    // set all pixels to black initially
    memset(&pixel_color[0], 0, sizeof pixel_color);
}

void handle_input(Chip8 &chip8)
{
    SDL_Event e;
    while (SDL_PollEvent(&e))
    {
        if (e.type == SDL_QUIT)
        {
            chip8.state = 'Q';
        }
        else if (e.type == SDL_KEYDOWN)
        {
            switch (e.key.keysym.sym)
            {

            case SDLK_ESCAPE:
                chip8.state = 'Q';
                break;

            case SDLK_SPACE:
                if (chip8.state == RUNNING)
                {
                    chip8.state = PAUSED;
                }
                else
                {
                    chip8.state = RUNNING;
                }
                break;

            case SDLK_i:
                if (VOLUME)
                    VOLUME -= 500;
                break;

            case SDLK_o:
                if (VOLUME < INT16_MAX)
                    VOLUME += 500;
                break;

            case SDLK_n:
                if (lerp_rate < 1.0)
                    lerp_rate += 0.1;
                break;

            case SDLK_m:
                if (lerp_rate > 0.1)
                    lerp_rate -= 0.1;
                break;

            case SDLK_1:
            {
                chip8.keypad[0x1] = true;
            }
            break;

            case SDLK_2:
            {
                chip8.keypad[0x2] = true;
            }
            break;

            case SDLK_3:
            {
                chip8.keypad[0x3] = true;
            }
            break;

            case SDLK_4:
            {
                chip8.keypad[0xC] = true;
            }
            break;

            case SDLK_q:
            {
                chip8.keypad[0x4] = true;
            }
            break;

            case SDLK_w:
            {
                chip8.keypad[0x5] = true;
            }
            break;

            case SDLK_e:
            {
                chip8.keypad[0x6] = true;
            }
            break;

            case SDLK_r:
            {
                chip8.keypad[0xD] = true;
            }
            break;

            case SDLK_a:
            {
                chip8.keypad[0x7] = true;
            }
            break;

            case SDLK_s:
            {
                chip8.keypad[0x8] = true;
            }
            break;

            case SDLK_d:
            {
                chip8.keypad[0x9] = true;
            }
            break;

            case SDLK_f:
            {
                chip8.keypad[0xE] = true;
            }
            break;

            case SDLK_z:
            {
                chip8.keypad[0xA] = true;
            }
            break;

            case SDLK_x:
            {
                chip8.keypad[0x0] = true;
            }
            break;

            case SDLK_c:
            {
                chip8.keypad[0xB] = true;
            }
            break;

            case SDLK_v:
            {
                chip8.keypad[0xF] = true;
            }
            break;

            default:
                break;
            }
        }
        else if (e.type == SDL_KEYUP)
        {
            switch (e.key.keysym.sym)
            {
            case SDLK_1:
            {
                chip8.keypad[0x1] = false;
            }
            break;

            case SDLK_2:
            {
                chip8.keypad[0x2] = false;
            }
            break;

            case SDLK_3:
            {
                chip8.keypad[0x3] = false;
            }
            break;

            case SDLK_4:
            {
                chip8.keypad[0xC] = false;
            }
            break;

            case SDLK_q:
            {
                chip8.keypad[0x4] = false;
            }
            break;

            case SDLK_w:
            {
                chip8.keypad[0x5] = false;
            }
            break;

            case SDLK_e:
            {
                chip8.keypad[0x6] = false;
            }
            break;

            case SDLK_r:
            {
                chip8.keypad[0xD] = false;
            }
            break;

            case SDLK_a:
            {
                chip8.keypad[0x7] = false;
            }
            break;

            case SDLK_s:
            {
                chip8.keypad[0x8] = false;
            }
            break;

            case SDLK_d:
            {
                chip8.keypad[0x9] = false;
            }
            break;

            case SDLK_f:
            {
                chip8.keypad[0xE] = false;
            }
            break;

            case SDLK_z:
            {
                chip8.keypad[0xA] = false;
            }
            break;

            case SDLK_x:
            {
                chip8.keypad[0x0] = false;
            }
            break;

            case SDLK_c:
            {
                chip8.keypad[0xB] = false;
            }
            break;

            case SDLK_v:
            {
                chip8.keypad[0xF] = false;
            }
            break;

            default:
                break;
            }
        }
    }
}

// lerp-helper function
uint32_t lerp(uint32_t initial, uint32_t final, float t)
{
    // extract and handle RGBa separately
    uint8_t r_initial = (initial >> 24) & 0xFF;
    uint8_t g_initial = (initial >> 16) & 0xFF;
    uint8_t b_initial = (initial >> 8) & 0xFF;
    uint8_t a_initial = (initial >> 0) & 0xFF;

    uint8_t r_final = (final >> 24) & 0xFF;
    uint8_t g_final = (final >> 16) & 0xFF;
    uint8_t b_final = (final >> 8) & 0xFF;
    uint8_t a_final = (final >> 0) & 0xFF;

    // use the "precise" method to generate a middle color
    uint8_t r = (1 - t) * r_initial + (t * r_final);
    uint8_t g = (1 - t) * g_initial + (t * g_final);
    uint8_t b = (1 - t) * b_initial + (t * b_final);
    uint8_t a = (1 - t) * a_initial + (t * a_final);

    uint32_t res = (r << 24) | (g << 16) | (b << 8) | (a << 0);
    return res;
}

void update_screen(SDL_Renderer **renderer, Chip8 &chip8)
{
    SDL_Rect rect;
    rect.x = 0, rect.y = 0, rect.w = SCALE_FACTOR, rect.h = SCALE_FACTOR;

    for (uint32_t i = 0; i < sizeof chip8.display; ++i)
    {
        rect.x = (i % (WINDOW_WIDTH)) * SCALE_FACTOR;
        rect.y = (i / (WINDOW_WIDTH)) * SCALE_FACTOR;

        if (chip8.display[i])
        {
            // pixel is on, lerp towards white (which should be drawn)
            uint32_t white = 0xFFFFFFFF;

            if (chip8.pixel_color[i] != white)
            {
                chip8.pixel_color[i] = lerp(chip8.pixel_color[i], white, lerp_rate);
            }

            uint8_t r = (chip8.pixel_color[i] >> 24) & 0xFF;
            uint8_t g = (chip8.pixel_color[i] >> 16) & 0xFF;
            uint8_t b = (chip8.pixel_color[i] >> 8) & 0xFF;
            uint8_t a = (chip8.pixel_color[i] >> 0) & 0xFF;

            SDL_SetRenderDrawColor(*renderer, r, g, b, a);
            SDL_RenderFillRect(*renderer, &rect);

            // draw outlines
            SDL_SetRenderDrawColor(*renderer, 0, 0, 0, 255);
            SDL_RenderDrawRect(*renderer, &rect);
        }
        else
        {
            // pixel is off, lerp towards black (which should be drawn)
            uint32_t black = 0x000000FF;

            if (chip8.pixel_color[i] != black)
            {
                chip8.pixel_color[i] = lerp(chip8.pixel_color[i], black, lerp_rate);
            }

            uint8_t r = (chip8.pixel_color[i] >> 24) & 0xFF;
            uint8_t g = (chip8.pixel_color[i] >> 16) & 0xFF;
            uint8_t b = (chip8.pixel_color[i] >> 8) & 0xFF;
            uint8_t a = (chip8.pixel_color[i] >> 0) & 0xFF;

            SDL_SetRenderDrawColor(*renderer, r, g, b, a);
            SDL_RenderFillRect(*renderer, &rect);
        }
    }

    SDL_RenderPresent(*renderer);
}

void update_timers(Chip8 &chip8, SDL_AudioDeviceID &dev)
{
    if (chip8.delay_timer)
    {
        chip8.delay_timer--;
    }

    if (chip8.sound_timer)
    {
        chip8.sound_timer--;
        SDL_PauseAudioDevice(dev, 0);
    }
    else
    {
        SDL_PauseAudioDevice(dev, 1);
    }
}

void emulate_instruction(Chip8 &chip8)
{
    chip8.opcode = ((chip8.memory)[chip8.pc] << 8) | ((chip8.memory)[chip8.pc + 1]);
    chip8.pc += 2;

    uint16_t NNN = chip8.opcode & 0x0FFF;
    uint8_t NN = chip8.opcode & 0x0FF;
    uint8_t N = chip8.opcode & 0x0F;
    uint8_t X = (chip8.opcode >> 8) & 0x0F;
    uint8_t Y = (chip8.opcode >> 4) & 0x0F;

    switch ((chip8.opcode >> 12) & 0x0F)
    {
    case 0x00:
        if (NN == 0xE0)
        {
            // 0x00E0 clear screen
            printf("0x00E0: Clear screen");
            memset(&chip8.display[0], false, sizeof chip8.display);
        }
        else if (NN == 0xEE)
        {
            // 0x00EE: return from subroutine
            printf("0X00EE: Return from subroutine\n");
            chip8.pc = *--chip8.stack_ptr;
        }
        break;

    case 0x01:
        // 1NNN jump to NNN
        printf("1NNN: jump to NNN\n");
        chip8.pc = NNN;
        break;

    case 0x02:
        // 0x02NNN: call subroutine at NNN
        printf("0x02NNN: call subroutine at NNN\n");
        *chip8.stack_ptr++ = chip8.pc;
        chip8.pc = NNN;
        break;

    case 0x03:
        // 0x3XNN: if VX == NN, skip next instruction;
        printf("0x3XNN: if VX == NN, skip next instruction");
        if (chip8.registers[X] == NN)
        {
            chip8.pc += 2;
        }
        break;

    case 0x04:
        // 0x4XNN: if VX != NN, skip next instruction
        printf("0x4XNN: if VX != NN, skip next instruction");
        if (chip8.registers[X] != NN)
        {
            chip8.pc += 2;
        }
        break;

    case 0x05:
        // 0x5XY0: if VX == VY, skip next instruction
        if (N != 0)
            break;
        printf("0x5XY0: if VX == VY, skip next instruction");
        if (chip8.registers[X] == chip8.registers[Y])
        {
            chip8.pc += 2;
        }
        break;

    case 0x06:
        // 0x6XNN: set register vx to NN
        printf("0x06 //set register vx to NN\n");
        chip8.registers[X] = NN;
        break;

    case 0x07:
        // 0x07XNN:  set register vx += NN
        printf("0x07XNN:  set register vx += NN\n");
        chip8.registers[X] += NN;
        break;

    case 0x08:
        switch (N)
        {
        case 0:
            // 0x8XY0: set VX = VY
            printf("0x8XY0: set VX = VY");
            chip8.registers[X] = chip8.registers[Y];
            break;

        case 1:
            // 0x0XY1: set register VX |= VY
            printf("0x0XY1: set register VX |= VY");
            chip8.registers[X] |= chip8.registers[Y];
            break;

        case 2:
            // 0x8XY2: set register VX &= VY
            printf("0x8XY2: set register VX &= VY");
            chip8.registers[X] &= chip8.registers[Y];
            break;

        case 3:
            // 0x8XY3: set register VX ^= VY
            printf("0x8XY3: set register VX ^= VY");
            chip8.registers[X] ^= chip8.registers[Y];
            break;

        case 4:
            // 0x8XY4: set register VX += VY and set VF to 1 if overflow, else 0
            printf("0x8XY4: set register VX += VY and set VF to 1 if overflow, else 0");
            {
                bool carry = ((uint16_t)(chip8.registers[X] + chip8.registers[Y]) > 255);
                chip8.registers[X] += chip8.registers[Y];
                chip8.registers[0xF] = carry;
                break;
            }

        case 5:
            // 0x8XY5: set register VX-=VY, set VF to 0 when there's underflow, else 1
            printf("0x8XY5: set register VX-=VY, set VF to 0 when there's underflow, else 1");
            {
                bool carry = (chip8.registers[X] <= chip8.registers[Y]);
                chip8.registers[X] -= chip8.registers[Y];
                chip8.registers[0xF] = carry;
                break;
            }

        case 6:
            // 0x8XY6: set VX >>= 1, store LSB of VX prior to shift in VF;
            printf("0x8XY6: set VX >>= 1, store LSB of VX prior to shift in VF");
            {
                bool carry = chip8.registers[Y] & 1;
                chip8.registers[X] = chip8.registers[Y] >> 1;
                chip8.registers[0xF] = carry;
                break;
            }

        case 7:
            // 0x8XY7: set VX = VY - VX, set VF to 0 if underflow, else 1
            printf("0x8XY7: set VX = VY - VX, set VF to 0 if underflow, else 1");
            {
                bool carry = chip8.registers[X] <= chip8.registers[Y];
                chip8.registers[X] = chip8.registers[Y] - chip8.registers[X];
                chip8.registers[0xF] = carry;
                break;
            }

        case 0xE:
            // 0x8XYE set register VX <<= 1, set VF to 1 if MSB of VX prior to shift was set, else 0
            printf("0x8XYE set register VX <<= 1, set VF to 1 if MSB of VX prior to shift was set, else 0");
            {
                bool carry = (chip8.registers[Y] & 0x80) >> 7;
                chip8.registers[X] = chip8.registers[Y] << 1;
                chip8.registers[0xF] = carry;
                break;
            }

        default:
            printf("UNIMPLEMENTED INSTRUCTION FOR 0x08\n");
            break;
        }
        break;

    case 0x09:
        // 0x9XY0: if VX != VY skip next instruction
        printf("0x9XY0: if VX != VY, skip next instruction \n");
        if (chip8.registers[X] != chip8.registers[Y])
        {
            chip8.pc += 2;
        }
        break;

    case 0x0A:
        // 0xANNN: set index register I to NNN
        printf("0xANNN: set index register I to NNN;\n");
        chip8.index = NNN;
        break;

    case 0x0B:
        // 0xBNNN: jump to V0 + NNN
        printf("0xBNNN: jump to address NNN + V0\n");
        chip8.pc = chip8.registers[0] + NNN;
        break;

    case 0x0C:
        // 0xCXNN: set VX = random%(256) & NN
        printf("0xCXNN: set VX = NN & (random in [0,255])\n");
        chip8.registers[X] = (rand() % 256) & NN;
        break;

    case 0x0D:
    {
        // 0xDXYN: draw N height sprite at (X,Y); Read from I;
        printf("0xDXYN: draw N height sprite at (X,Y); Read from I;\n");
        uint8_t posX = chip8.registers[X] % (WINDOW_WIDTH);
        uint8_t posY = chip8.registers[Y] % (WINDOW_HEIGHT);

        chip8.registers[0xF] = 0;

        for (uint8_t i = 0; i < N; ++i)
        {
            uint8_t sprite = chip8.memory[chip8.index + i];
            posX = chip8.registers[X] % (WINDOW_WIDTH);

            for (int8_t j = 7; j >= 0; --j)
            {
                if ((sprite & (1 << j)) && (chip8.display[posY * WINDOW_WIDTH + posX]))
                {
                    chip8.registers[0xF] = 1;
                }

                chip8.display[posY * WINDOW_WIDTH + posX] ^= (bool)(sprite & (1 << j));
                if (++posX >= WINDOW_WIDTH)
                    break;
            }

            if (++posY >= WINDOW_HEIGHT)
                break;
        }
        break;
    }
    case 0x0E:
        if (NN == 0x9E)
        {
            // 0xEX9E if key in VX is pressed, skip next inst
            printf("0xEX9E: if key in VX is pressed, skip next instruction\n");
            if (chip8.keypad[chip8.registers[X]])
            {
                chip8.pc += 2;
            }
        }
        else if (NN == 0xA1)
        {
            // 0xEX9E: if key in VX is not pressed, skip next inst;
            printf("0xEX9E: if key in VX isn't pressed, skip next instruction\n");
            if (!chip8.keypad[chip8.registers[X]])
            {
                chip8.pc += 2;
            }
        }
        break;

    case 0x0F:
        switch (NN)
        {
        case 0x0A:
        {
            static bool any_key_pressed = false;
            static uint8_t key = 0xFF;
            for (uint8_t i = 0; key == 0xFF && i < sizeof chip8.keypad; ++i)
            {
                if (chip8.keypad[i])
                {
                    key == i;
                    any_key_pressed = true;
                    break;
                }
            }
            if (!any_key_pressed)
                chip8.pc -= 2;
            else
            {
                // wait until key is released
                if (chip8.keypad[key])
                    chip8.pc -= 2;
                else
                {
                    // it has been released
                    chip8.registers[X] = key;
                    key = 0xFF; // reset to not found
                    any_key_pressed = false;
                }
            }
            break;
        }

        case 0x1E:
            // 0xFX1E: set I += VX
            // Need to handle Commodore Amiga case
            printf("0xFX1E: set I += VX\n");
            chip8.index += chip8.registers[X];
            break;

        case 0x07:
            // 0xFX07: VX = delay timer
            // Need to implement delay timer
            printf("0xFX07: Set VX = delay timer value\n");
            chip8.registers[X] = chip8.delay_timer;
            break;

        case 0x15:
            // 0xFX15: delay timer = VX
            // Need to implement delay timer
            printf("0xFX15: Set delay timer to value of VX\n");
            chip8.delay_timer = chip8.registers[X];
            break;

        case 0x18:
            // 0xFX18: sound timer = VX
            // Need to implement sound timer
            printf("0xFX18: set sound timer to VX\n");
            chip8.sound_timer = chip8.registers[X];
            break;

        case 0x29:
            // 0xFX29: Set I to the location of the sprite for the character in VX
            printf("0xFX29: Set I to the location of the sprite for the character in VX\n");
            chip8.index = chip8.registers[X] * 5;
            break;

        case 0x33:
        {
            // 0xFX33: store BCD representaiton of VX, unit digit at I+2, tens digit at I+1, hundreds digit at I
            printf("0xFX33: store BCD representaiton of VX, unit digit at I+2, tens digit at I+1, hundreds digit at I\n");
            uint8_t bcd = chip8.registers[X];
            chip8.memory[chip8.index + 2] = bcd % 10;
            bcd /= 10;
            chip8.memory[chip8.index + 1] = bcd % 10;
            bcd /= 10;
            chip8.memory[chip8.index] = bcd;
            break;
        }

        case 0x55:
            // 0xFX55: dump V0 to VX inclusive starting from I
            printf("0xFX55: load V0 to VX into memory starting from I\n");
            for (uint8_t i = 0; i <= X; ++i)
            {
                chip8.memory[chip8.index++] = chip8.registers[i];
            }
            break;

        case 0x65:
            // 0xFX65: load V0 to VX inclusive offset from I
            printf("0xFX65: write V0 to VX values from memory starting with I and incrementing by 1\n");
            for (uint8_t i = 0; i <= X; ++i)
            {
                chip8.registers[i] = chip8.memory[chip8.index++];
            }
            break;

        default:
            printf("UNIMPLEMENTED INSTRUCTION FOR 0x0F\n");
            break;
        }
        break;

    default:
        printf("UNIMPLEMENTED INSTRUCTION\n");
        break;
    }
}

int main(int argc, char **argv)
{

    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s <rom_file_name>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    srand(time(NULL));

    SDL_Window *window = nullptr;
    SDL_Renderer *renderer = nullptr;

    SDL_AudioDeviceID dev = 0;
    SDL_AudioSpec want, have;

    if (initialize_SDL(&window, &renderer, want, have, dev) == false)
    {
        exit(EXIT_FAILURE);
    }

    set_screen(&renderer);

    char *rom_file_name = argv[1];

    Chip8 chip8(rom_file_name);

    if (chip8.state != 'R')
    {
        exit(EXIT_FAILURE);
    }

    while (chip8.state != 'Q')
    {
        handle_input(chip8);

        if (chip8.state == PAUSED)
            continue;

        uint64_t intial_time = SDL_GetPerformanceCounter();

        for (uint32_t i = 0; i < CLOCK_RATE / FPS; ++i)
        {
            emulate_instruction(chip8);
        }

        uint64_t final_time = SDL_GetPerformanceCounter();

        const double delta_time = (double)((1000 * (final_time - intial_time)) / SDL_GetPerformanceFrequency());

        SDL_Delay(1000 / FPS > delta_time ? 1000 / FPS - delta_time : 0);

        update_screen(&renderer, chip8);
        update_timers(chip8, dev);
    }

    cleanup(&window, &renderer, dev);

    return 0;
}