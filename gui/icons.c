/* Minimal Desktop - 16x16 bitmap icon set for the compositor */
#include "icons.h"
#include "gfx.h"
#include "gui.h"

/* Each row: '.' transparent, 'X' main colour, 'o' white, '#' dark outline */
static const char *icons[ICON_COUNT][16] = {
    /* terminal */
    {
        "################",
        "#XXXXXXXXXXXXXX#",
        "#XooooooooooooX#",
        "#Xo..........oX#",
        "#Xo.XXX.XXX..oX#",
        "#Xo.X.o.o.X..oX#",
        "#Xo.XXX.XXX..oX#",
        "#Xo..........oX#",
        "#Xo.XXXXX....oX#",
        "#Xo.X...X....oX#",
        "#Xo.XXXXX....oX#",
        "#Xo..........oX#",
        "#XooooooooooooX#",
        "#XXXXXXXXXXXXXX#",
        ".##############.",
        "................"
    },
    /* notepad */
    {
        "..############..",
        ".#XXXXXXXXXXXX#.",
        ".#XooooooooooX#.",
        ".#Xo........oX#.",
        ".#Xo.oooooo.oX#.",
        ".#Xo........oX#.",
        ".#Xo.oooooo.oX#.",
        ".#Xo........oX#.",
        ".#Xo.oooooo.oX#.",
        ".#Xo........oX#.",
        ".#Xo.ooo....oX#.",
        ".#Xo........oX#.",
        ".#XooooooooooX#.",
        ".#XXXXXXXXXXXX#.",
        "..############..",
        "................"
    },
    /* tasks */
    {
        "################",
        "#XXXXXXXXXXXXXX#",
        "#Xo#X#X#X#X#XoX#",
        "#Xo..........oX#",
        "#XoXX.oXX.oXXoX#",
        "#XoXX.oXX.oXXoX#",
        "#XoXX.oXX.oXXoX#",
        "#Xo..........oX#",
        "#XoXX.oXX.oXXoX#",
        "#XoXX.oXX.oXXoX#",
        "#XoXX.oXX.oXXoX#",
        "#Xo..........oX#",
        "#XooooooooooooX#",
        "#XXXXXXXXXXXXXX#",
        ".##############.",
        "................"
    },
    /* network */
    {
        "................",
        ".....XXXX.......",
        "....XooooX......",
        "...Xo.oo.oX.....",
        ".....XooX.......",
        "..XXXoXXoXXX....",
        ".XoooXooXoooX...",
        ".Xo..XooX..oX...",
        ".XooooooooooX...",
        ".Xo..XooX..oX...",
        ".XoooXooXoooX...",
        "..XXXoXXoXXX....",
        ".....XooX.......",
        "...Xo.oo.oX.....",
        "....XooooX......",
        ".....XXXX......."
    },
    /* browser globe */
    {
        ".....XXXXXX.....",
        "...XXooooooXX...",
        "..Xooo#oo#oooX..",
        ".Xooo.#oo#.oooX.",
        ".Xoo..XooX..ooX.",
        "Xoo...XooX...ooX",
        "XoXXXXXXXXXXXXoX",
        "XoXXXXXXXXXXXXoX",
        "Xoo...XooX...ooX",
        ".Xoo..XooX..ooX.",
        ".Xooo.#oo#.oooX.",
        "..Xooo#oo#oooX..",
        "...XXooooooXX...",
        ".....XXXXXX.....",
        "................",
        "................"
    },
    /* folder */
    {
        "................",
        "..XXX...........",
        ".XoooX..........",
        ".XooooXXXXXXXX..",
        ".XooooXooooooX..",
        ".XooooooooooooX.",
        ".XooooooooooooX.",
        ".XooooooooooooX.",
        ".XooooooooooooX.",
        ".XooooooooooooX.",
        ".XooooooooooooX.",
        ".XooooooooooooX.",
        "..XooooooooooX..",
        "..XXXXXXXXXXXX..",
        "................",
        "................"
    },
    /* doom monster */
    {
        "................",
        "....XXX..XXX....",
        "...XoooXXoooX...",
        "..XooooooooooX..",
        "..XoXXooooXXoX..",
        "..XoXXooooXXoX..",
        "..XooooooooooX..",
        "...XoXooooXoX...",
        "....XooooooX....",
        "...XXoXXXXoXX...",
        "..XoooXooXoooX..",
        "..XoXoXooXoXoX..",
        "..XoXoXooXoXoX..",
        "...XX......XX...",
        "..XX........XX..",
        "................"
    },
    /* snake */
    {
        "................",
        "................",
        "...XXXX.........",
        "..XooooXX.......",
        "..Xo....X.......",
        "..Xo.XX.X.......",
        "..Xo.Xo.X.......",
        "..Xo.Xo.X.......",
        ".....Xo.X.......",
        ".....Xo.X.......",
        ".....XoooX......",
        ".......XooX.....",
        "........XoX.....",
        ".......XX.......",
        "................",
        "................"
    },
    /* pong */
    {
        "................",
        "#..............#",
        "#..............#",
        "#..............#",
        "#.....oo.......#",
        "#..............#",
        "#..............#",
        "#..............#",
        "#..........oo..#",
        "#..............#",
        "#..............#",
        "#..............#",
        "#..............#",
        "#..............#",
        "################",
        "................"
    },
    /* start logo */
    {
        "................",
        "................",
        "..XX........XX..",
        ".XooX......XooX.",
        ".XooXX....XXooX.",
        ".XoooX....XoooX.",
        ".XoooXX..XXoooX.",
        ".XooooXXXXooooX.",
        ".XoooXX..XXoooX.",
        ".XoooX....XoooX.",
        ".XooXX....XXooX.",
        ".XooX......XooX.",
        "..XX........XX..",
        "................",
        "................",
        "................"
    },
    /* disk */
    {
        ".##############.",
        "#XXXXXXXXXXXXXX#",
        "#Xooooooooo##oX#",
        "#XoXXXXXoXX#oX#",
        "#XoXoooXoXX#oX#",
        "#XoXoooXoXX#oX#",
        "#XoXoooXoXX#oX#",
        "#XoXoooXoXX#oX#",
        "#XoXoooXoXX#oX#",
        "#XoXXXXXoXX#oX#",
        "#Xooooooooo##oX#",
        "#XooXXXXXoooXX#",
        "#XoXoooooXoXoo#",
        "#XoXoooooXoXoo#",
        "#XXXXXXXXXXXXXX#",
        ".##############."
    },
    /* folder small */
    {
        "................",
        "...XXX..........",
        "..XoooX.........",
        "..XooooXXXXXXX..",
        "..XooooooooooX..",
        "..XooooooooooX..",
        "..XooooooooooX..",
        "..XooooooooooX..",
        "..XooooooooooX..",
        "..XooooooooooX..",
        "..XooooooooooX..",
        "..XooooooooooX..",
        "...XooooooooX...",
        "...XXXXXXXXXX...",
        "................",
        "................"
    },
    /* file */
    {
        "..XXXXX.........",
        "..XoooXX........",
        "..XooooX........",
        "..XoooX.........",
        "..XoooX.........",
        "..XooooX........",
        "..XoooooX.......",
        "..XooooooX......",
        "..XoooooooX.....",
        "..XooooooooX....",
        "..XoooooooooX...",
        "..XooooooooooX..",
        "..XoooooooooooX.",
        "..XXXXXXXXXXXXX.",
        "................",
        "................"
    },
    /* speaker */
    {
        "................",
        "......XXX.......",
        ".....XoooX......",
        "..XXXoooXX......",
        ".XoooooooX......",
        ".XoooooooX......",
        ".XoooooooX......",
        ".XoooooooX......",
        "..XXXoooXX......",
        ".....XoooX......",
        "......XXX.......",
        "................",
        "................",
        "................",
        "................",
        "................"
    },
    /* music note */
    {
        "................",
        "........XXX.....",
        ".......XoooX....",
        "......XoooX.....",
        "......XoooX.....",
        "......XoooX.....",
        "......XoooX.....",
        "......XoooX.....",
        "....XXoooXX.....",
        "...XooooooooX...",
        "..XoooXoooXoooX.",
        "..XoooXoooXoooX.",
        "..XoooXoooXoooX.",
        "...XooXoooXooX..",
        "....XX..X..XX...",
        "........X......."
    }
};

void gfx_icon(int x, int y, int icon, u8 color) {
    if (icon < 0 || icon >= ICON_COUNT) return;
    const char *const *b = icons[icon];
    for (int row = 0; row < 16; row++) {
        for (int col = 0; col < 16; col++) {
            char c = b[row][col];
            u8 v;
            if (c == 'X') v = color;
            else if (c == 'o') v = GUI_WHITE;
            else if (c == '#') v = GUI_TEXT;
            else continue;
            gfx_pixel(x + col, y + row, v);
        }
    }
}
