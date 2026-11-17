#pragma once
#include "types.h"

enum {
    ICON_TERM = 0, ICON_NOTEPAD, ICON_TASKS, ICON_NET, ICON_BROWSER,
    ICON_EXPLORER, ICON_DOOM, ICON_SNAKE, ICON_PONG, ICON_START,
    ICON_DISK, ICON_FOLDER, ICON_FILE, ICON_SPEAKER, ICON_MUSIC, ICON_COUNT
};

/* Draw a 16x16 bitmap icon at (x,y). 'X' uses color, 'o' white,
 * '#' dark, '.' transparent. */
void gfx_icon(int x, int y, int icon, u8 color);
