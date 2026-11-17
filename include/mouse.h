#pragma once
#include "types.h"

typedef struct {
    int x;
    int y;
    u8  buttons;
    bool present;
    bool irq_seen;
} mouse_state_t;

void mouse_init(void);
void mouse_get_state(mouse_state_t *st);
void mouse_set_bounds(int w, int h);
u32  mouse_packets(void);
bool mouse_selftest(char *err, int errlen);
