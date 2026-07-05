#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <string.h> // Added for memset

#define SCREEN_WIDTH 64
#define SCREEN_HEIGHT 32
#define SCALE 12 // bigger boxes for better viewing

typedef struct
{
    uint8_t memory[4096];
    uint8_t V[16]; // V0 - VF registers
    uint16_t I; // index register
    uint16_t pc; // program counter
    uint16_t stack[16];
    uint16_t sp; //stack pointer
    uint8_t delay_timer;
    uint8_t sound_timer;
    uint8_t keypad[16];
    uint32_t display[SCREEN_WIDTH * SCREEN_HEIGHT];// 1 = white 0 = black

} Chip8;

const uint8_t fontset[80] = {
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

void Chip8_init(Chip8 *chip8)
{
    // Fix: Clear entire memory structure to prevent garbage data crashes
    memset(chip8, 0, sizeof(Chip8));

    chip8->pc  = 0x200; // roms start here
    chip8->I = 0;
    chip8->sp = 0;

    // loading fontset into memory
    for(int i = 0; i < 80; i++)
    {
        chip8->memory[0x050 + i] = fontset[i];
    }
}

void Chip8_load_rom(Chip8 *chip8, const char *filename)
{
    FILE *file = fopen(filename, "rb");
    if(file == NULL)
    {
        printf("Unable to open Rom \n");
        exit(1);
    }
    // read directly into memory
    fread(&chip8->memory[0x200], 1, 4096 - 0x200, file);
    fclose(file);
}

void Chip8_cycle(Chip8 *chip8)
{
    // 1. FETCH
    uint16_t opcode = (chip8->memory[chip8->pc] << 8) | chip8->memory[chip8->pc + 1];
    chip8->pc += 2; 

    // Variables commonly extracted from opcodes
    uint16_t NNN = opcode & 0x0FFF; 
    uint8_t NN = opcode & 0x00FF; 
    uint8_t N = opcode & 0x000F; 
    uint8_t X = (opcode & 0x0F00) >> 8; 
    uint8_t Y = (opcode & 0x00F0) >> 4; 
    
    // 2. DECODE & EXECUTE
    switch (opcode & 0xF000)
    {
        case 0x0000:
        {
            if (opcode == 0x00E0) // Clear screen
            {
                for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++)
                {
                    chip8->display[i] = 0;
                }
            }
            else if (opcode == 0x00EE) // Return from subroutine
            {
                chip8->sp--;
                chip8->pc = chip8->stack[chip8->sp];
            }
            break; 
        }
        case 0x1000: // JP addr (1NNN)
        {
            chip8->pc = NNN;
            break;
        }
        case 0x3000: // SE Vx, byte (3XNN)
        {
            if(chip8->V[X] == NN)
            {
                chip8->pc += 2;
            }
            break;
        }
        case 0x6000: // LD Vx, byte (6XNN)
        {
            chip8->V[X] = NN;
            break;
        }
        case 0x7000: // ADD Vx, byte (7XNN)
        {
            chip8->V[X] += NN;
            break;
        }
        case 0xA000: // LD I, addr (ANNN)
        {
            chip8->I = NNN;
            break;
        }
        case 0xD000: // DRW Vx, Vy, nibble (DXYN)
        {
            chip8->V[0xF] = 0;
            for (int row = 0; row < N; row++) {
                uint8_t sprite_byte = chip8->memory[chip8->I + row];
                for (int col = 0; col < 8; col++) {
                    if ((sprite_byte & (0x80 >> col)) != 0) {
                        int tx = (chip8->V[X] + col) % SCREEN_WIDTH;
                        int ty = (chip8->V[Y] + row) % SCREEN_HEIGHT;
                        int index = tx + (ty * SCREEN_WIDTH);
                        
                        if (chip8->display[index] == 1) chip8->V[0xF] = 1; // Collision
                        chip8->display[index] ^= 1;
                    }
                }
            }
            break;
        }
        default:
        {
            printf("Unknown opcode: 0x%X\n", opcode);
        }
    } // Fix: Closed the switch statement brace
} // Fix: Closed the Chip8_cycle function brace

void HandleInput(Chip8 *chip8)
{
    chip8->keypad[0x1] = IsKeyDown(KEY_ONE); chip8->keypad[0x2] = IsKeyDown(KEY_TWO);
    chip8->keypad[0x3] = IsKeyDown(KEY_THREE); chip8->keypad[0x4] = IsKeyDown(KEY_Q);
} // Fix: Closed the HandleInput function brace

int main(void) 
{
    InitWindow(SCREEN_WIDTH * SCALE, SCREEN_HEIGHT * SCALE, "CHIP-8 Emulator");
    SetTargetFPS(60); 

    Chip8 chip8;
    Chip8_init(&chip8);
    Chip8_load_rom(&chip8, "roms/pong.ch8");

    while (!WindowShouldClose()) {
        HandleInput(&chip8);

        int instructions_per_frame = 10; 
        for (int i = 0; i < instructions_per_frame; i++) {
            Chip8_cycle(&chip8);
        }

        if (chip8.delay_timer > 0) chip8.delay_timer--;
        if (chip8.sound_timer > 0) {
            chip8.sound_timer--;
        }

        BeginDrawing();
            ClearBackground(BLACK);

            for (int y = 0; y < SCREEN_HEIGHT; y++) {
                for (int x = 0; x < SCREEN_WIDTH; x++) {
                    if (chip8.display[x + (y * SCREEN_WIDTH)]) {
                        DrawRectangle(x * SCALE, y * SCALE, SCALE, SCALE, WHITE);
                    }
                }
            }
        EndDrawing();
    }

    CloseWindow();
    return 0;
}