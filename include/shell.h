#pragma once
#include "types.h"

typedef int (*cmd_fn_t)(int argc, char **argv);

typedef struct {
    const char *name;
    const char *category;
    const char *usage;
    const char *desc;
    cmd_fn_t    fn;
} command_t;

void shell_init(void);
void shell_run(void);
int  shell_execute(const char *line);
int  shell_execute_silent(const char *line);
const command_t *shell_find_command(const char *name);
const command_t *shell_command_table(int *count);
int  shell_tokenize(char *line, char **argv, int max);
const char *shell_cwd_path(void);
void shell_banner(void);
bool shell_selftest(char *err, int errlen);

/* command implementations */
int cmd_help(int, char **);
int cmd_sysinfo(int, char **);
int cmd_uptime(int, char **);
int cmd_drivers(int, char **);
int cmd_hwinfo(int, char **);
int cmd_taskmng(int, char **);
int cmd_selftest(int, char **);
int cmd_clear(int, char **);
int cmd_reboot(int, char **);
int cmd_shutdown(int, char **);
int cmd_date(int, char **);
int cmd_time(int, char **);
int cmd_ls(int, char **);
int cmd_cd(int, char **);
int cmd_pwd(int, char **);
int cmd_mkdir(int, char **);
int cmd_touch(int, char **);
int cmd_rm(int, char **);
int cmd_cat(int, char **);
int cmd_write(int, char **);
int cmd_echo(int, char **);
int cmd_memmap(int, char **);
int cmd_hexdump(int, char **);
int cmd_edit(int, char **);
int cmd_crash(int, char **);
int cmd_install(int, char **);
int cmd_read(int, char **);
int cmd_df(int, char **);
int cmd_calc(int, char **);
int cmd_theme(int, char **);
int cmd_matrix(int, char **);
int cmd_beep(int, char **);
int cmd_credits(int, char **);
int cmd_snake(int, char **);
int cmd_pong(int, char **);
int cmd_doom(int, char **);
int cmd_startgui(int, char **);
int cmd_miniland(int, char **);
int cmd_keymap(int, char **);
int cmd_minipacket(int, char **);
int cmd_ifconfig(int, char **);
int cmd_ping(int, char **);
int cmd_nslookup(int, char **);
int cmd_curl(int, char **);
int cmd_netstat(int, char **);
int cmd_uname(int, char **);
