#pragma once
#include "types.h"

/* Palette indices used by the desktop */
#define GUI_BLACK       0
#define GUI_WHITE       15
#define GUI_DARK        1
#define GUI_BLUE        2
#define GUI_CYAN        3
#define GUI_RED         4
#define GUI_MAGENTA     5
#define GUI_GREEN       6
#define GUI_YELLOW      7
#define GUI_TASKBAR     8
#define GUI_TITLE       9
#define GUI_TITLE_LT    10
#define GUI_PANEL       11
#define GUI_TEXT        12
#define GUI_GREY        13
#define GUI_DESK1       16
#define GUI_DESK2       17
#define GUI_DESK3       18
#define GUI_CURSOR      19

#define GUI_LAUNCH_NONE  0
#define GUI_LAUNCH_SNAKE 1
#define GUI_LAUNCH_PONG  2
#define GUI_LAUNCH_DOOM  3

int  gui_run(void);
int  gui_run_miniland(void);
bool gui_selftest(char *err, int errlen);
