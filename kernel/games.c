/* Minimal Desktop - bundled games (snake, pong, doom) running in VGA mode 13h */
#include "shell.h"
#include "gfx.h"
#include "keyboard.h"
#include "pit.h"
#include "console.h"
#include "string.h"
#include "task.h"
#include "store.h"
#include "gui.h"
#include "mouse.h"
#include "io.h"

/* ------------------------------------------------------------------------- */
/* common helpers                                                            */
/* ------------------------------------------------------------------------- */
static void game_banner(const char *title, const char *hint) {
    gfx_clear(GUI_DESK1);
    gfx_text(8, 6, title, GUI_WHITE);
    gfx_text(8, 190, hint, GUI_GREY);
}

static void center_text(int y, const char *s, u8 color) {
    int x = (GFX_WIDTH - gfx_text_width(s)) / 2;
    gfx_text(x < 0 ? 0 : x, y, s, color);
}

/* ------------------------------------------------------------------------- */
/* snake                                                                     */
/* ------------------------------------------------------------------------- */
#define SNAKE_CELL   8
#define SNAKE_COLS   40
#define SNAKE_ROWS   22
#define SNAKE_MAX    256
#define SNAKE_TOP    24

static void snake_draw_cell(int cx, int cy, u8 color) {
    gfx_fill_rect(cx * SNAKE_CELL, SNAKE_TOP + cy * SNAKE_CELL,
                  SNAKE_CELL - 1, SNAKE_CELL - 1, color);
}

static int snake_round(int *best) {
    int body_x[SNAKE_MAX], body_y[SNAKE_MAX];
    int len = 3;
    int dx = 1, dy = 0;
    int px = 8, py = SNAKE_ROWS / 2;
    int fx = 20, fy = 8;
    int score = 0;
    bool dead = false;
    bool paused = false;

    for (int i = 0; i < len; i++) {
        body_x[i] = px - i;
        body_y[i] = py;
    }
    fx = (int)(rand() % SNAKE_COLS);
    fy = (int)(rand() % SNAKE_ROWS);

    while (!dead) {
        game_banner("SNAKE  -  arrows steer, P pause, ESC quit", "");
        for (int x = 0; x <= GFX_WIDTH; x += SNAKE_CELL)
            gfx_vline(x, SNAKE_TOP, SNAKE_ROWS * SNAKE_CELL, 1);
        for (int y = 0; y <= SNAKE_ROWS; y++)
            gfx_hline(0, SNAKE_TOP + y * SNAKE_CELL, GFX_WIDTH, 1);
        char buf[48];
        ksnprintf(buf, sizeof(buf), "score %d   best %d   len %d", score, *best, len);
        gfx_text(8, 6, buf, GUI_CYAN);

        snake_draw_cell(fx, fy, GUI_RED);
        for (int i = len - 1; i >= 0; i--)
            snake_draw_cell(body_x[i], body_y[i], i == 0 ? GUI_WHITE : GUI_GREEN);
        if (paused) center_text(96, "PAUSED", GUI_YELLOW);

        gfx_fill_rect(0, SNAKE_TOP + SNAKE_ROWS * SNAKE_CELL,
                      GFX_WIDTH, GFX_HEIGHT - (SNAKE_TOP + SNAKE_ROWS * SNAKE_CELL),
                      GUI_BLACK);
        gfx_text(8, 192, "ESC quit   P pause", GUI_GREY);
        gfx_present();

        u32 deadline = pit_ticks() + (paused ? 2 : (score > 10 ? 5 : 8));
        while ((s32)(pit_ticks() - deadline) < 0) {
            int k = kbd_getchar();
            if (k < 0) { pit_sleep_ms(4); continue; }
            switch (k) {
                case KEY_ESC: return -1;
                case 'p': case 'P': paused = !paused; break;
                case KEY_UP:    if (dy != 1)  { dx = 0; dy = -1; } break;
                case KEY_DOWN:  if (dy != -1) { dx = 0; dy = 1; } break;
                case KEY_LEFT:  if (dx != 1)  { dx = -1; dy = 0; } break;
                case KEY_RIGHT: if (dx != -1) { dx = 1; dy = 0; } break;
                default: break;
            }
            if (paused) break;
        }
        if (paused) continue;

        int nx = body_x[0] + dx;
        int ny = body_y[0] + dy;
        if (nx < 0 || nx >= SNAKE_COLS || ny < 0 || ny >= SNAKE_ROWS) { dead = true; break; }
        for (int i = 0; i < len; i++)
            if (body_x[i] == nx && body_y[i] == ny) { dead = true; break; }
        if (dead) break;

        for (int i = len - 1; i > 0; i--) {
            body_x[i] = body_x[i - 1];
            body_y[i] = body_y[i - 1];
        }
        body_x[0] = nx;
        body_y[0] = ny;

        bool grow = (nx == fx && ny == fy);
        if (grow) {
            if (len < SNAKE_MAX) len++;
            score += 10;
            if (score > *best) *best = score;
            do {
                fx = (int)(rand() % SNAKE_COLS);
                fy = (int)(rand() % SNAKE_ROWS);
            } while (body_x[0] == fx && body_y[0] == fy);
        }
    }

    game_banner("SNAKE  -  game over", "");
    char buf[48];
    ksnprintf(buf, sizeof(buf), "score %d", score);
    center_text(84, buf, GUI_YELLOW);
    center_text(104, "R restart   ESC quit", GUI_WHITE);
    gfx_present();
    for (;;) {
        int k = kbd_getchar();
        if (k == KEY_ESC) return -1;
        if (k == 'r' || k == 'R') return score;
        pit_sleep_ms(10);
    }
}

int cmd_snake(int argc, char **argv) {
    UNUSED(argc); UNUSED(argv);
    if (!store_module_bound(MOD_ENTRY_SNAKE)) {
        kprintf("snake: not installed. Run: store install snake\n");
        return -1;
    }
    int pid = task_register("snake", 64, true);
    task_set_state(pid, "running");
    int best = 0;
    gfx_enter();
    for (;;) {
        int r = snake_round(&best);
        if (r < 0) break;
    }
    gfx_leave();
    task_unregister(pid);
    kprintf("snake: closed (best score %d)\n", best);
    return 0;
}

/* ------------------------------------------------------------------------- */
/* pong                                                                      */
/* ------------------------------------------------------------------------- */
#define PONG_TOP     24
#define PONG_BOTTOM  184
#define PONG_PAD_W   5
#define PONG_PAD_H   26
#define PONG_BALL    4
#define PONG_WIN     7

static void pong_draw_paddles(int ly, int ry, int bx, int by, int ls, int rs) {
    (void)bx; (void)by;
    gfx_clear(GUI_DESK1);
    gfx_text(8, 6, "PONG  -  up/down or W/S, ESC quit", GUI_WHITE);
    char buf[32];
    ksnprintf(buf, sizeof(buf), "%d : %d", ls, rs);
    gfx_text(GFX_WIDTH / 2 - 16, 6, buf, GUI_CYAN);
    for (int y = PONG_TOP; y < PONG_BOTTOM; y += 8)
        gfx_vline(GFX_WIDTH / 2, y, 4, GUI_DESK3);
    gfx_fill_rect(8, ly, PONG_PAD_W, PONG_PAD_H, GUI_GREEN);
    gfx_fill_rect(GFX_WIDTH - 8 - PONG_PAD_W, ry, PONG_PAD_W, PONG_PAD_H, GUI_RED);
}

int cmd_pong(int argc, char **argv) {
    UNUSED(argc); UNUSED(argv);
    if (!store_module_bound(MOD_ENTRY_PONG)) {
        kprintf("pong: not installed. Run: store install pong\n");
        return -1;
    }
    int pid = task_register("pong", 64, true);
    task_set_state(pid, "running");

    int ly = (PONG_BOTTOM - PONG_TOP - PONG_PAD_H) / 2 + PONG_TOP;
    int ry = ly;
    int bx = GFX_WIDTH / 2, by = (PONG_TOP + PONG_BOTTOM) / 2;
    int bdx = 2, bdy = 1;
    int ls = 0, rs = 0;
    bool exit_game = false;

    gfx_enter();
    while (!exit_game && ls < PONG_WIN && rs < PONG_WIN) {
        pong_draw_paddles(ly, ry, bx, by, ls, rs);
        gfx_fill_rect(bx, by, PONG_BALL, PONG_BALL, GUI_WHITE);
        gfx_text(8, 192, "ESC quit", GUI_GREY);
        gfx_present();

        u32 deadline = pit_ticks() + 3;
        while ((s32)(pit_ticks() - deadline) < 0) {
            int k = kbd_getchar();
            if (k == KEY_ESC) { exit_game = true; break; }
            if (k == KEY_UP || k == 'w' || k == 'W') {
                ly -= 4;
                if (ly < PONG_TOP) ly = PONG_TOP;
            } else if (k == KEY_DOWN || k == 's' || k == 'S') {
                ly += 4;
                if (ly + PONG_PAD_H > PONG_BOTTOM) ly = PONG_BOTTOM - PONG_PAD_H;
            }
            pit_sleep_ms(2);
        }
        if (exit_game) break;

        /* Simple AI with a small tracking error. */
        int target = by - PONG_PAD_H / 2;
        if (ry < target) ry += 2;
        else if (ry > target) ry -= 2;
        if (ry < PONG_TOP) ry = PONG_TOP;
        if (ry + PONG_PAD_H > PONG_BOTTOM) ry = PONG_BOTTOM - PONG_PAD_H;

        bx += bdx;
        by += bdy;
        if (by <= PONG_TOP) { by = PONG_TOP; bdy = -bdy; }
        if (by + PONG_BALL >= PONG_BOTTOM) { by = PONG_BOTTOM - PONG_BALL; bdy = -bdy; }

        bool hit_left = bx <= 8 + PONG_PAD_W && bx >= 8 &&
                        by + PONG_BALL >= ly && by <= ly + PONG_PAD_H;
        bool hit_right = bx + PONG_BALL >= GFX_WIDTH - 8 - PONG_PAD_W &&
                         bx + PONG_BALL <= GFX_WIDTH - 8 &&
                         by + PONG_BALL >= ry && by <= ry + PONG_PAD_H;
        if (hit_left && bdx < 0) {
            bdx = -bdx + 1;
            if (bdx > 5) bdx = 5;
            int rel = by - ly;
            bdy = rel < PONG_PAD_H / 3 ? -2 : (rel > 2 * PONG_PAD_H / 3 ? 2 : (bdy > 0 ? 1 : -1));
        }
        if (hit_right && bdx > 0) {
            bdx = -bdx - 1;
            if (bdx < -5) bdx = -5;
            int rel = by - ry;
            bdy = rel < PONG_PAD_H / 3 ? -2 : (rel > 2 * PONG_PAD_H / 3 ? 2 : (bdy > 0 ? 1 : -1));
        }
        if (bx + PONG_BALL < 0) {
            rs++;
            bx = GFX_WIDTH / 2; by = (PONG_TOP + PONG_BOTTOM) / 2;
            bdx = 2; bdy = 1;
            pit_sleep_ms(500);
        }
        if (bx > GFX_WIDTH) {
            ls++;
            bx = GFX_WIDTH / 2; by = (PONG_TOP + PONG_BOTTOM) / 2;
            bdx = -2; bdy = -1;
            pit_sleep_ms(500);
        }
    }

    if (!exit_game) {
        pong_draw_paddles(ly, ry, bx, by, ls, rs);
        center_text(88, ls > rs ? "YOU WIN!" : "CPU WINS", ls > rs ? GUI_GREEN : GUI_RED);
        center_text(110, "R restart   ESC quit", GUI_WHITE);
        gfx_present();
        for (;;) {
            int k = kbd_getchar();
            if (k == KEY_ESC) break;
            if (k == 'r' || k == 'R') {
                gfx_leave();
                task_unregister(pid);
                return cmd_pong(argc, argv);
            }
            pit_sleep_ms(10);
        }
    }
    gfx_leave();
    task_unregister(pid);
    kprintf("pong: closed (%d : %d)\n", ls, rs);
    return 0;
}

/* ------------------------------------------------------------------------- */
/* doom - simple 2.5D raycaster FPS                                           */
/* ------------------------------------------------------------------------- */

/* ------------------------------------------------------------------------- */
/* doom - simple 2.5D raycaster FPS                                           */
/* ------------------------------------------------------------------------- */

/* DOOM - enhanced 2.5D raycasting FPS (fixed-point, no floats) */

#define DOOM_MAP_W 16
#define DOOM_MAP_H 16
#define DOOM_FOV   66

static const char doom_map[DOOM_MAP_H][DOOM_MAP_W + 1] = {
    "1111111111111111",
    "1..............1",
    "1..2222....33..1",
    "1..2.......3...1",
    "1..2..E....3...1",
    "1......22......1",
    "1..E...22...E..1",
    "1..............1",
    "1...333..222...1",
    "1...3......2...1",
    "1...3..E...2...1",
    "1..............1",
    "1..22......33..1",
    "1..E........E..1",
    "1..............1",
    "1111111111111111"
};

typedef struct { int x, y; bool alive; int flash; int phase; } doom_enemy_t;
typedef struct { int x, y; bool taken; } doom_item_t;

static void doom_beep(u32 freq, u32 ms) {
    if (freq < 20) freq = 20;
    u32 div = 1193180u / freq;
    outb(0x43, 0xB6);
    outb(0x42, (u8)(div & 0xFF));
    outb(0x42, (u8)((div >> 8) & 0xFF));
    u8 port = inb(0x61);
    outb(0x61, (u8)(port | 0x03));
    pit_sleep_ms(ms);
    outb(0x61, (u8)(port & 0xFC));
}

static int doom_abs(int v) { return v < 0 ? -v : v; }

/* coarse atan2 in degrees (0..359) for sprite billboarding */
static int doom_atan2(int y, int x) {
    int ax = doom_abs(x), ay = doom_abs(y);
    int deg;
    if (ax >= ay) deg = (ay * 45) / (ax + 1);
    else deg = 45 + (ax * 45) / (ay + 1);
    if (x >= 0 && y >= 0) return deg;
    if (x < 0  && y >= 0) return 180 - deg;
    if (x < 0  && y < 0)  return 180 + deg;
    return 360 - deg;
}

/* Fixed-point trig: angle 0..359 degrees, scale 1024 (Bhaskara approximation) */
static int doom_sin(int deg) {
    deg %= 360; if (deg < 0) deg += 360;
    int q = deg % 90, sign = 1, x;
    if (deg < 90)      { x = q;      sign = 1; }
    else if (deg < 180){ x = 90 - q; sign = 1; }
    else if (deg < 270){ x = q;      sign = -1; }
    else               { x = 90 - q; sign = -1; }
    int v = (4 * x * (180 - x) * 1024) / (40500 - x * (180 - x));
    return sign * v;
}
static int doom_cos(int deg) { return doom_sin(deg + 90); }

/* 16x16 monster sprite. '.' = transparent. */
static const char *doom_sprite[16] = {
    "................",
    ".....1111.......",
    "....122221......",
    "...12222221.....",
    "..1221221221....",
    "..1221221221....",
    "..1222222221....",
    "...12233221.....",
    "....122221......",
    "...13333331.....",
    "..1334444331....",
    "..1334444331....",
    "..1334433441....",
    "...13333331.....",
    "...14....41.....",
    "..11......11...."
};
static u8 doom_sprite_color(char c) {
    if (c == '1') return GUI_RED;
    if (c == '2') return GUI_MAGENTA;
    if (c == '3') return GUI_YELLOW;
    if (c == '4') return GUI_WHITE;
    return 0;
}

/* Wall texture colour: cell selects material, tx/ty are 0..63, shade 0..2 */
static u8 doom_wall_tex(char cell, int tx, int ty, int shade) {
    int row = ty >> 3;
    int bx = (tx + ((row & 1) ? 16 : 0)) & 31;
    bool mortar = (ty & 7) == 0 || bx == 0;
    u8 base, dark;
    if (cell == '2')      { base = GUI_CYAN;  dark = GUI_BLUE; }   /* metal */
    else if (cell == '3') { base = GUI_TITLE; dark = 2; }          /* stone */
    else                  { base = GUI_GREY;  dark = 8; }          /* brick */
    u8 c = mortar ? dark : base;
    if (!mortar && ((tx ^ ty) & 31) < 3) c = dark;                  /* grain */
    if (shade >= 2) c = (c == GUI_CYAN) ? 2 : 1;
    else if (shade == 1) c = dark;
    return c;
}

static bool doom_solid(int gx, int gy) {
    if (gx < 0 || gy < 0 || gx >= DOOM_MAP_W || gy >= DOOM_MAP_H) return true;
    return doom_map[gy][gx] == '1' || doom_map[gy][gx] == '2' || doom_map[gy][gx] == '3';
}

static bool doom_can_see(int px, int py, int ex, int ey) {
    int dx = ex - px, dy = ey - py;
    int steps = doom_abs(dx) > doom_abs(dy) ? doom_abs(dx) / 128 : doom_abs(dy) / 128;
    if (steps < 1) steps = 1;
    for (int i = 1; i < steps; i++) {
        int sx = px + (dx * i) / steps;
        int sy = py + (dy * i) / steps;
        if (doom_solid(sx / 1024, sy / 1024)) return false;
    }
    return true;
}

static void doom_try_move(int *px, int *py, int dx, int dy) {
    /* sliding collision: test X and Y independently with a small radius */
    const int R = 220;
    int nx = *px + dx;
    if (!doom_solid((nx - R) / 1024, (*py - R) / 1024) &&
        !doom_solid((nx + R) / 1024, (*py - R) / 1024) &&
        !doom_solid((nx - R) / 1024, (*py + R) / 1024) &&
        !doom_solid((nx + R) / 1024, (*py + R) / 1024)) *px = nx;
    int ny = *py + dy;
    if (!doom_solid((*px - R) / 1024, (ny - R) / 1024) &&
        !doom_solid((*px + R) / 1024, (ny - R) / 1024) &&
        !doom_solid((*px - R) / 1024, (ny + R) / 1024) &&
        !doom_solid((*px + R) / 1024, (ny + R) / 1024)) *py = ny;
}

int cmd_doom(int argc, char **argv) {
    UNUSED(argc); UNUSED(argv);
    if (!store_module_bound(MOD_ENTRY_DOOM)) {
        kprintf("doom: not installed. Run: minipacket -I doom\n");
        return -1;
    }
    int pid = task_register("doom", 128, true);
    task_set_state(pid, "running");
    gfx_enter();
    mouse_set_bounds(GFX_WIDTH, GFX_HEIGHT);

    int px = 3 * 1024 + 512, py = 3 * 1024 + 512;
    int angle = 0, bob = 0;
    int health = 100, armor = 0, ammo = 48, score = 0, level = 1;
    bool running = true, dead = false;
    int fire_cool = 0, damage_cool = 0, dmg_flash = 0, msg_timer = 0;
    char message[40] = "";
    int zbuf[GFX_WIDTH];

    doom_enemy_t enemies[16];
    doom_item_t items[8];
    int ec = 0, ic = 0;
    for (int y = 0; y < DOOM_MAP_H && ec < 16; y++)
        for (int x = 0; x < DOOM_MAP_W && ec < 16; x++)
            if (doom_map[y][x] == 'E') {
                enemies[ec].x = x * 1024 + 512; enemies[ec].y = y * 1024 + 512;
                enemies[ec].alive = true; enemies[ec].flash = 0; enemies[ec].phase = 100; ec++;
            }
    /* health/armor pickups in open corridors */
    int item_cells[8][2] = {{1,1},{14,1},{1,14},{14,14},{8,1},{1,7},{14,7},{8,14}};
    for (int i = 0; i < 8; i++) {
        items[i].x = item_cells[i][0] * 1024 + 512;
        items[i].y = item_cells[i][1] * 1024 + 512;
        items[i].taken = false;
    }
    ic = 8;

    mouse_state_t prevm; mouse_get_state(&prevm);

    while (running && !dead) {
        mouse_state_t m; mouse_get_state(&m);
        int mdx = m.x - prevm.x; prevm = m;
        angle += mdx / 2;
        while (angle < 0) angle += 360;
        while (angle >= 360) angle -= 360;

        int dirX = doom_cos(angle), dirY = doom_sin(angle);
        int planeX = (-dirY * 600) / 1024, planeY = (dirX * 600) / 1024;
        bool moved = false;

        int k = kbd_getchar();
        while (k >= 0) {
            if (k == KEY_ESC) { running = false; break; }
            if (k == KEY_LEFT)  { angle -= 7; }
            if (k == KEY_RIGHT) { angle += 7; }
            int step = kbd_shift_down() ? 170 : 105;
            int mvx = 0, mvy = 0;
            if (k == KEY_UP || k == 'w' || k == 'W') { mvx += (dirX*step)/1024; mvy += (dirY*step)/1024; }
            if (k == KEY_DOWN || k == 's' || k == 'S') { mvx -= (dirX*step)/1024; mvy -= (dirY*step)/1024; }
            if (k == 'a' || k == 'A') { mvx += (dirY*step)/1024; mvy -= (dirX*step)/1024; }
            if (k == 'd' || k == 'D') { mvx -= (dirY*step)/1024; mvy += (dirX*step)/1024; }
            if (mvx || mvy) { doom_try_move(&px, &py, mvx, mvy); moved = true; }
            if (k == ' ' || (m.buttons & 1)) {
                if (fire_cool == 0 && ammo > 0) {
                    ammo--; fire_cool = 8; doom_beep(200, 12);
                    doom_beep(120, 18);
                    /* hitscan: pick closest enemy in front within range */
                    int best = -1, bestd = 1 << 20;
                    for (int i = 0; i < ec; i++) if (enemies[i].alive) {
                        int ex = enemies[i].x - px, ey = enemies[i].y - py;
                        int d = doom_abs(ex) + doom_abs(ey);
                        if (d < bestd && d < 8 * 1024 && doom_can_see(px, py, enemies[i].x, enemies[i].y)) {
                            int ea = 0;
                            if (doom_abs(ex) > doom_abs(ey)) ea = ex > 0 ? 0 : 180;
                            else ea = ey > 0 ? 90 : 270;
                            int diff = doom_abs(ea - angle); if (diff > 180) diff = 360 - diff;
                            if (diff < 30) { best = i; bestd = d; }
                        }
                    }
                    if (best >= 0) {
                        enemies[best].flash = 4;
                        int dmg = 25 + (int)(rand() % 20);
                        score += 50 + dmg;
                        strncpy(message, "HIT!", sizeof(message)); message[sizeof(message)-1]=0; msg_timer = 30;
                        enemies[best].phase -= dmg;
                        if (enemies[best].phase <= 0) {
                            enemies[best].alive = false; score += 200;
                            strncpy(message, "ENEMY DOWN +200", sizeof(message)); message[sizeof(message)-1]=0; msg_timer = 45;
                            doom_beep(90, 40);
                        }
                    } else {
                        strncpy(message, "MISS", sizeof(message)); message[sizeof(message)-1]=0; msg_timer = 14;
                    }
                }
            }
            if (k == 'r' || k == 'R') { ammo = 48; strncpy(message,"AMMO REFILLED",sizeof(message)); message[sizeof(message)-1]=0; msg_timer=30; }
            k = kbd_getchar();
        }
        while (angle < 0) angle += 360;
        while (angle >= 360) angle -= 360;
        dirX = doom_cos(angle); dirY = doom_sin(angle);
        planeX = (-dirY * 600) / 1024; planeY = (dirX * 600) / 1024;
        if (moved) bob += (int)(doom_sin((int)(pit_ticks() * 6 % 360)) / 90); else bob = 0;

        /* ---------- render ---------- */
        gfx_clear(1);
        int horizon = GFX_HEIGHT / 2 + (bob >> 1);
        /* floor / ceiling gradient */
        for (int y = 0; y < GFX_HEIGHT; y++) {
            int dy = y - horizon;
            u8 c;
            if (dy < 0) c = (dy < -50) ? 0 : 1;                 /* ceiling */
            else c = (dy > 50) ? 8 : 13;                         /* floor */
            gfx_hline(0, y, GFX_WIDTH, c);
        }

        for (int x = 0; x < GFX_WIDTH; x++) {
            int cameraX = (2 * x * 1024) / GFX_WIDTH - 1024;
            int rdx = dirX + (planeX * cameraX) / 1024;
            int rdy = dirY + (planeY * cameraX) / 1024;
            int mapX = px / 1024, mapY = py / 1024;
            int ddx = rdx ? (1024 * 1024) / doom_abs(rdx) : (1 << 22);
            int ddy = rdy ? (1024 * 1024) / doom_abs(rdy) : (1 << 22);
            int sdx = rdx < 0 ? ((px - mapX * 1024) * ddx) / 1024 : (((mapX + 1) * 1024 - px) * ddx) / 1024;
            int sdy = rdy < 0 ? ((py - mapY * 1024) * ddy) / 1024 : (((mapY + 1) * 1024 - py) * ddy) / 1024;
            int side = 0, hitcell = '1', guard = 0;
            while (guard++ < 64) {
                if (sdx < sdy) { sdx += ddx; mapX += rdx < 0 ? -1 : 1; side = 0; }
                else           { sdy += ddy; mapY += rdy < 0 ? -1 : 1; side = 1; }
                if (doom_solid(mapX, mapY)) { hitcell = doom_map[mapY][mapX]; break; }
            }
            int perp = side == 0 ? (sdx - ddx) : (sdy - ddy);
            if (perp < 256) perp = 256;
            zbuf[x] = perp;
            int lh = (200 * 1024) / perp;
            if (lh > 240) lh = 240;
            int y0 = horizon - lh / 2;
            if (y0 < 0) y0 = 0;
            int y1 = horizon + lh / 2;
            if (y1 > GFX_HEIGHT) y1 = GFX_HEIGHT;
            /* exact hit coordinate for texture */
            int wallx = side == 0 ? (py + (perp * rdy) / 1024) : (px + (perp * rdx) / 1024);
            wallx -= (wallx / 1024) * 1024;
            int tx = (wallx * 64) / 1024;
            if ((side == 0 && rdx > 0) || (side == 1 && rdy < 0)) tx = 63 - tx;
            int shade = perp > 5 * 1024 ? 2 : (perp > 2 * 1024 ? 1 : 0);
            if (side == 1 && shade < 2) shade++;
            for (int y = y0; y < y1; y++) {
                int ty = ((y - (horizon - lh / 2)) * 64) / lh;
                if (ty < 0) ty = 0;
                if (ty > 63) ty = 63;
                gfx_pixel(x, y, doom_wall_tex(hitcell, tx, ty, shade));
            }
        }

        /* ---------- enemies (billboard sprites with depth test) ---------- */
        for (int i = 0; i < ec; i++) if (enemies[i].alive) {
            int ex = enemies[i].x - px, ey = enemies[i].y - py;
            int edist = (doom_abs(ex) + doom_abs(ey)) * 3 / 4;
            if (edist < 256) continue;
            int ea = doom_atan2(ey, ex);
            int rel = ea - angle;
            if (rel > 180) rel -= 360;
            if (rel < -180) rel += 360;
            if (doom_abs(rel) > 36) continue;
            int screenX = GFX_WIDTH / 2 + (rel * (GFX_WIDTH / 2)) / 33;
            int ty = edist;
            int sh = (200 * 1024) / ty;
            if (sh > 150) sh = 150;
            int sw = sh;
            int sy = horizon - sh / 2;
            for (int col = 0; col < 16; col++) {
                int sx = screenX + (col - 8) * sw / 16;
                if (sx < 0 || sx >= GFX_WIDTH || ty >= zbuf[sx]) continue;
                for (int row = 0; row < 16; row++) {
                    u8 c = doom_sprite_color(doom_sprite[row][col]);
                    if (!c) continue;
                    if (enemies[i].flash > 0) c = GUI_WHITE;
                    int yy = sy + row * sh / 16;
                    gfx_pixel(sx, yy, c);
                }
            }
            /* health bar */
            if (enemies[i].phase > 0) {
                int pct = enemies[i].phase;
                if (pct > 100) pct = 100;
                int hb = (pct * 16) / 100;
                gfx_fill_rect(screenX - 8, sy - 3, 16, 2, GUI_RED);
                gfx_fill_rect(screenX - 8, sy - 3, hb, 2, GUI_GREEN);
            }
            /* simple chase AI */
            if (ty < 7 * 1024 && doom_can_see(px, py, enemies[i].x, enemies[i].y)) {
                int dx = px - enemies[i].x, dy = py - enemies[i].y;
                int dl = doom_abs(dx) + doom_abs(dy);
                if (dl > 512) {
                    int sp = 40;
                    int mx = (dx * sp) / dl, my = (dy * sp) / dl;
                    doom_try_move(&enemies[i].x, &enemies[i].y, mx, my);
                }
                if (dl < 1100 && damage_cool == 0 && enemies[i].flash <= 0) {
                    int dmg = 6;
                    if (armor > 0) { int a = dmg / 2; armor -= a; dmg -= a; }
                    health -= dmg; damage_cool = 30; dmg_flash = 12;
                    strncpy(message, "OUCH!", sizeof(message)); message[sizeof(message)-1]=0; msg_timer = 20;
                    doom_beep(70, 30);
                }
            }
            if (enemies[i].flash > 0) enemies[i].flash--;
        }

        /* ---------- items ---------- */
        for (int i = 0; i < ic; i++) if (!items[i].taken) {
            int ex = items[i].x - px, ey = items[i].y - py;
            int d = doom_abs(ex) + doom_abs(ey);
            if (d < 400) {
                items[i].taken = true;
                if (i & 1) { armor += 25; strncpy(message,"+25 ARMOR",sizeof(message)); }
                else       { health += 20; if (health>100) health=100; strncpy(message,"+20 HEALTH",sizeof(message)); }
                message[sizeof(message)-1]=0; msg_timer = 40; doom_beep(440, 40); doom_beep(660, 50);
            } else if (d < 9 * 1024 && doom_can_see(px, py, items[i].x, items[i].y)) {
                int idist = (doom_abs(ex) + doom_abs(ey)) * 3 / 4;
                if (idist > 256) {
                    int ia = doom_atan2(ey, ex);
                    int ir = ia - angle;
                    if (ir > 180) ir -= 360;
                    if (ir < -180) ir += 360;
                    if (doom_abs(ir) <= 36) {
                        int scr = GFX_WIDTH/2 + (ir * (GFX_WIDTH/2)) / 33;
                        int isz = (200 * 512) / idist; if (isz > 14) isz = 14;
                        if (isz < 2) isz = 2;
                        gfx_fill_rect(scr - isz/2, horizon + 6, isz, isz, (i&1)?GUI_BLUE:GUI_GREEN);
                        gfx_frame(scr - isz/2, horizon + 6, isz, isz, GUI_WHITE);
                    }
                }
            }
        }

        /* ---------- weapon + HUD ---------- */
        /* crosshair */
        gfx_hline(GFX_WIDTH/2 - 3, GFX_HEIGHT/2, 7, GUI_GREEN);
        gfx_vline(GFX_WIDTH/2, GFX_HEIGHT/2 - 3, 7, GUI_GREEN);
        /* weapon (simple pistol) */
        gfx_fill_rect(GFX_WIDTH/2 - 16, GFX_HEIGHT - 34, 32, 12, GUI_GREY);
        gfx_fill_rect(GFX_WIDTH/2 - 8, GFX_HEIGHT - 40, 16, 8, 1);
        gfx_fill_rect(GFX_WIDTH/2 - 4, GFX_HEIGHT - 26, 8, 14, GUI_TITLE);
        if (fire_cool > 4) { gfx_fill_rect(GFX_WIDTH/2 - 6, GFX_HEIGHT - 42, 12, 6, GUI_YELLOW); }

        gfx_fill_rect(0, GFX_HEIGHT - 12, GFX_WIDTH, 12, GUI_TASKBAR);
        /* health bar */
        gfx_text(3, GFX_HEIGHT - 10, "HP", GUI_WHITE);
        gfx_fill_rect(19, GFX_HEIGHT - 10, 60, 7, GUI_BLACK);
        gfx_fill_rect(20, GFX_HEIGHT - 9, (58 * health) / 100, 5, health > 40 ? GUI_GREEN : GUI_RED);
        /* armor */
        gfx_text(84, GFX_HEIGHT - 10, "AR", GUI_WHITE);
        gfx_fill_rect(100, GFX_HEIGHT - 10, 40, 7, GUI_BLACK);
        gfx_fill_rect(101, GFX_HEIGHT - 9, (38 * armor) / 100, 5, GUI_CYAN);
        char hud[48];
        ksnprintf(hud, sizeof(hud), "AMMO %d  SCORE %d  LV %d", ammo, score, level);
        gfx_text(146, GFX_HEIGHT - 10, hud, GUI_WHITE);

        /* minimap top-right */
        int mx0 = GFX_WIDTH - 52, my0 = 2;
        gfx_fill_rect(mx0 - 1, my0 - 1, 50, 50, GUI_BLACK);
        for (int yy = 0; yy < DOOM_MAP_H; yy++)
            for (int xx = 0; xx < DOOM_MAP_W; xx++) {
                u8 c = doom_solid(xx, yy) ? GUI_GREY : 1;
                if (doom_map[yy][xx] == 'E') c = GUI_RED;
                gfx_pixel(mx0 + xx * 3, my0 + yy * 3, c);
            }
        gfx_pixel(mx0 + (px/1024)*3, my0 + (py/1024)*3, GUI_YELLOW);

        if (msg_timer > 0) {
            center_text(40, message, GUI_YELLOW);
            msg_timer--;
        }
        if (dmg_flash > 0) {
            /* dithered red damage vignette */
            for (int y = 0; y < GFX_HEIGHT; y += 2)
                for (int x = 0; x < GFX_WIDTH; x += 2)
                    gfx_pixel(x, y, GUI_RED);
            dmg_flash--;
        }
        gfx_present();

        if (fire_cool > 0) fire_cool--;
        if (damage_cool > 0) damage_cool--;
        if (health <= 0) dead = true;

        int alive = 0;
        for (int i = 0; i < ec; i++) if (enemies[i].alive) alive++;
        if (alive == 0) {
            center_text(78, "LEVEL CLEARED!", GUI_GREEN);
            char s[32]; ksnprintf(s, sizeof(s), "SCORE %d", score);
            center_text(92, s, GUI_WHITE);
            gfx_present();
            pit_sleep_ms(1200);
            level++;
            /* respawn enemies with more hp */
            for (int i = 0; i < ec; i++) {
                enemies[i].alive = true; enemies[i].flash = 0;
                enemies[i].phase = 100 + (level - 1) * 30;
            }
            for (int i = 0; i < ic; i++) items[i].taken = false;
            health = 100; armor = 25; ammo = 48;
            px = 3 * 1024 + 512; py = 3 * 1024 + 512; angle = 0;
        }
        pit_sleep_ms(25);
    }

    if (dead) {
        center_text(84, "YOU DIED", GUI_RED);
        char s[32]; ksnprintf(s, sizeof(s), "FINAL SCORE %d", score);
        center_text(100, s, GUI_WHITE);
        gfx_present();
        doom_beep(60, 300);
        pit_sleep_ms(1000);
    }
    gfx_leave();
    task_unregister(pid);
    kprintf("doom: closed (score %d, hp %d, level %d)\n", score, health, level);
    return 0;
}
