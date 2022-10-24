/**
 * MANJI BIRD
 * Compile and load, then press "continue" three times to play
 * Press any key in the JTAG console to start, then any key to jump
 * Github: https://github.com/n-milo/manji-bird
 */

#define PIXEL_BUFFER ((volatile char *) 0x8000000)

#define TIMER_STATUS        ((volatile int *) 0x10002000)
#define TIMER_CONTROL       ((volatile int *) 0x10002004)
#define TIMER_START_LOW     ((volatile int *) 0x10002008)
#define TIMER_START_HIGH    ((volatile int *) 0x10002008)
#define TIMER_SNAPSHOT_LOW  ((volatile unsigned *) 0x10002010)
#define TIMER_SNAPSHOT_HIGH ((volatile unsigned *) 0x10002014)

#define PUSHBUTTON_DATA ((volatile int *) 0x10000050)

#define JTAG_UART_DATA    ((volatile unsigned *) 0x10001000)
#define JTAG_UART_CONTROL ((volatile unsigned *) 0x10001004)


#define VGA_WIDTH  80
#define VGA_HEIGHT 60

#define PIPE_WIDTH 8
#define PIPE_GAP   20
#define PIPE_APART 40

#define PLAYER_X      8
#define PLAYER_WIDTH  5
#define PLAYER_HEIGHT 4

#define JUMP_HEIGHT 10


// see bottom of file for sprite data
char bird_sprite[20];
// the only letters included are: S, C, O, R, E, and all 10 digits
// indexed by letter, then pixel, all letters are 3x5 in size
// these were written by hand
char alphabet[15][15];


// game-state data
int player_y = 40;
int pipes_x = 81;
int button_held = 0;
int score = 0;
int pipe_heights[3] = { 10, 10, 10 }; // fill in some valid dummy values
                                      // these get overwritten before the game
                                      // even starts


// game logic
int check_for_data_in_jtag(void);
void run(void);
void draw(void);
void check_collision(void);
void die(void);
void player_update(void);
void jump(void);

// drawing
void draw_pixel(int color, int x, int y);
void draw_rect(int color, int x1, int y1, int x2, int y2);
void draw_pipe(int x, int height);
void draw_sprite(char *sprite, int x, int y, int w, int h);
void draw_digit(int digit, int x, int y);

// the "libc" I used (implementation at bottom)
int rand(void);
void srand(unsigned seed);
int putchar(int c);
int puts(char *s);
void *memcpy(void *dst, const void *src, int n);
void puthex(unsigned hex);

// --------------------------------------
//          BEGIN IMPLEMENTATION
// --------------------------------------

int
check_for_data_in_jtag(void) {
    unsigned jtag_data = *JTAG_UART_DATA;
    // RVALID is in bit 15 and is '1' whenever there is data in the jtag queue
    int rvalid = jtag_data & 0x8000;

    // we don't actually care what that data is; we want any key to jump
    return rvalid;
}

void
player_update(void) {
    player_y++;

    int btn = *PUSHBUTTON_DATA;
    if (btn & 0x2) {
        if (!button_held)
            jump();
        button_held = 1;
    } else {
        button_held = 0;
    }

    if (check_for_data_in_jtag() != 0)
        jump();
}

void
jump(void) {
    if (player_y > 0)
        player_y -= JUMP_HEIGHT;
}

void
run(void) {
    int timer_period = 2500000;

    *TIMER_START_HIGH = (timer_period & 0xFFFF0000) >> 16;
    *TIMER_START_LOW = (timer_period & 0xFFFF);
    *TIMER_CONTROL = 0x6;

    puts("Welcome to MANJI BIRD\nSelect this box and press any key to jump\nPress any key to begin...");

    draw();
    while (check_for_data_in_jtag() == 0); // wait for key to be pressed

    // give the player a little jump so they know it works
    jump();

    // we can take a timer snapshot by writing (anything) to the snapshot register
    *TIMER_SNAPSHOT_LOW = 0;
    unsigned time = (*TIMER_SNAPSHOT_HIGH << 16u) | *TIMER_SNAPSHOT_LOW;
    srand(time);

    for (int i = 0; i < 3; i++) {
        // pick a random number in from 8 to 39 by masking the first 5 bits and
        // adding 8
        pipe_heights[i] = (rand() & 31) + 8;
    }

    for(;;) {
        int status = *TIMER_STATUS;
        if (status & 1) {
            *TIMER_STATUS = 0;

            int pipe_speed = 2;
            pipes_x -= pipe_speed;
            if (pipes_x == 7) {
                score++;
            }

            if (pipes_x < -10) {
                // cycle the pipes for infinite gameplay
                pipes_x += 40;
                pipe_heights[0] = pipe_heights[1];
                pipe_heights[1] = pipe_heights[2];
                pipe_heights[2] = (rand() & 31) + 8;
            }

            player_update();
            check_collision();
            draw();
        }
    }
}

void
check_collision(void) {
    int height = pipe_heights[0];

    // check if player collides on the x-axis
    if (PLAYER_X < pipes_x + PIPE_WIDTH &&
            PLAYER_X + PLAYER_WIDTH > pipes_x) {

        if (player_y < height) {
            // player is colliding with top pipe
            die();
        } else if (player_y + PLAYER_HEIGHT > height + PIPE_GAP) {
            // player is colliding with bottom pipe
            die();
        }
    }
}

void
die(void) {
    // play the death "animation" and send a message
    puts("You died!\nReload to play again");
    for(;;) {
        int status = *TIMER_STATUS;
        if (status & 1) {
            *TIMER_STATUS = 0;

            player_y += 2;
            draw();
        }
    }
}

void
draw(void) {
    draw_rect(0x13, 0, 0, VGA_WIDTH, VGA_HEIGHT);

    for (int i = 0; i < 3; i++)
        draw_pipe(pipes_x + i * PIPE_APART, pipe_heights[i]);

    draw_sprite(bird_sprite, PLAYER_X, player_y, PLAYER_WIDTH, PLAYER_HEIGHT);

    draw_rect(0x00, 0, 0, VGA_WIDTH, 7);
    for (int i = 0; i < 5; i++) {
        draw_sprite(alphabet[i], 1+i*4, 1, 3, 5);
    }
    draw_pixel(0xff, 21, 2);
    draw_pixel(0xff, 21, 4);

    int digit1 = score / 10;
    int digit2 = score - digit1*10; // = score % 10
    draw_sprite(alphabet[5+digit1], 26, 1, 3, 5);
    draw_sprite(alphabet[5+digit2], 30, 1, 3, 5);
}

int
main(void) {
    // for some reason, cpulator sets sp to 0x04000000, then immediately
    // crashes when it tries to store values to the stack because the address
    // is out of bounds

    // so here, we set the sp to something more reasonable and then call
    // another function to avoid the compiler automatically saving more values
    // onto the stack

    asm("movia sp, 0x7fffc");
    run();
    return 0;
}

void
draw_pixel(int color, int x, int y) {
    PIXEL_BUFFER[(y << 7) | x] = (char) color;
}

void
draw_rect(int color, int x1, int y1, int x2, int y2) {
    for (int y = y1; y < y2; y++)
        for (int x = x1; x < x2; x++)
            draw_pixel(color, x, y);
}

void
draw_pipe(int x, int height) {
    if (x >= -PIPE_WIDTH && x < VGA_WIDTH) {
        int min_x = x;
        if (min_x < 0)
            min_x = 0;

        int max_x = x + PIPE_WIDTH;
        if (max_x > VGA_WIDTH)
            max_x = VGA_WIDTH;

        draw_rect(0x1C, min_x, 0, max_x, height);
        draw_rect(0x1C, min_x, height+PIPE_GAP, max_x, VGA_HEIGHT);
    }

    if (x-1 >= 0 && x-1 < VGA_WIDTH) {
        draw_pixel(0x1C, x-1, height-1);
        draw_pixel(0x1C, x-1, height + PIPE_GAP);
    }

    if (x+PIPE_WIDTH >= 0 && x+PIPE_WIDTH < VGA_WIDTH) {
        draw_pixel(0x1C, x+PIPE_WIDTH, height-1);
        draw_pixel(0x1C, x+PIPE_WIDTH, height + PIPE_GAP);
    }
}

void
draw_sprite(char *sprite, int x, int y, int w, int h) {
    for (int j = 0; j < h; j++) {
        if (y+j < 0) continue;
        if (y+j >= VGA_HEIGHT) continue;

        for (int i = 0; i < w; i++) {
            if (x+i < 0) continue;
            if (x+i >= VGA_WIDTH) continue;

            char col = sprite[i + j * w];
            draw_pixel(col, x + i, y + j);
        }
    }
}

// libc functions

unsigned rng_state = 0xbeefbabe;

int
rand(void) {
    // https://en.wikipedia.org/wiki/Xorshift
    unsigned x = rng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rng_state = x;
    return (int) x;
}

void
srand(unsigned seed) {
    rng_state = seed;
}

int
putchar(int c) {
    while ((*JTAG_UART_CONTROL & 0xFFFF0000) == 0);
    *JTAG_UART_DATA = (unsigned) c;
    return c;
}

int
puts(char *s) {
    for (; *s; s++)
        putchar(*s);
    putchar('\n');
    return 0;
}

void
puthex(unsigned hex) {
    putchar('0');
    putchar('x');
    for (int i = 7; i >= 0; i--) {
        char hex_char;
        unsigned hex_digit = (hex >> (i*4u)) & 0xFu;

        if (hex_digit >= 10) {
            hex_char = hex_digit - 10 + 'A';
        } else {
            hex_char = hex_digit + '0';
        }

        putchar(hex_char);
    }

    putchar('\n');
}

void *
memcpy(void *dst, const void *src, int n) {
    // convert to char because we can't deref void pointers
    char *cdst = dst;
    const char *csrc = src;
    for (; n > 0; n--)
        *cdst++ = *csrc++;

    // memcpy returns the original dst pointer according to libc spec
    return dst;
}

char bird_sprite[20] = {
    0x13, 0xfc, 0xfc, 0xfc, 0x13,
    0xfc, 0xfc, 0xff, 0x00, 0x13,
    0xfc, 0xfc, 0xfc, 0xe0, 0xe0,
    0x13, 0xfc, 0xfc, 0xfc, 0x13
};

// these were written by hand
char alphabet[15][15] = {
    { 0xff, 0xff, 0xff,  0xff, 0x00, 0x00,  0xff, 0xff, 0xff,  0x00, 0x00, 0xff,  0xff, 0xff, 0xff }, // S
    { 0xff, 0xff, 0xff,  0xff, 0x00, 0x00,  0xff, 0x00, 0x00,  0xff, 0x00, 0x00,  0xff, 0xff, 0xff }, // C
    { 0xff, 0xff, 0xff,  0xff, 0x00, 0xff,  0xff, 0x00, 0xff,  0xff, 0x00, 0xff,  0xff, 0xff, 0xff }, // O
    { 0xff, 0xff, 0x00,  0xff, 0x00, 0xff,  0xff, 0xff, 0x00,  0xff, 0x00, 0xff,  0xff, 0x00, 0xff }, // R
    { 0xff, 0xff, 0xff,  0xff, 0x00, 0x00,  0xff, 0xff, 0x00,  0xff, 0x00, 0x00,  0xff, 0xff, 0xff }, // E
    { 0xff, 0xff, 0xff,  0xff, 0x00, 0xff,  0xff, 0x00, 0xff,  0xff, 0x00, 0xff,  0xff, 0xff, 0xff }, // 0
    { 0x00, 0xff, 0x00,  0xff, 0xff, 0x00,  0x00, 0xff, 0x00,  0x00, 0xff, 0x00,  0xff, 0xff, 0xff }, // 1
    { 0xff, 0xff, 0xff,  0x00, 0x00, 0xff,  0xff, 0xff, 0xff,  0xff, 0x00, 0x00,  0xff, 0xff, 0xff }, // 2
    { 0xff, 0xff, 0xff,  0x00, 0x00, 0xff,  0xff, 0xff, 0xff,  0x00, 0x00, 0xff,  0xff, 0xff, 0xff }, // 3
    { 0xff, 0x00, 0xff,  0xff, 0x00, 0xff,  0xff, 0xff, 0xff,  0x00, 0x00, 0xff,  0x00, 0x00, 0xff }, // 4
    { 0xff, 0xff, 0xff,  0xff, 0x00, 0x00,  0xff, 0xff, 0xff,  0x00, 0x00, 0xff,  0xff, 0xff, 0xff }, // 5
    { 0xff, 0xff, 0xff,  0xff, 0x00, 0x00,  0xff, 0xff, 0xff,  0xff, 0x00, 0xff,  0xff, 0xff, 0xff }, // 6
    { 0xff, 0xff, 0xff,  0x00, 0x00, 0xff,  0x00, 0x00, 0xff,  0x00, 0x00, 0xff,  0x00, 0x00, 0xff }, // 7
    { 0xff, 0xff, 0xff,  0xff, 0x00, 0xff,  0xff, 0xff, 0xff,  0xff, 0x00, 0xff,  0xff, 0xff, 0xff }, // 8
    { 0xFF, 0xff, 0xff,  0xff, 0x00, 0xff,  0xff, 0xff, 0xff,  0x00, 0x00, 0xff,  0xff, 0xff, 0xff }, // 9
};
