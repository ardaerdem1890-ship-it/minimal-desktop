#pragma once
#include "types.h"

#define TASK_MAX 24
#define TASK_NAME_MAX 24

typedef struct {
    bool used;
    int  pid;
    char name[TASK_NAME_MAX];
    char state[12];
    u32  mem_kb;
    u32  ticks;
    bool killable;
    bool is_kernel;
} task_info_t;

void task_init(void);
int  task_register(const char *name, u32 mem_kb, bool killable);
void task_unregister(int pid);
void task_set_state(int pid, const char *state);
void task_set_current(int pid);
int  task_current(void);
void task_tick(void);
task_info_t *task_get(int index);
task_info_t *task_find(int pid);
int  task_count(void);
u32  task_total_ticks(void);
void task_print_table(void);
int  task_kill(int pid);
bool task_selftest(char *err, int errlen);
