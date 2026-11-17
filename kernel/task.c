/* Minimal Desktop - cooperative task registry used by taskmng and the GUI */
#include "task.h"
#include "console.h"
#include "string.h"
#include "pit.h"

static task_info_t tasks[TASK_MAX];
static int next_pid = 1;
static int current_pid = 0;
static u32 total_ticks;

void task_init(void) {
    memset(tasks, 0, sizeof(tasks));
    next_pid = 1;
    current_pid = 0;
    total_ticks = 0;
}

int task_register(const char *name, u32 mem_kb, bool killable) {
    for (int i = 0; i < TASK_MAX; i++) {
        if (!tasks[i].used) {
            memset(&tasks[i], 0, sizeof(tasks[i]));
            tasks[i].used = true;
            tasks[i].pid = next_pid++;
            strncpy(tasks[i].name, name, TASK_NAME_MAX - 1);
            strncpy(tasks[i].state, "ready", 11);
            tasks[i].mem_kb = mem_kb;
            tasks[i].killable = killable;
            tasks[i].is_kernel = !killable;
            tasks[i].ticks = 0;
            return tasks[i].pid;
        }
    }
    return -1;
}

void task_unregister(int pid) {
    for (int i = 0; i < TASK_MAX; i++) {
        if (tasks[i].used && tasks[i].pid == pid) {
            if (tasks[i].killable) {
                tasks[i].used = false;
                if (current_pid == pid) current_pid = 0;
            }
            return;
        }
    }
}

void task_set_state(int pid, const char *state) {
    task_info_t *t = task_find(pid);
    if (t) strncpy(t->state, state, sizeof(t->state) - 1);
}

void task_set_current(int pid) { current_pid = pid; }
int task_current(void) { return current_pid; }

void task_tick(void) {
    total_ticks++;
    task_info_t *t = task_find(current_pid);
    if (t) t->ticks++;
}

task_info_t *task_get(int index) {
    if (index < 0 || index >= TASK_MAX) return NULL;
    return tasks[index].used ? &tasks[index] : NULL;
}

task_info_t *task_find(int pid) {
    if (pid <= 0) return NULL;
    for (int i = 0; i < TASK_MAX; i++)
        if (tasks[i].used && tasks[i].pid == pid) return &tasks[i];
    return NULL;
}

int task_count(void) {
    int n = 0;
    for (int i = 0; i < TASK_MAX; i++) if (tasks[i].used) n++;
    return n;
}

u32 task_total_ticks(void) { return total_ticks; }

void task_print_table(void) {
    kprintf(" PID  NAME                 STATE      MEM     TICKS   TYPE\n");
    kprintf(" ---  -------------------  ---------  ------  ------  --------\n");
    for (int i = 0; i < TASK_MAX; i++) {
        if (!tasks[i].used) continue;
        kprintf(" %-4d %-20s %-10s %5uK  %6u  %s\n",
                tasks[i].pid, tasks[i].name, tasks[i].state, tasks[i].mem_kb,
                tasks[i].ticks, tasks[i].is_kernel ? "kernel" : "module");
    }
}

int task_kill(int pid) {
    task_info_t *t = task_find(pid);
    if (!t) return -1;
    if (!t->killable) return -2;
    if (pid == current_pid) return -3;
    t->used = false;
    return 0;
}

bool task_selftest(char *err, int errlen) {
    int pid = task_register("selftest-task", 1, true);
    if (pid <= 0) {
        ksnprintf(err, errlen, "could not register task");
        return false;
    }
    task_info_t *t = task_find(pid);
    if (!t || strcmp(t->name, "selftest-task") != 0) {
        ksnprintf(err, errlen, "task lookup failed");
        return false;
    }
    int before = task_count();
    if (task_kill(pid) != 0) {
        ksnprintf(err, errlen, "kill failed");
        return false;
    }
    if (task_count() != before - 1) {
        ksnprintf(err, errlen, "task count did not drop");
        return false;
    }
    if (task_total_ticks() == 0 && pit_ticks() == 0) {
        ksnprintf(err, errlen, "scheduler tick counter is zero");
        return false;
    }
    return true;
}
