#include "chip8.h"

void test_characters() {
  Chip8 *c8 = c8_init();
  C8Display *c8d = c8->display;

  for(int t = 0; t < 0x10; ++t) {
    for(int i = 0; i < 5; ++i) {
      c8d->display_bytes[i + (t / 8) * 6][t] = CHARACTERS[t][i];
    }
  }

  c8display_to_stdout(c8d, 3, 3);
}

void test_blit() {
  Chip8 *c8 = c8_init();
  C8Display *c8d = c8->display;
  u8 sprite[] = { 0x97, 0x11, 0x92, 0x07, 0x70, 0x35, 0x17, 0x61 }; /* 1-2-3-4 test pattern */
  u8 sprite_len = sizeof(sprite) / sizeof(u8);

  c8->rgstr_I = 0x234;

  for(int i = 0; i < sprite_len; ++i) {
    c8->memlocs[c8->rgstr_I + i] = sprite[i];
  }

  for(int i = 0; i < 128; ++i) {
    u8 x = i * 1.5;
    u8 y = i;

    /* load x into register 1 */
    c8_dispatch(c8, 0x6100 + x);

    /* load y into register 2 */
    c8_dispatch(c8, 0x6200 + y);

    c8_dispatch(c8, 0xD120 + sprite_len);

    c8display_to_stdout(c8d, 2, 1);
    usleep(50000);
  }
}

void test_instructions() {
  Chip8 *c8 = c8_init();

  FILE *instfile = fopen("BC_test.ch8", "rb");

  if(instfile) {
    fseek(instfile, 0, SEEK_END);
    int length = ftell(instfile);
    fseek(instfile, 0, SEEK_SET);
    fread(c8->memlocs + 0x200, 1, length, instfile);
  }

  fclose(instfile);

  while(c8_mainloop(c8)) {
    c8display_to_stdout(c8->display, 1, 1);
  }

  free(c8->display);
  free(c8);
}

int main() {
  test_instructions();

  return 0;
}
