#include <cstdint>
#include "SDL.h"


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

class Chip8
{
    public:
        uint8_t memory[MEMORY_SIZE]{};
        uint8_t registers[REGISTER_COUNT]{};
        uint16_t stack[STACK_SIZE]{};
        bool display[DISPLAY_WIDTH * DISPLAY_HEIGHT]{};
        bool keypad[KEY_COUNT]{};
        uint16_t *stack_ptr;
        uint16_t index{};
        uint16_t pc{};
        uint8_t delay_timer{};
        uint8_t sound_timer{};
        uint16_t opcode;
        Chip8(char* rom_file_name);
        char state;

};



bool initialize_SDL(SDL_Window** window, SDL_Renderer** renderer)
{
    const char* window_title = "CHIP8 Emulator";

    if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0) 
    {
        SDL_Log("Failed To Initialize SDL: %s\n", SDL_GetError());
        return false;
    }

    *window = SDL_CreateWindow(window_title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH*SCALE_FACTOR, WINDOW_HEIGHT*SCALE_FACTOR, 0);

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
}


void cleanup(SDL_Window** window, SDL_Renderer** renderer)
{
    SDL_DestroyRenderer(*renderer);
    SDL_DestroyWindow(*window);
    SDL_Quit();
}


Chip8::Chip8(char* rom_file_name)
{
    //load font
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

    //open rom file
    FILE* rom = fopen(rom_file_name, "rb");
    if(!rom) 
    {
        SDL_Log("Rom File Could Not Opened\n");
        return;
    }

    fseek(rom, 0, SEEK_END);
    const size_t rom_size = ftell(rom);
    const size_t max_size = sizeof memory - START_ADDRESS;
    rewind(rom);

    if(rom_size > max_size) 
    {
        SDL_Log("Rom File Size Too Large\n");
        return;
    }

    //load rom contents into memory
    if (fread(&memory[START_ADDRESS],rom_size, 1, rom) != 1)
    {
        SDL_Log("Could Not Load Rom Content Into Memory\n");
        return;
    }

    fclose(rom);



    stack_ptr = &stack[0];
         //set pc to start address
    pc = START_ADDRESS;
    //everything successful, set state to running
    state = 'R';
    
}

void handle_input(Chip8 &chip8)
{
    SDL_Event e;
    while(SDL_PollEvent(&e))
    {
        if(e.type == SDL_QUIT)
        {
            chip8.state = 'Q';
        }
        else if(e.type == SDL_KEYDOWN)
        {
            switch(e.key.keysym.sym)
            {
                
                case SDLK_ESCAPE:
                    chip8.state = 'Q';
                    break;

                case SDLK_SPACE:
                    if(chip8.state == RUNNING) 
                    {
                        chip8.state = PAUSED;
                    }
                    else
                    {
                        chip8.state = RUNNING;
                    }
                    break;

                default:
                    break;
            }
        }
    }
}

void update_screen(SDL_Renderer** renderer, Chip8 &chip8)
{
    SDL_Rect rect;
    rect.x = 0, rect.y = 0, rect.w = SCALE_FACTOR, rect.h = SCALE_FACTOR;

    for(uint32_t i = 0; i < sizeof chip8.display; ++i)
    {
        rect.x = (i % (WINDOW_WIDTH))*SCALE_FACTOR;
        rect.y = (i / (WINDOW_WIDTH))*SCALE_FACTOR;

        if(chip8.display[i])
        {
            //pixel is on, draw white
            SDL_SetRenderDrawColor(*renderer, 255, 255, 255, 255);
            SDL_RenderFillRect(*renderer, &rect);

            //draw outlines
            SDL_SetRenderDrawColor(*renderer, 0, 0, 0, 255);
            SDL_RenderDrawRect(*renderer, &rect);

        }
        else
        {
            //pixel is off draw black;
            SDL_SetRenderDrawColor(*renderer, 0, 0, 0, 255);
            SDL_RenderFillRect(*renderer, &rect);
        }

    }

    SDL_RenderPresent(*renderer);
}


//emulating instructions
void emulate_instruction(Chip8 &chip8)
{


    chip8.opcode = ( (chip8.memory)[chip8.pc] << 8 ) | ( (chip8.memory)[chip8.pc + 1] );
    chip8.pc += 2;

    uint16_t NNN = chip8.opcode & 0x0FFF;
    uint8_t NN = chip8.opcode & 0x0FF;
    uint8_t N = chip8.opcode & 0x0F;
    uint8_t X = (chip8.opcode >> 8) & 0x0F;
    uint8_t Y = (chip8.opcode >> 4) & 0x0F;

    switch( (chip8.opcode >> 12) & 0x0F )
    {
        case 0x00:
            if(NN == 0xE0)
            {
                //0x00E0 clear screen
                printf("0x00E0: Clear screen");
                memset(&chip8.display[0], false, sizeof chip8.display);
            }
            else if(NN == 0xEE)
            {
                //0x00EE: return from subroutine
                printf("0X00EE: Return from subroutine\n");
                chip8.pc = *--chip8.stack_ptr;
            }
            break;

        case 0x01:
            //1NNN jump to NNN
            printf("1NNN: jump to NNN\n");
            chip8.pc = NNN;
            break;

        case 0x02:
            //0x02NNN: call subroutine at NNN
            printf("0x02NNN: call subroutine at NNN\n");
            *chip8.stack_ptr++ = chip8.pc;
            chip8.pc = NNN;
            break;

        case 0x03:
            //0x3XNN: if VX == NN, skip next instruction;
            printf("0x3XNN: if VX == NN, skip next instruction");
            if(chip8.registers[X] == NN)
            {
                chip8.pc += 2;
            }
            break;

        case 0x04:
            //0x4XNN: if VX != NN, skip next instruction
            printf("0x4XNN: if VX != NN, skip next instruction");
            if(chip8.registers[X] != NN)
            {
                chip8.pc += 2;
            }
            break;

        case 0x05:
            //0x5XY0: if VX == VY, skip next instruction
            printf("0x5XY0: if VX == VY, skip next instruction");
            if(chip8.registers[X] == chip8.registers[Y])
            {
                chip8.pc += 2;
            }
            break;

        case 0x06:
            //0x6XNN: set register vx to NN
            printf("0x06 //set register vx to NN\n");
            chip8.registers[X] = NN;
            break;

        case 0x07:
            //0x07XNN:  set register vx += NN
            printf("0x07XNN:  set register vx += NN\n");
            chip8.registers[X] += NN;
            break;

        case 0x08:
            switch(N)
            {
                case 0:
                    //0x8XY0: set VX = VY
                    printf("0x8XY0: set VX = VY");
                    chip8.registers[X] = chip8.registers[Y];
                    break;

                case 1:
                    //0x0XY1: set register VX |= VY
                    printf("0x0XY1: set register VX |= VY");
                    chip8.registers[X] |= chip8.registers[Y];
                    break;

                case 2:
                    //0x8XY2: set register VX &= VY
                    printf("0x8XY2: set register VX &= VY");
                    chip8.registers[X] &= chip8.registers[Y];
                    break;

                case 3:
                    //0x8XY3: set register VX ^= VY
                    printf("0x8XY3: set register VX ^= VY");
                    chip8.registers[X] ^= chip8.registers[Y];
                    break;

                case 4:
                    //0x8XY4: set register VX += VY and set VF to 1 if overflow, else 0
                    printf("0x8XY4: set register VX += VY and set VF to 1 if overflow, else 0");
                    {      
                        bool carry = ((uint16_t)(chip8.registers[X] + chip8.registers[Y]) > 255);
                        chip8.registers[X] += chip8.registers[Y];
                        chip8.registers[0xF] = carry;
                        break;
                    }

                case 5:
                    //0x8XY5: set register VX-=VY, set VF to 0 when there's underflow, else 1
                    printf("0x8XY5: set register VX-=VY, set VF to 0 when there's underflow, else 1");
                    {
                        bool carry = (chip8.registers[X] <= chip8.registers[Y]);
                        chip8.registers[X] -= chip8.registers[Y];
                        chip8.registers[0xF] = carry;
                        break;
                    }

                case 6:
                    //0x8XY6: set VX >>= 1, store LSB of VX prior to shift in VF;
                    printf("0x8XY6: set VX >>= 1, store LSB of VX prior to shift in VF");
                    {             
                        bool carry = chip8.registers[Y] & 1;
                        chip8.registers[X] = chip8.registers[Y] >> 1;
                        chip8.registers[0xF] = carry;
                        break;
                    }

                case 7:
                    //0x8XY7: set VX = VY - VX, set VF to 0 if underflow, else 1
                    printf("0x8XY7: set VX = VY - VX, set VF to 0 if underflow, else 1");
                    {             
                        bool carry = chip8.registers[X] <= chip8.registers[Y];
                        chip8.registers[X] = chip8.registers[Y] - chip8.registers[X];
                        chip8.registers[0xF] = carry;
                        break;
                    }

                case 0xE:
                    //0x8XYE set register VX <<= 1, set VF to 1 if MSB of VX prior to shift was set, else 0
                    printf("0x8XYE set register VX <<= 1, set VF to 1 if MSB of VX prior to shift was set, else 0");
                    {
                        bool carry = ( chip8.registers[Y] & 0x80 ) >> 7;
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
            //0x9XY0: if VX != VY skip next instruction
            printf("0x9XY0: if VX != VY, skip next instruction \n");
            if(chip8.registers[X] != chip8.registers[Y])
            {
                chip8.pc += 2;
            }
            break;

        case 0x0A:
            //0xANNN: set index register I to NNN
            printf("0xANNN: set index register I to NNN;\n");
            chip8.index = NNN;
            break;

        case 0x0B:
            //0xBNNN: jump to V0 + NNN
            printf("0xBNNN: jump to address NNN + V0\n")
            chip8.pc = chip8.registers[0] + NNN;
            break;

        case 0x0C:
            //0xCXNN: set VX = random%(256) & NN
            printf("0xCXNN: set VX = NN & (random in [0,255])\n")
            chip8.registers[X] = (rand() % 256) & NN;
            break;

        case 0x0D:
        {   
            //0xDXYN: draw N height sprite at (X,Y); Read from I;
            printf("0xDXYN: draw N height sprite at (X,Y); Read from I;\n");
            uint8_t posX = chip8.registers[X]%(WINDOW_WIDTH);
            uint8_t posY = chip8.registers[Y]%(WINDOW_HEIGHT);

            chip8.registers[0xF] = 0; 

            for(uint8_t i = 0; i < N; ++i)
            {
                uint8_t sprite = chip8.memory[chip8.index + i];
                posX = chip8.registers[X]%(WINDOW_WIDTH);

                for(int8_t j = 7; j >= 0; --j)
                {
                    if((sprite & (1 << j)) && (chip8.display[posY * WINDOW_WIDTH + posX])) 
                    {
                        chip8.registers[0xF] = 1;
                    }

                    chip8.display[posY * WINDOW_WIDTH + posX] ^= (sprite & (1 << j));
                    if(++posX >= WINDOW_WIDTH) break;
                }

                if(++posY >= WINDOW_HEIGHT) break;
            }
            break;
        }

        case 0x0E:
            if(NN == 0x9E)
            {
                //0xEX9E if key in VX is pressed, skip next inst
                printf("0xEX9E: if key in VX is pressed, skip next instruction\n");
                if(chip8.keypad[chip8.registers[X]])
                {
                    chip8.pc += 2;
                }
            }
            else if(NN == 0xA1)
            {
                //0xEX9E: if key in VX is not pressed, skip next inst;
                printf("0xEX9E: if key in VX isn't pressed, skip next instruction\n");
                if(!chip8.keypad[chip8.registers[X]])
                {
                    chip8.pc += 2;
                }
            }
            break;

        case 0x0F:
            switch(NN)
            {
                case 0x0A:
                {    
                    static bool any_key_pressed = false;
                    static uint8_t key = 0xFF;
                    for(uint8_t i = 0; key == 0xFF && i < sizeof chip8.keypad; ++i)
                    {
                        if(chip8.keypad[i])
                        {
                            key == i;
                            any_key_pressed = true;
                            break;
                        }
                    }
                    if(!any_key_pressed) chip8.pc -= 2;
                    else
                    {
                        //wait until key is released
                        if(chip8.keypad[key])
                            chip8.pc -= 2;
                        else
                        {
                            //it has been released
                            chip8.registers[X] = key;
                            key = 0xFF; //reset to not found 
                            any_key_pressed = false;
                        }
                    }
                    break;
                }

                case 0x1E:
                    //0xFX1E: set I += VX
                    //Need to handle Commodore Amiga case
                    printf("0xFX1E: set I += VX\n");
                    chip8.index += chip8.registers[X];
                    break;

                case 0x07:
                    //0xFX07: VX = delay timer
                    //Need to implement delay timer
                    printf("0xFX07: Set VX = delay timer value\n");
                    chip8.registers[X] = chip8.delay_timer;
                    break;

                case 0x15:
                    //0xFX15: delay timer = VX
                    //Need to implement delay timer
                    printf("0xFX15: Set delay timer to value of VX\n");
                    chip8.delay_timer = chip8.registers[X];
                    break;

                case 0x18:
                    // 0xFX18: sound timer = VX
                    //Need to implement sound timer
                    printf("0xFX18: set sound timer to VX\n"); 
                    chip8.sound_timer = chip8.registers[X];
                    break;

                case 0x29:
                    //0xFX29: Set I to the location of the sprite for the character in VX
                    printf("0xFX29: Set I to the location of the sprite for the character in VX\n")
                    chip8.index = chip8.registers[X] * 5;
                    break;

                case 0x33:
                    {                 
                        //0xFX33: store BCD representaiton of VX, unit digit at I+2, tens digit at I+1, hundreds digit at I
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
                    //0xFX55: dump V0 to VX inclusive starting from I
                    printf("0xFX55: load V0 to VX into memory starting from I\n");
                    for(uint8_t i = 0; i <= X; ++i)
                    {
                        chip8.memory[chip8.index++] = chip8.registers[i];
                    }
                    break;

                case 0x65:
                    //0xFX65: load V0 to VX inclusive offset from I
                    printf("0xFX65: write V0 to VX values from memory starting with I and incrementing by 1\n")
                    for(uint8_t i = 0; i <= X; ++i)
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

    if(argc < 2)
    {
        fprintf(stderr, "Usage: %s <rom_file_name>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
   

    if(initialize_SDL(&window, &renderer) == false)
    {
        exit(EXIT_FAILURE);
    }

    set_screen(&renderer);

    char* rom_file_name = argv[1];

    Chip8 chip8(rom_file_name);

    if(chip8.state != 'R')
    {
        exit(EXIT_FAILURE);
    }

    while(chip8.state != 'Q')
    {
        handle_input(chip8);
        if(chip8.state == PAUSED) continue;
        
        emulate_instruction(chip8);
        
        update_screen(&renderer, chip8);
    }

    cleanup(&window, &renderer);

    return 0;
}