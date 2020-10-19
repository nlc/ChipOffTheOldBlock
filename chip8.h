#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif

/* The first 512 bytes are for the interpreter and should not be accessed.
 * Most C8 programs start at 0x200.
 * 0x200 - 0xFFF are the Program/Data space (3583? total bytes)
*/

/* BEGIN typedefs/struct/global declarations */

typedef uint8_t u8;
typedef uint16_t u16;

/* original version: 64x32-pixel display */
typedef struct C8Display {
  u8 display_bytes[32][8];
} C8Display;


const char CHARACTERS[16][5] = {
  { 0xF0, 0x90, 0x90, 0x90, 0xF0 },
  { 0x20, 0x60, 0x20, 0x20, 0x70 },
  { 0xF0, 0x10, 0xF0, 0x80, 0xF0 },
  { 0xF0, 0x10, 0xF0, 0x10, 0xF0 },
  { 0x90, 0x90, 0xF0, 0x10, 0x10 },
  { 0xF0, 0x80, 0xF0, 0x10, 0xF0 },
  { 0xF0, 0x80, 0xF0, 0x90, 0xF0 },
  { 0xF0, 0x10, 0x20, 0x40, 0x40 },
  { 0xF0, 0x90, 0xF0, 0x90, 0xF0 },
  { 0xF0, 0x90, 0xF0, 0x10, 0xF0 },
  { 0xF0, 0x90, 0xF0, 0x90, 0x90 },
  { 0xE0, 0x90, 0xE0, 0x90, 0xE0 },
  { 0xF0, 0x80, 0x80, 0x80, 0xF0 },
  { 0xE0, 0x90, 0x90, 0x90, 0xE0 },
  { 0xF0, 0x80, 0xF0, 0x80, 0xF0 },
  { 0xF0, 0x80, 0xF0, 0x80, 0x80 }
};

typedef struct Chip8 {
  u8  memlocs[4096]; /* memory */
  u8  genregs[16];   /* general-purpose registers (VF is reserved!) */
  u16 rgstr_I;       /* special register for addresses (only 12 lowest bits used) */
  u16 progctr;       /* program counter (instruction pointer) */
  u8  stakptr;       /* stack pointer */
  u16 calstak[26];   /* the call stack */
  u8  delaytm;       /* delay timer (dec. @ 60Hz until 0) */
  u8  soundtm;       /* sound timer (dec. @ 60Hz and sound buzzer until 0) */

  u16 keyboard; /* which keys are pressed */
  u8 jumped; /* Whether we've jumped or we should increment pointer as normal */

  C8Display *display;
} Chip8;

/* END typedefs/struct/global declarations */


/* BEGIN display testing helper functions */

/* For testing purposes */
void c8display_to_pbm(C8Display *c8display, char *fnamebase) {
  char fname[256];
  sprintf(fname, "%s.pbm", fnamebase);
  FILE *pbm = fopen(fname, "w");

  fprintf(pbm, "P1\n# %s\n64 32\n", fname);
  for(int y = 0; y < 32; ++y) {
    for(int x = 0; x < 8; ++x) {
      u8 byte = c8display->display_bytes[y][x];
      for(int b = 7; b >= 0; --b) {
        fprintf(pbm, "%d", !((byte >> b) & 1)); /* invert bc of pbm b/w */
      }
    }
  }
  fclose(pbm);
}

/* For testing purposes */
const int DRAW_FRAME = 1;
void c8display_to_stdout(C8Display *c8display, int x_loc, int y_loc) {
  printf("\033[%d;%dH", y_loc, x_loc);

  if(DRAW_FRAME) {
    for(int i = 0; i < 2; i++) {
      for(int j = 0; j < 2; j++) {
        printf("\033[%d;%dH+", 1 + y_loc + j * 17, 1 + x_loc + i * 65);
      }
    }
    printf("\033[%d;%dH", y_loc + 2, x_loc + 2);
    fflush(stdout); /* flush the buffer */
  }

  for(int y = 0; y < 32; y += 2) {
    for(int x = 0; x < 8; ++x) {
      u8 byte_top = c8display->display_bytes[y][x];
      u8 byte_bot = c8display->display_bytes[y + 1][x];
      for(int b = 7; b >= 0; --b) {
        u8 lobit = ((byte_top >> b) & 1);
        u8 hibit = ((byte_bot >> b) & 1);

        /* hack to sidestep multipbyte char issue */
        switch((hibit << 1) | lobit) {
          case(0): {
            printf(" ");
            break;
          }
          case(1): {
            printf("▀");
            break;
          }
          case(2): {
            printf("▄");
            break;
          }
          case(3): {
            printf("█");
            break;
          }
        }
      }
    }
    printf("\033[B\033[64D");
  }

  fflush(stdout); /* flush the buffer */
}

/* END display testing helper functions */


/* BEGIN initializers */

C8Display *c8display_init() {
  C8Display *c8display = (C8Display *)malloc(sizeof(C8Display));

  for(int y = 0; y < 32; ++y) {
    for(int x = 0; x < 8; ++x) {
      c8display->display_bytes[y][x] = (u8)0;
    }
  }

  return c8display;
}

Chip8 *c8_init() {
  Chip8 *c8 = (Chip8 *)malloc(sizeof(Chip8));

  for(int i = 0; i < 4096; ++i) { /* TODO: do this properly with a memset */
    c8->memlocs[i] = (u8)0;
  }
  for(int i = 0; i < 16; ++i) { /* TODO: do this properly with a memset */
    c8->genregs[i] = (u8)0;
  }
  c8->rgstr_I = 0;
  c8->progctr = 0x200; /* Assume we begin at start of accessible memory */
  c8->stakptr = 0;
  for(int i = 0; i < 26; ++i) { /* TODO: do this properly with a memset */
    c8->calstak[i] = (u8)0;
  }
  c8->delaytm = 0;
  c8->soundtm = 0;

  c8->keyboard = 0;
  c8->jumped = 0;

  c8->display = c8display_init();

  return c8;
}

/* END initializers */


/* BEGIN Emulator utility functions */

/* TODO: check for popping from empty */
u16 c8_stack_pop(Chip8 *c8) {
  c8->stakptr--;
  return c8->calstak[c8->stakptr + 1];
}

/* TODO: check for erroneous access */
u16 c8_set_prog_ctr(Chip8 *c8, u16 val) {
  c8->progctr = val;

  return 0;
}

void c8_jump(Chip8 *c8, u16 addr) {
  c8_set_prog_ctr(c8, addr);

  c8->jumped = 1;
}

void c8_skip(Chip8 *c8) {
  c8_jump(c8, c8->progctr + 4);
}

/* is the key with value "key" pressed? */
u8 c8_get_key(Chip8 *c8, u8 key) {
  return (c8->keyboard >> key) & 1;
}

void c8_press_key(Chip8 *c8, u8 key) {
  c8->keyboard |= (1 << key);
}

u16 c8_extract_bnnn(u16 val) {
  return val & 0x0FFF;
}

u8 c8_extract_bxbb(u16 val) {
  return (val & 0x0F00) >> 0x8;
}

u8 c8_extract_bbyb(u16 val) {
  return (val & 0x00F0) >> 0x4;
}

u8 c8_extract_bbkk(u16 val) {
  return val & 0x00FF;
}

u8 c8_extract_bbbn(u16 val) {
  return val & 0x000F;
}

/* END Emulator utility functions */


/* BEGIN bytecode instruction functions */

void c8_0nnn(Chip8 *c8, u16 instruction) {
  /* noop */
}

/* Clear the screen */
void c8_00E0(Chip8 *c8, u16 instruction) {
  for(int y = 0; y < 32; ++y) {
    for(int x = 0; x < 8; ++x) {
      c8->display->display_bytes[y][x] = (u8)0;
    }
  }
}

/* Return from subroutine */
void c8_00EE(Chip8 *c8, u16 instruction) {
  c8_jump(c8, c8_stack_pop(c8));
}

/* Jump to address nnn */
void c8_1nnn(Chip8 *c8, u16 instruction) {
  c8_jump(c8, c8_extract_bnnn(instruction));
}

/* Call subroutine at nnn */
/* TODO: Handle stack overflow */
void c8_2nnn(Chip8 *c8, u16 instruction) {
  c8->stakptr++;
  c8->calstak[c8->stakptr] = c8->progctr;
  c8_jump(c8, c8_extract_bnnn(instruction));
}

/* Skip next instruction if register x == kk */
void c8_3xkk(Chip8 *c8, u16 instruction) {
  u8 regidx = c8_extract_bxbb(instruction);
  u8 value = c8_extract_bbkk(instruction);

  if(c8->genregs[regidx] == value) {
    c8_skip(c8);
  }
}

/* Skip next instruction if register x != kk */
void c8_4xkk(Chip8 *c8, u16 instruction) {
  u8 regidx = c8_extract_bxbb(instruction);
  u8 value = c8_extract_bbkk(instruction);

  if(c8->genregs[regidx] != value) {
    c8_skip(c8);
  }
}

/* Skip next instruction if register x == register y */
void c8_5xy0(Chip8 *c8, u16 instruction) {
  u8 regidx_x = c8_extract_bxbb(instruction);
  u8 regidx_y = c8_extract_bbyb(instruction);

  if(c8->genregs[regidx_x] != c8->genregs[regidx_y]) {
    c8_skip(c8);
  }
}

/* Load kk into register x */
void c8_6xkk(Chip8 *c8, u16 instruction) {
  u8 regidx = c8_extract_bxbb(instruction);
  u8 value = c8_extract_bbkk(instruction);

  c8->genregs[regidx] = value;
}

/* register x += kk */
void c8_7xkk(Chip8 *c8, u16 instruction) {
  u8 regidx = c8_extract_bxbb(instruction);
  u8 value = c8_extract_bbkk(instruction);

  c8->genregs[regidx] += value;
}

/* Load contents of register y into register x */
void c8_8xy0(Chip8 *c8, u16 instruction) {
  u8 regidx_x = c8_extract_bxbb(instruction);
  u8 regidx_y = c8_extract_bbyb(instruction);

  c8->genregs[regidx_x] = c8->genregs[regidx_y];
}

/* register x |= register y */
void c8_8xy1(Chip8 *c8, u16 instruction) {
  u8 regidx_x = c8_extract_bxbb(instruction);
  u8 regidx_y = c8_extract_bbyb(instruction);

  c8->genregs[regidx_x] |= c8->genregs[regidx_y];
}

/* register x &= register y */
void c8_8xy2(Chip8 *c8, u16 instruction) {
  u8 regidx_x = c8_extract_bxbb(instruction);
  u8 regidx_y = c8_extract_bbyb(instruction);

  c8->genregs[regidx_x] &= c8->genregs[regidx_y];
}

/* register x ^= register y */
void c8_8xy3(Chip8 *c8, u16 instruction) {
  u8 regidx_x = c8_extract_bxbb(instruction);
  u8 regidx_y = c8_extract_bbyb(instruction);

  c8->genregs[regidx_x] ^= c8->genregs[regidx_y];
}

/* add reg. x to reg. y, store in reg. x, set VF to carry bit */
void c8_8xy4(Chip8 *c8, u16 instruction) {
  u8 regidx_x = c8_extract_bxbb(instruction);
  u8 regidx_y = c8_extract_bbyb(instruction);

  u16 sum = c8->genregs[regidx_x] + c8->genregs[regidx_y];
  if(sum > 255) {
    c8->genregs[0xF] = 1;
  }

  c8->genregs[regidx_x] = (u8)(sum & 0xFF);
}

/* subtract and set VF if underflow */
void c8_8xy5(Chip8 *c8, u16 instruction) {
  u8 regidx_x = c8_extract_bxbb(instruction);
  u8 regidx_y = c8_extract_bbyb(instruction);

  if(c8->genregs[regidx_y] > c8->genregs[regidx_x]) {
    c8->genregs[0xF] = 1;
  }

  c8->genregs[regidx_x] -= c8->genregs[regidx_y];
}

/* Shift reg. x right */
/* TODO: Ensure that this actually ignores reg. y like we think */
void c8_8xy6(Chip8 *c8, u16 instruction) {
  u8 regidx = c8_extract_bxbb(instruction);

  if(c8->genregs[regidx] & 0x01) {
    c8->genregs[0xF] = 1;
  }

  c8->genregs[regidx] >>= 1;
}

/* Subtract reg. x from reg. y, store result in reg. x, set VF if underflow */
void c8_8xy7(Chip8 *c8, u16 instruction) {
  u8 regidx_x = c8_extract_bxbb(instruction);
  u8 regidx_y = c8_extract_bbyb(instruction);

  if(c8->genregs[regidx_y] > c8->genregs[regidx_x]) {
    c8->genregs[0xF] = 1;
  }

  c8->genregs[regidx_x] = c8->genregs[regidx_y] - c8->genregs[regidx_x];
}

/* Shift reg. x left */
/* TODO: Ensure that this actually ignores reg. y like we think */
void c8_8xyE(Chip8 *c8, u16 instruction) {
  u8 regidx = c8_extract_bxbb(instruction);

  if(c8->genregs[regidx] & 0x80) {
    c8->genregs[0xF] = 1;
  }

  c8->genregs[regidx] <<= 1;
}

/* Skip if reg. x not equal reg. y */
void c8_9xy0(Chip8 *c8, u16 instruction) {
  u8 regidx_x = c8_extract_bxbb(instruction);
  u8 regidx_y = c8_extract_bbyb(instruction);

  if(c8->genregs[regidx_x] != c8->genregs[regidx_y]) {
    c8_skip(c8);
  }
}

/* load nnn into I register */
void c8_Annn(Chip8 *c8, u16 instruction) {
  u16 value = c8_extract_bnnn(instruction);

  c8->rgstr_I = value;
}

/* jump to location nnn + V0 */
void c8_Bnnn(Chip8 *c8, u16 instruction) {
  u16 value = c8_extract_bnnn(instruction);
  c8_jump(c8, c8->genregs[0] + value);
}

/* TODO: see if there are rules to the rng */
/* generate random integer in [0, 255] and AND it with kk */
void c8_Cxkk(Chip8 *c8, u16 instruction) {
  u8 regidx = c8_extract_bxbb(instruction);
  u8 value = c8_extract_bbkk(instruction);

  c8->genregs[regidx] = (u8)(rand()) & value;
}

/* blit n-byte sprite from memory value (reg. I) to screen @ Vx, Vy */
void c8_Dxyn(Chip8 *c8, u16 instruction) {
  u8 x_coord = c8->genregs[c8_extract_bxbb(instruction)];
  u8 y_coord = c8->genregs[c8_extract_bbyb(instruction)];
  u8 n_bytes = c8_extract_bbbn(instruction);

  /* wrap at edge */
  u8 x_byte = (x_coord / 8) & 0x2F;
  u8 bit_offset = x_coord & 0x7;

  for(int i = 0; i < n_bytes; ++i) {
    /* wrap at edge */
    u8 y_byte = (y_coord + i) & 0x1F;

    u8 byte = c8->memlocs[c8->rgstr_I + i];

    /* potentially have to split over a byte break */
    u8 left_data = byte >> bit_offset;
    u8 left_mask = 0xFF << (8 - bit_offset);
    u8 right_data = byte << (8 - bit_offset);
    u8 right_mask = 0xFF >> bit_offset;

    u8 rest_of_byte;
    /* If we're unsetting a pixel, set VF */
    /* TODO: Make absolutely sure this is right */
    if(c8->display->display_bytes[y_byte][x_byte] & ~left_data) {
      c8->genregs[0xF] = 1;
    }
    rest_of_byte = left_mask & c8->display->display_bytes[y_byte][x_byte];
    c8->display->display_bytes[y_byte][x_byte] = left_data | rest_of_byte;
    /* If we're unsetting a pixel, set VF */
    /* TODO: Make absolutely sure this is right */
    if(c8->display->display_bytes[y_byte][x_byte + 1] & ~right_data) {
      c8->genregs[0xF] = 1;
    }
    rest_of_byte = right_mask & c8->display->display_bytes[y_byte][x_byte + 1];
    c8->display->display_bytes[y_byte][x_byte + 1] = right_data | rest_of_byte;
  }
}

/* Skip next instruction if key with value of reg. x is pressed */
void c8_Ex9E(Chip8 *c8, u16 instruction) {
  u8 regidx = c8_extract_bxbb(instruction);

  if(c8_get_key(c8, c8->genregs[regidx])) {
    c8_skip(c8);
  }
}

/* Skip next instruction if key with value of reg. x is not pressed */
void c8_ExA1(Chip8 *c8, u16 instruction) {
  u8 regidx = c8_extract_bxbb(instruction);

  if(!c8_get_key(c8, c8->genregs[regidx])) {
    c8_skip(c8);
  }
}

/* Set contents of reg. x to delay timer DT */
void c8_Fx07(Chip8 *c8, u16 instruction) {
  u8 regidx = c8_extract_bxbb(instruction);
  c8->genregs[regidx] = c8->delaytm;
}

/* FIXME: We can't handle this abstraction as-is */
/* Wait for key press, then load preessed value into reg. x */
void c8_Fx0A(Chip8 *c8, u16 instruction) {
  /* TODO */
}

/* Set delay timer DT to contents of reg. x */
void c8_Fx15(Chip8 *c8, u16 instruction) {
  u8 regidx = c8_extract_bxbb(instruction);
  c8->delaytm = c8->genregs[regidx];
}

/* Set sound timer ST to contents of reg. x */
void c8_Fx18(Chip8 *c8, u16 instruction) {
  u8 regidx = c8_extract_bxbb(instruction);
  c8->soundtm = c8->genregs[regidx];
}

/* incremenet I by contents of reg. x */
void c8_Fx1E(Chip8 *c8, u16 instruction) {
  u8 regidx = c8_extract_bxbb(instruction);
  c8->rgstr_I += c8->genregs[regidx];
}

/* TODO: Figure out if these can go into low memory, and if so, where */
/* Set I to point at the numeral sprite for digit in reg. x */
void c8_Fx29(Chip8 *c8, u16 instruction) {
  /* TODO */
}

/* Store hundreds, tens and ones place of decimal val. of reg. x in I..I+2 */
void c8_Fx33(Chip8 *c8, u16 instruction) {
  u8 regidx = c8_extract_bxbb(instruction);
  u16 val = c8->genregs[regidx];
  u8 h = (val / 100) % 10;
  u8 t = (val / 10) % 10;
  u8 o = val % 10;

  u16 addr = c8->rgstr_I;
  c8->memlocs[addr] = h;
  c8->memlocs[addr + 1] = t;
  c8->memlocs[addr + 2] = o;
}

/* store registers V0-Vx in memory starting at contents of I */
void c8_Fx55(Chip8 *c8, u16 instruction) {
  u8 regidx_max = c8_extract_bxbb(instruction);
  u16 addr = c8->rgstr_I;

  for(int i = 0; i < regidx_max; ++i) {
    c8->memlocs[addr + i] = c8->genregs[i];
  }
}

/* Read into registers V0-Vx starting from memory locations I */
void c8_Fx65(Chip8 *c8, u16 instruction) {
  u8 regidx_max = c8_extract_bxbb(instruction);
  u16 addr = c8->rgstr_I;

  for(int i = 0; i < regidx_max; ++i) {
    c8->genregs[i] = c8->memlocs[addr + i];
  }
}

/* END bytecode instruction functions */


/* BEGIN dispatcher */

/* Call appropriate instruction function */
u8 c8_dispatch(Chip8 *c8, u16 instruction) {
  if((instruction & 0xFFFF) == 0x0000) {
    return 0;
  } else if((instruction & 0xFFFF) == 0x00E0) {
    c8_00E0(c8, instruction);
  } else if((instruction & 0xFFFF) == 0x00EE) {
    c8_00EE(c8, instruction);
  } else if((instruction & 0xF000) == 0x0000) {
    c8_0nnn(c8, instruction);
  } else if((instruction & 0xF000) == 0x1000) {
    c8_1nnn(c8, instruction);
  } else if((instruction & 0xF000) == 0x2000) {
    c8_2nnn(c8, instruction);
  } else if((instruction & 0xF000) == 0x3000) {
    c8_3xkk(c8, instruction);
  } else if((instruction & 0xF000) == 0x4000) {
    c8_4xkk(c8, instruction);
  } else if((instruction & 0xF00F) == 0x5000) {
    c8_5xy0(c8, instruction);
  } else if((instruction & 0xF000) == 0x6000) {
    c8_6xkk(c8, instruction);
  } else if((instruction & 0xF000) == 0x7000) {
    c8_7xkk(c8, instruction);
  } else if((instruction & 0xF00F) == 0x8000) {
    c8_8xy0(c8, instruction);
  } else if((instruction & 0xF00F) == 0x8001) {
    c8_8xy1(c8, instruction);
  } else if((instruction & 0xF00F) == 0x8002) {
    c8_8xy2(c8, instruction);
  } else if((instruction & 0xF00F) == 0x8003) {
    c8_8xy3(c8, instruction);
  } else if((instruction & 0xF00F) == 0x8004) {
    c8_8xy4(c8, instruction);
  } else if((instruction & 0xF00F) == 0x8005) {
    c8_8xy5(c8, instruction);
  } else if((instruction & 0xF00F) == 0x8006) {
    c8_8xy6(c8, instruction);
  } else if((instruction & 0xF00F) == 0x8007) {
    c8_8xy7(c8, instruction);
  } else if((instruction & 0xF00F) == 0x800E) {
    c8_8xyE(c8, instruction);
  } else if((instruction & 0xF00F) == 0x9000) {
    c8_9xy0(c8, instruction);
  } else if((instruction & 0xF000) == 0xA000) {
    c8_Annn(c8, instruction);
  } else if((instruction & 0xF000) == 0xB000) {
    c8_Bnnn(c8, instruction);
  } else if((instruction & 0xF000) == 0xC000) {
    c8_Cxkk(c8, instruction);
  } else if((instruction & 0xF000) == 0xD000) {
    c8_Dxyn(c8, instruction);
  } else if((instruction & 0xF0FF) == 0xE09E) {
    c8_Ex9E(c8, instruction);
  } else if((instruction & 0xF0FF) == 0xE0A1) {
    c8_ExA1(c8, instruction);
  } else if((instruction & 0xF0FF) == 0xF007) {
    c8_Fx07(c8, instruction);
  } else if((instruction & 0xF0FF) == 0xF00A) {
    c8_Fx0A(c8, instruction);
  } else if((instruction & 0xF0FF) == 0xF015) {
    c8_Fx15(c8, instruction);
  } else if((instruction & 0xF0FF) == 0xF018) {
    c8_Fx18(c8, instruction);
  } else if((instruction & 0xF0FF) == 0xF01E) {
    c8_Fx1E(c8, instruction);
  } else if((instruction & 0xF0FF) == 0xF029) {
    c8_Fx29(c8, instruction);
  } else if((instruction & 0xF0FF) == 0xF033) {
    c8_Fx33(c8, instruction);
  } else if((instruction & 0xF0FF) == 0xF055) {
    c8_Fx55(c8, instruction);
  } else if((instruction & 0xF0FF) == 0xF065) {
    c8_Fx65(c8, instruction);
  } else {
    return 0; /* bad instruction */
  }

  return 1;
}

/* END dispatcher */


/* BEGIN main loop */

u8 c8_mainloop(Chip8 *c8) {
  /* beep if ST is >0. decrement DT and ST. */
  if(c8->soundtm) {
    /* TODO: beep */
    c8->soundtm--;
  }
  if(c8->delaytm) {
    c8->delaytm--;
  }

  /* read instruction */
  u16 instruction = (((u16)(c8->memlocs[c8->progctr])) << 8) | (u16)(c8->memlocs[c8->progctr + 1]);

  /* dispatch instruction */
  if(!c8_dispatch(c8, instruction)) {
    return 0;
  }

  /* increment program counter IF jumped is not true. set jumped to false. */
  if(!c8->jumped) {
    c8->progctr += 2;
    if(c8->progctr >= 4096) { /* halt */
      return 0;
    }
  }
  c8->jumped = 0;

  /* clear keyboard bits and get new keyboard state */
  c8->keyboard = 0;
  /* TODO: Get keyboard state */

  /* sleep until C8 clock cycle is up */
  /* TODO */
  usleep(2000);

  return 1; /* keep going */
}

/* END main loop */
