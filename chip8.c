#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL/SDL.h>

#undef main
#define SCREEN_W 640
#define SCREEN_H 320
#define SCREEN_BPP 32

typedef struct C8{
    unsigned short opcode;
    unsigned char memory[4096];
    unsigned char V[16];
    unsigned short I;
    unsigned short pc;
    unsigned char gfx[64 * 32];
    unsigned char delay_timer;
    unsigned char sound_timer;
    unsigned short stack[16];
    unsigned short sp;
    unsigned int key[16];
} chip8;

unsigned char chip8_fontset[80] =
{
    0xF0, 0x90, 0x90, 0x90, 0xF0, //0
    0x20, 0x60, 0x20, 0x20, 0x70, //1
    0xF0, 0x10, 0xF0, 0x80, 0xF0, //2
    0xF0, 0x10, 0xF0, 0x10, 0xF0, //3
    0x90, 0x90, 0xF0, 0x10, 0x10, //4
    0xF0, 0x80, 0xF0, 0x10, 0xF0, //5
    0xF0, 0x80, 0xF0, 0x90, 0xF0, //6
    0xF0, 0x10, 0x20, 0x40, 0x40, //7
    0xF0, 0x90, 0xF0, 0x90, 0xF0, //8
    0xF0, 0x90, 0xF0, 0x10, 0xF0, //9
    0xF0, 0x90, 0xF0, 0x90, 0x90, //A
    0xE0, 0x90, 0xE0, 0x90, 0xE0, //B
    0xF0, 0x80, 0x80, 0x80, 0xF0, //C
    0xE0, 0x90, 0x90, 0x90, 0xE0, //D
    0xF0, 0x80, 0xF0, 0x80, 0xF0, //E
    0xF0, 0x80, 0xF0, 0x80, 0x80  //F
};

void initialize(chip8 *, char *);
void draw(chip8 *, SDL_Event *);
void emulationCycle(chip8 *);

void initialize(chip8 * c8, char * file_name){
	c8->pc = 0x200;
	c8->opcode = 0;
	c8->I = 0;
	c8->sp = 0;
	c8->delay_timer = 0;
	c8->sound_timer = 0;
	memset(c8->memory,0,sizeof(c8->memory));
	memset(c8->V,0,sizeof(c8->V));
	memset(c8->stack,0,sizeof(c8->stack));
	memset(c8->gfx,0,sizeof(c8->gfx));
	memset(c8->key,0,sizeof(c8->key));

	for(int i = 0; i < 80; i++)
		c8->memory[i] = chip8_fontset[i];
	FILE * file;
	file = fopen(file_name,"rb");
	if(file == NULL){
		fprintf(stderr, "Game does not exist: %s", file_name);
		exit(0);
	}
	fseek(file, 0, SEEK_END);
	long file_size = ftell(file);
	if(file_size >  3584){
		fprintf(stderr,"Rom too big");
		exit(0);
	}

	fseek(file, 0, SEEK_SET);
	char * buffer = malloc(sizeof(char) * file_size);
	size_t result = fread(buffer, 1, file_size, file);
	if(result != file_size){
		fprintf(stderr, "read error");
		exit(0);
	}
	int i;
	for(i = 0; i < file_size;i++)
		c8->memory[0x200 + i] = buffer[i];

	fclose(file);
	free(buffer);

	c8->key[0] = SDLK_1;
	c8->key[1] = SDLK_2;
	c8->key[2] = SDLK_3;
	c8->key[3] = SDLK_q;
	c8->key[4] = SDLK_w;
	c8->key[5] = SDLK_e;
	c8->key[6] = SDLK_a;
	c8->key[7] = SDLK_s;
	c8->key[8] = SDLK_d;
	c8->key[9] = SDLK_z;
	c8->key[10] = SDLK_x;
	c8->key[11] = SDLK_c;
	c8->key[12] = SDLK_4;
	c8->key[13] = SDLK_r;
	c8->key[14] = SDLK_f;
	c8->key[15] = SDLK_v;


}


void draw(chip8 * c8, SDL_Event * event){
    int i, j;
    SDL_Surface * surface = SDL_GetVideoSurface();
    SDL_LockSurface(surface);
    Uint32 * screen = (Uint32 *)surface->pixels;
    memset(screen,0,surface->w*surface->h*sizeof(Uint32));

    for (i = 0; i < SCREEN_H; i++){
        for (j = 0; j < SCREEN_W; j++){
            screen[j+i*surface->w] = c8->gfx[(j/10)+(i/10)*64] ? 0xFFFFFFFF : 0;
        }
    }

    SDL_UnlockSurface(surface);
    SDL_Flip(surface);
    SDL_Delay(15);

    Uint8 * key_pressed = SDL_GetKeyState(NULL);
    if(key_pressed[SDLK_ESCAPE])
        exit(1);
    if(key_pressed[SDLK_p]){
        while(1){
            if(SDL_PollEvent(event)){
                key_pressed = SDL_GetKeyState(NULL);
                if(key_pressed[SDLK_ESCAPE])
                    exit(1);
                if(key_pressed[SDLK_u])
                    break;
            }
        }
    }
}

void emulationCycle(chip8 *c8){
	Uint8 * key_pressed;
	int i;
	unsigned short xCord, yCord;

	// Fetch Opcode
	c8->opcode = c8->memory[c8->pc] << 8  | c8->memory[c8->pc + 1];
	printf("%X\n",c8->opcode);
	// Decode Opcode
	switch(c8->opcode & 0xF000){
		case 0x0000:
			switch(c8->opcode&0x000F){
				case 0x0000: //0x00E0 clears gfx
					memset(c8->gfx, 0, sizeof(c8->gfx));
					c8->pc += 2;
					break;
				case 0x000E: //0x000E returns form subroutine
					c8->pc = c8->stack[(--c8->sp)&0xF] + 2;
					break;
				default:
					printf("Invalid Opcode 0x%X\n", c8->opcode);
			}
			break;
		case 0x1000: //0x1nnn set pc to nnn
			c8->pc = c8->opcode & 0x0FFF;
			break;
		case 0x2000: // 0x2nnn jump to subroutine nnn
			c8->stack[c8->sp] = c8->pc;
			c8->sp++;
			c8->pc = c8->opcode & 0x0FFF;
			break;
		case 0x3000: //0x3xkk skip next instruction if x == kk
			if(c8->V[(c8->opcode & 0x0F00) >> 8] == (c8->opcode & 0x00FF))
				c8->pc += 4;
			else
				c8->pc += 2;
			break;
		case 0x4000: //0x4xnn skip next instruction if x !- kk
			if(c8->V[(c8->opcode & 0x0F00) >> 8] != (c8->opcode & 0x00FF))
				c8->pc += 4;
			else
				c8->pc += 2;
			break;
		case 0x5000: //0x5xy0  Skip next instruction if x = y
			if(c8->V[(c8->opcode & 0x0F00) >> 8] == c8->V[(c8->opcode & 0x00F0) >> 4])
				c8->pc += 4;
			else
				c8->pc += 2;
			break;
		case 0x6000: // 0x6xkk The interpreter puts the value kk into register x
			c8->V[(c8->opcode & 0x0F00) >> 8] = (c8->opcode & 0x00FF);
			c8->pc += 2;
			break;
		case 0x7000: // 0x7xkk Set x = x + kk.
			c8->V[(c8->opcode & 0x0F00) >> 8] += (c8->opcode & 0x00FF);
			c8->pc += 2;
			break;
		case 0x8000:
			switch(c8->opcode & 0x000F){
				case 0x0000: //0x8xy0 store the value of y in x
					c8->V[(c8->opcode & 0x0F00) >> 8] = c8->V[(c8->opcode & 0x00F0) >> 4];
					c8->pc += 2;
					break;
				case 0x0001: //0x8xy1 OR operation on x and y, store results in x
					c8->V[(c8->opcode & 0x0F00) >> 8] |= c8->V[(c8->opcode & 0x00F0) >> 4];
					c8->pc += 2;
					break;
				case 0x0002: //0x8xy2 AND operation on x and y, store results in x
					c8->V[(c8->opcode & 0x0F00) >> 8] &= c8->V[(c8->opcode & 0x00F0) >> 4];
					c8->pc += 2;
					break;
				case 0x0003: //0x8xy3 XOR operation on x and y, store results in x
					c8->V[(c8->opcode & 0x0F00) >> 8] ^= c8->V[(c8->opcode & 0x00F0) >> 4];
					c8->pc += 2;
					break;
				case 0x0004: //0x8xy4 add y to x, set Vf on overflow
					if(c8->V[(c8->opcode & 0x00F0) >> 4] > (0xFF - c8->V[(c8->opcode & 0x0F00) >> 8]))
						c8->V[0xF] = 1;
					else
						c8->V[0xF] = 0;
					c8->V[(c8->opcode & 0x0F00) >> 8] += c8->V[(c8->opcode & 0x00F0) >> 4];
					c8->pc +=2;
					break;
				case 0x0005: //0x8xy5 subtract y from x, set Vf on overflow
					if(c8->V[(c8->opcode & 0x0F00) >> 8] > c8->V[(c8->opcode & 0x00F0) >> 4])
						c8->V[0xF] = 1;
					else
						c8->V[0xF] = 0;
					c8->V[(c8->opcode & 0x0F00) >> 8] -= c8->V[(c8->opcode & 0x00F0) >> 4];
					c8->pc += 2;
					break;
				case 0x0006: //0x8xy6 x = x shr 1, set Vf on overflow
					c8->V[0xF] = c8->V[(c8->opcode & 0x0F00) >> 8] & 0x1;
					c8->V[(c8->opcode & 0x0F00) >> 8] >>= 1;
					c8->pc  += 2;
					break;
				case 0x0007: //0x8xy7 subtract x from y and store in x, set Vf on overflow
					if(c8->V[(c8->opcode & 0x0F00) >> 8] < c8->V[(c8->opcode & 0x00F0) >> 4])
						c8->V[0xF] = 1;
					else
						c8->V[0xF] = 0;
					c8->V[(c8->opcode & 0x0F00) >> 8] = c8->V[(c8->opcode & 0x00F0) >> 4] - c8->V[(c8->opcode & 0x0F00) >> 8];
					c8->pc += 2;
					break;
				case 0x000E:  //0x8xyE y = x shr 1, set Vf on overflow
					c8->V[0xF] = c8->V[(c8->opcode & 0x0F00) >> 8] >> 7;
					c8->V[(c8->opcode & 0x0F00) >> 8] <<= 1;
					c8->pc  += 2;
					break;
				default:
					printf("Invalid Opcode 0x%X\n",c8->opcode);
			}
			break;
		case 0x9000: //0x9xy0 skip next instruction if x != y
			if(c8->V[(c8->opcode & 0x00F0) >> 4] != (0xFF - c8->V[(c8->opcode & 0x0F00) >> 8]))
				c8->pc+=4;
			else
				c8->pc+=2;
			break;
		case 0xA000: //0xAnnn instruction register is set to nnn
			c8->I = c8->opcode & 0x0FFF;
			c8->pc += 2;
			break;
		case 0xB000: //0xBnnn jump to location nnn + V0
			c8->pc = (c8->opcode & 0x0FFF) + c8->V[0];
			break;
		case 0xC000: //0xCxkk vx = random byte and kk
			c8->V[(c8->opcode & 0x0F00) >> 8] = (rand() % 0xFF) & (c8->opcode & 0x00FF);
			c8->pc += 2;
			break;
		case 0xD000: //0xDxyn draw sprite at xy of height n
			xCord = c8->V[(c8->opcode & 0x0F00) >> 8];
			yCord = c8->V[(c8->opcode & 0x00F0) >> 4];
			unsigned short height = c8->opcode & 0x000F;
			unsigned short pixel;
			int y,x;
			c8->V[0xF] = 0;
			for(y = 0; y < height; y++){
				pixel = c8->memory[c8->I + y];
				for(x = 0; x <8; x++){
					if((pixel & (0x80 >> x)) != 0){
						if(c8->gfx[(x + xCord + ((y + yCord) * 64))] == 1)
								c8->V[0xF] = 1;
						c8->gfx[x + xCord + ((y + yCord) * 64)] ^= 1;

					}
				}
			}
			c8->pc += 2;
			break;
		case 0xE000:
			switch(c8->opcode & 0x00FF){
				case 0x009E: //0xEx9e skip instruction if x is pressed
					key_pressed = SDL_GetKeyState(NULL);
					if(key_pressed[c8->key[c8->V[(c8->opcode & 0x0F00) >> 8]]])
						c8->pc += 4;
					else
						c8->pc += 2;
					break;
				case 0x00A1: //0xExA1 skip instruction if x not pressed
					key_pressed = SDL_GetKeyState(NULL);
					if(!key_pressed[c8->key[c8->V[(c8->opcode & 0x0F00) >> 8]]])
						c8->pc += 4;
					else
						c8->pc += 2;
					break;
				default:
					printf("Invalid Opcode 0x%X\n", c8->opcode);
			}
			break;
		case 0xF000:
			switch(c8->opcode & 0x00FF){
				case 0x0007: //0xFx07 x = delay timer
					c8->V[(c8->opcode & 0x0F00) >> 8] = c8->delay_timer;
					c8->pc+=2;
					break;
				case 0x000A: //0xFx0A wait for key press and store value  in x
                    key_pressed = SDL_GetKeyState(NULL);
                    for(i = 0; i < 0x10; i++)
                        if(key_pressed[c8->key[i]]){
                            c8->V[(c8->opcode & 0x0F00) >> 8] = i;
                            c8->pc += 2;
                        }
					break;
				case 0x0015: //0xFx15 delay timer equal to x
					c8->delay_timer = c8->V[(c8->opcode & 0x0F00) >> 8];
					c8->pc  += 2;
					break;
				case 0x0018: //0xFx18 sound timer equal to x
					c8->sound_timer = c8->V[(c8->opcode & 0x0F00) >> 8];
					c8->pc  += 2;
					break;
				case 0x001E: //0xFx1E add x to instruction register
					if(c8->I + c8->V[(c8->opcode & 0x0F00) >> 8] > 0xFFF)
						c8->V[0xF] = 1;
					else
						c8->V[0xF] = 0;
					c8->I += c8->V[(c8->opcode & 0x0F00) >> 8];
					c8->pc += 2;
					break;
				case 0x0029: //0xFx29set instruction register to location of sprite for digit Vx.
					c8->I = c8->V[(c8->opcode & 0x0F00) >> 8] * 0x5;
					c8->pc += 2;
					break;
				case 0x0033: //0xFX33 store x as binary-coded decimal at I, I+1,and I+2
					c8->memory[c8->I]     = c8->V[(c8->opcode & 0x0F00) >> 8] / 100;
					c8->memory[c8->I + 1] = (c8->V[(c8->opcode & 0x0F00) >> 8] / 10) % 10;
					c8->memory[c8->I + 2] = (c8->V[(c8->opcode & 0x0F00) >> 8] % 100) % 10;
					c8->pc += 2;
					break;
				case 0x0055: //0xFx55 stores V0 to Vx in memory starting at address I
					for(i = 0; i <= ((c8->opcode &  0x0F00) >> 8); i++)
						c8->memory[c8->I+i] = c8->V[i];
					c8->I += ((c8->opcode & 0x0F00) >>8) + 1;
					c8->pc+=2;
					break;
				case 0x0065: //0xFx65 fills V0-Vx with values from memory starting at address cI
					for(i = 0; i <= ((c8->opcode & 0x0F00) >> 8); i++)
						c8->V[i] = c8->memory[c8->I+i];
					c8->I += ((c8->opcode & 0x0F00) >> 8) + 1;
					c8->pc += 2;
					break;

				default:
					printf("Invalid Opcode 0x%X\n", c8->opcode);

			}
			break;
		default:
			printf("Invalid opcode 0x%X\n", c8->opcode);
	}

	// Update timers
	if(c8->delay_timer > 0)
		c8->delay_timer--;
	if(c8->sound_timer >  0){
		if(c8->sound_timer  == 1)
			printf("BEEP!\n");
		c8->sound_timer--;
	}


}

int main(int argc, char * argv[]){
	if(argc != 2){
		printf("Wrong Usage\n");
		return 0;
	}

	chip8 c8;
	initialize(&c8,argv[1]);
    SDL_Event event;
    SDL_Init(SDL_INIT_EVERYTHING);
    SDL_SetVideoMode(SCREEN_W, SCREEN_H, SCREEN_BPP, SDL_HWSURFACE | SDL_DOUBLEBUF);
    int i;
     for(;;){
        if(SDL_PollEvent (&event))
            continue;
         for(i=0;i<10;i++)
         emulationCycle(&c8);
         draw(&c8, &event);
     }
	return 0;
}
