#include <math.h>
#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <string.h>

#define SCREEN_WIDTH 64
#define SCREEN_HEIGHT 32
#define SCALE 12

typedef struct
{
    uint8_t memory[4096];
    uint8_t V[16]; // V0 - VF registers
    uint16_t I;    // Index register
    uint16_t pc;   // Program counter

    uint16_t stack[16];
    uint16_t sp;   // Stack pointer

    uint8_t delay_timer;
    uint8_t sound_timer;

    uint8_t keypad[16];
    uint32_t display[SCREEN_WIDTH * SCREEN_HEIGHT];

} Chip8;

const uint8_t fontset[80] =
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

void Chip8_init(Chip8 *chip8)
{
    memset(chip8, 0, sizeof(Chip8));

    chip8->pc = 0x200;
    chip8->I = 0;
    chip8->sp = 0;

    for (int i = 0; i < 80; i++)
    {
        chip8->memory[0x050 + i] = fontset[i];
    }
}

void Chip8_load_rom(Chip8 *chip8, const char *filename)
{
    FILE *file = fopen(filename, "rb");

    if (file == NULL)
    {
        printf("Unable to open ROM: %s\n", filename);
        exit(1);
    }

    fseek(file, 0, SEEK_END);
    long rom_size = ftell(file);
    rewind(file);

    if (rom_size > (4096 - 0x200))
    {
        printf("ROM is too large for CHIP-8 memory!\n");
        fclose(file);
        exit(1);
    }

    fread(&chip8->memory[0x200], 1, rom_size, file);
    fclose(file);
}

void Chip8_cycle(Chip8 *chip8)
{
    // Stop if PC is outside CHIP-8 memory.
    if (chip8->pc >= 4095)
    {
        printf("Program counter out of bounds: 0x%03X\n", chip8->pc);
        exit(1);
    }

    // FETCH
    uint16_t opcode =
        (chip8->memory[chip8->pc] << 8) |
        chip8->memory[chip8->pc + 1];

    chip8->pc += 2;

    uint16_t NNN = opcode & 0x0FFF;
    uint8_t NN = opcode & 0x00FF;
    uint8_t N = opcode & 0x000F;
    uint8_t X = (opcode & 0x0F00) >> 8;
    uint8_t Y = (opcode & 0x00F0) >> 4;

    // DECODE AND EXECUTE
    switch (opcode & 0xF000)
    {
        case 0x0000:
        {
            if (opcode == 0x00E0) // CLS
            {
                memset(chip8->display, 0, sizeof(chip8->display));
            }
            else if (opcode == 0x00EE) // RET
            {
                if (chip8->sp == 0)
                {
                    printf("Stack underflow error!\n");
                    exit(1);
                }

                chip8->sp--;
                chip8->pc = chip8->stack[chip8->sp];
            }

            // Other 0NNN instructions are ignored.
            break;
        }

        case 0x1000: // 1NNN: JP addr
        {
            chip8->pc = NNN;
            break;
        }

        case 0x2000: // 2NNN: CALL addr
        {
            if (chip8->sp >= 16)
            {
                printf("Stack overflow error!\n");
                exit(1);
            }

            chip8->stack[chip8->sp] = chip8->pc;
            chip8->sp++;
            chip8->pc = NNN;
            break;
        }

        case 0x3000: // 3XNN: Skip if Vx == NN
        {
            if (chip8->V[X] == NN)
            {
                chip8->pc += 2;
            }

            break;
        }

        case 0x4000: // 4XNN: Skip if Vx != NN
        {
            if (chip8->V[X] != NN)
            {
                chip8->pc += 2;
            }

            break;
        }

        case 0x5000: // 5XY0: Skip if Vx == Vy
        {
            if ((opcode & 0x000F) == 0 && chip8->V[X] == chip8->V[Y])
            {
                chip8->pc += 2;
            }

            break;
        }

        case 0x6000: // 6XNN: Vx = NN
        {
            chip8->V[X] = NN;
            break;
        }

        case 0x7000: // 7XNN: Vx += NN
        {
            chip8->V[X] += NN;
            break;
        }

        case 0x8000:
        {
            switch (opcode & 0x000F)
            {
                case 0x0: // 8XY0: Vx = Vy
                {
                    chip8->V[X] = chip8->V[Y];
                    break;
                }

                case 0x1: // 8XY1: Vx |= Vy (Bitwise OR)
                {
                    chip8->V[X] |= chip8->V[Y];
                    chip8->V[0xF] = 0; // Quirk: Clears VF
                    break;
                }

                case 0x2: // 8XY2: Vx &= Vy (Bitwise AND)
                {
                    chip8->V[X] &= chip8->V[Y];
                    chip8->V[0xF] = 0; // Quirk: Clears VF
                    break;
                }

                case 0x3: // 8XY3: Vx ^= Vy (Bitwise XOR)
                {
                    chip8->V[X] ^= chip8->V[Y];
                    chip8->V[0xF] = 0; // Quirk: Clears VF
                    break;
                }

                case 0x4: // 8XY4: Vx += Vy, VF = carry
                {
                    uint16_t sum = (uint16_t)chip8->V[X] + (uint16_t)chip8->V[Y];
                    uint8_t carry = (sum > 255) ? 1 : 0;
                    
                    chip8->V[X] = (uint8_t)(sum & 0xFF);
                    chip8->V[0xF] = carry; // Flag is set last, safe even if X == 0xF
                    break;
                }

                case 0x5: // 8XY5: Vx -= Vy
                {
                    uint8_t tempX = chip8->V[X];
                    uint8_t tempY = chip8->V[Y];
                    uint8_t borrow = (tempX >= tempY) ? 1 : 0; // Must be >=
                    
                    chip8->V[X] = tempX - tempY;
                    chip8->V[0xF] = borrow;
                    break;
                }

                case 0x6: // 8XY6: Vx = Vy >> 1 (Original Quirk behavior)
                {
                    uint8_t lsb = chip8->V[Y] & 0x1;
                    chip8->V[X] = chip8->V[Y] >> 1;
                    chip8->V[0xF] = lsb;
                    break;
                }

                case 0x7: // 8XY7: Vx = Vy - Vx
                {
                    uint8_t tempX = chip8->V[X];
                    uint8_t tempY = chip8->V[Y];
                    uint8_t borrow = (tempY >= tempX) ? 1 : 0; // Must be >=
                    
                    chip8->V[X] = tempY - tempX;
                    chip8->V[0xF] = borrow;
                    break;
                }

                case 0xE: // 8XYE: Vx = Vy << 1 (Original Quirk behavior)
                {
                    uint8_t msb = (chip8->V[Y] & 0x80) >> 7;
                    chip8->V[X] = chip8->V[Y] << 1;
                    chip8->V[0xF] = msb;
                    break;
                }

                default:
                {
                    printf("Unknown 8XY opcode: 0x%04X at PC: 0x%03X\n",
                           opcode, chip8->pc - 2);
                    exit(1);
                }
            }
            break;
        }

        case 0x9000: // 9XY0: Skip if Vx != Vy
        {
            if ((opcode & 0x000F) == 0 && chip8->V[X] != chip8->V[Y])
            {
                chip8->pc += 2;
            }

            break;
        }

        case 0xA000: // ANNN: I = NNN
        {
            chip8->I = NNN;
            break;
        }
        
        case 0xB000:
        {
            chip8->pc = NNN + chip8->V[0];
            break;
        }

        case 0xC000:
        {
            chip8->V[X] = (rand() % 256) & NN;
            break;
        }

        case 0xD000: // DXYN: Draw sprite
        {
            chip8->V[0xF] = 0;

            for (int row = 0; row < N; row++)
            {
                if (chip8->I + row >= 4096)
                {
                    printf("Sprite memory out of bounds!\n");
                    exit(1);
                }

                uint8_t sprite_byte = chip8->memory[chip8->I + row];

                for (int col = 0; col < 8; col++)
                {
                    if ((sprite_byte & (0x80 >> col)) != 0)
                    {
                        int tx = (chip8->V[X] + col) % SCREEN_WIDTH;
                        int ty = (chip8->V[Y] + row) % SCREEN_HEIGHT;
                        int index = tx + (ty * SCREEN_WIDTH);

                        if (chip8->display[index] == 1)
                        {
                            chip8->V[0xF] = 1;
                        }

                        chip8->display[index] ^= 1;
                    }
                }
            }

            break;
        }

        case 0xE000:
        {
            switch (NN)
            {
                case 0x9E: // EX9E: Skip if key in Vx is pressed
                {
                    if (chip8->V[X] < 16 &&
                        chip8->keypad[chip8->V[X]] != 0)
                    {
                        chip8->pc += 2;
                    }

                    break;
                }

                case 0xA1: // EXA1: Skip if key in Vx is not pressed
                {
                    if (chip8->V[X] >= 16 ||
                        chip8->keypad[chip8->V[X]] == 0)
                    {
                        chip8->pc += 2;
                    }

                    break;
                }

                default:
                {
                    printf("Unknown EX opcode: 0x%04X at PC: 0x%03X\n",
                           opcode, chip8->pc - 2);
                    exit(1);
                }
            }

            break;
        }
        case 0xF000:
        {
            switch (NN)
            {
                case 0x07: // FX07: Vx = delay timer
                {
                    chip8->V[X] = chip8->delay_timer;
                    break;
                }

                case 0x0A: // FX0A: Wait for a key press, store hex value in Vx
                {
                    bool key_pressed = false;
                    for (int k = 0; k < 16; k++)
                    {
                        if (chip8->keypad[k])
                        {
                            chip8->V[X] = k;
                            key_pressed = true;
                            break;
                        }
                    }
                    
                    // If no key is pressed, wind back PC to repeat this instruction
                    if (!key_pressed)
                    {
                        chip8->pc -= 2;
                    }
                    break;
                }

                case 0x15: // FX15: delay timer = Vx
                {
                    chip8->delay_timer = chip8->V[X];
                    break;
                }

                case 0x18: // FX18: sound timer = Vx
                {
                    chip8->sound_timer = chip8->V[X];
                    break;
                }

                case 0x1E: // FX1E: I += Vx
                {
                    chip8->I += chip8->V[X];
                    break;
                }

                case 0x29: // FX29: I = font sprite for Vx
                {
                    chip8->I = 0x050 + ((chip8->V[X] & 0x0F) * 5);
                    break;
                }

                case 0x33: // FX33: Store BCD representation of Vx
                {
                    if (chip8->I + 2 >= 4096)
                    {
                        printf("BCD memory out of bounds!\n");
                        exit(1);
                    }

                    chip8->memory[chip8->I] = chip8->V[X] / 100;
                    chip8->memory[chip8->I + 1] = (chip8->V[X] / 10) % 10;
                    chip8->memory[chip8->I + 2] = chip8->V[X] % 10;
                    break;
                }

                case 0x55: // FX55: Store V0 through Vx in memory (With original quirk)
                {
                    if (chip8->I + X >= 4096)
                    {
                        printf("Register store memory out of bounds!\n");
                        exit(1);
                    }

                    for (int i = 0; i <= X; i++)
                    {
                        chip8->memory[chip8->I] = chip8->V[i];
                        chip8->I++; // Modifies I register per step
                    }
                    break;
                }

                case 0x65: // FX65: Load V0 through Vx from memory (With original quirk)
                {
                    if (chip8->I + X >= 4096)
                    {
                        printf("Register load memory out of bounds!\n");
                        exit(1);
                    }

                    for (int i = 0; i <= X; i++)
                    {
                        chip8->V[i] = chip8->memory[chip8->I];
                        chip8->I++; // Modifies I register per step
                    }
                    break;
                }

                default:
                {
                    printf("Unknown FX opcode: 0x%04X at PC: 0x%03X\n",
                           opcode, chip8->pc - 2);
                    exit(1);
                }
            }
            break;
        }

        default:
        {
            printf("Unknown opcode: 0x%04X at PC: 0x%03X\n",
                   opcode, chip8->pc - 2);
            exit(1);
        }
    }
}

void HandleInput(Chip8 *chip8)
{
    // CHIP-8 keypad:
    // 1 2 3 C
    // 4 5 6 D
    // 7 8 9 E
    // A 0 B F

    chip8->keypad[0x1] = IsKeyDown(KEY_ONE);
    chip8->keypad[0x2] = IsKeyDown(KEY_TWO);
    chip8->keypad[0x3] = IsKeyDown(KEY_THREE);
    chip8->keypad[0xC] = IsKeyDown(KEY_FOUR);

    chip8->keypad[0x4] = IsKeyDown(KEY_Q);
    chip8->keypad[0x5] = IsKeyDown(KEY_W);
    chip8->keypad[0x6] = IsKeyDown(KEY_E);
    chip8->keypad[0xD] = IsKeyDown(KEY_R);

    chip8->keypad[0x7] = IsKeyDown(KEY_A);
    chip8->keypad[0x8] = IsKeyDown(KEY_S);
    chip8->keypad[0x9] = IsKeyDown(KEY_D);
    chip8->keypad[0xE] = IsKeyDown(KEY_F);

    chip8->keypad[0xA] = IsKeyDown(KEY_Z);
    chip8->keypad[0x0] = IsKeyDown(KEY_X);
    chip8->keypad[0xB] = IsKeyDown(KEY_C);
    chip8->keypad[0xF] = IsKeyDown(KEY_V);
}

Sound CreateBeepSound(void)
{
    int SampleRate = 44100;
    float frequency = 440.0f;
    int SampleCount = SampleRate * 0.2f; // .2 seconds long
    // Allocate an array for 16-bit mono audio samples
    int16_t *data = (int16_t *)malloc(SampleCount * sizeof(int16_t));

    for (int i = 0;i < SampleCount;i++)
    {
        // Compute raw mathematical sine wave values
        double t = (double)i/ SampleRate;
        double waveValue = sin(2.0 * PI * frequency * t);
        // Scale the normalized wave (-1.0 to 1.0) into signed 16-bit integer bounds
        data[i] = (int16_t)(waveValue * 32000);
    }
    // Wrap our custom data configuration into Raylib's structural container
    Wave wave = {
        .frameCount = SampleCount,
        .sampleRate = SampleRate,
        .sampleSize = 16,
        .channels = 1,
        .data = data
    };
    Sound sound = LoadSoundFromWave(wave);
    free(data);
    return sound;
}

int main(void)
{
    InitWindow(SCREEN_WIDTH * SCALE,SCREEN_HEIGHT * SCALE,"CHIP-8 Emulator");
    InitAudioDevice();
    SetTargetFPS(60);
    srand(time(NULL));

    Chip8 chip8;

    Chip8_init(&chip8);
    Chip8_load_rom(&chip8, "roms/Pong.ch8");

    Sound beep = CreateBeepSound();
    while (!WindowShouldClose())
    {
        HandleInput(&chip8);

        int instructions_per_frame = 10;

        for (int i = 0; i < instructions_per_frame; i++)
        {
            Chip8_cycle(&chip8);
        }

        if (chip8.delay_timer > 0)
        {
            chip8.delay_timer--;
        }
        if (chip8.sound_timer > 0)
        {
            // If the sound isn't already playing, start playing it in a loop
            if (!IsSoundPlaying(beep)) 
            {
                PlaySound(beep);
            }
            chip8.sound_timer--;
        }
        else
        {
            // Stop the beep immediately when the timer hits 0
            if (IsSoundPlaying(beep)) 
            {
                StopSound(beep);
            }
        }

        BeginDrawing();

        ClearBackground(BLACK);

        for (int y = 0; y < SCREEN_HEIGHT; y++)
        {
            for (int x = 0; x < SCREEN_WIDTH; x++)
            {
                if (chip8.display[x + (y * SCREEN_WIDTH)])
                {
                    DrawRectangle(x * SCALE,
                                  y * SCALE,
                                  SCALE,
                                  SCALE,
                                  WHITE);
                }
            }
        }

        EndDrawing();
    }
    UnloadSound(beep);
    CloseWindow();
    CloseAudioDevice();
    return 0;
}
