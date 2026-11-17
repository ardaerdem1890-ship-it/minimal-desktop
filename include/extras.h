#pragma once
#include "types.h"

/* Screensaver modes */
void screensaver_run(int mode, int seconds);
/* mode: 0=matrix 1=starfield 2=logo 3=plasma */

/* Music player: parse note file and play via PC speaker */
int music_play(const char *path);

/* Theme persistence */
void theme_save(void);
void theme_load(void);

/* Backup / restore RAMFS to /disk */
int backup_create(const char *path);
int backup_restore(const char *path);

/* Slideshow of /pictures in mode 13h */
int slideshow_run(int seconds);
int chkdsk_fat(void);
int login_run(void);
