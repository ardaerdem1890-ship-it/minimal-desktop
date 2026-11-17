#pragma once
#include "types.h"

typedef struct {
    char name[24];
    char pass[24];
    u32  uid;          /* 0 = root */
    bool can_sudo;
} user_t;

/* Load/save users from /disk/users.cfg */
void users_init(void);
int  users_save(void);
int  users_count(void);
user_t *users_get(int i);
user_t *users_find(const char *name);
user_t *users_current(void);
bool users_is_root(void);

/* Interactive login; returns chosen user */
user_t *users_login(void);

/* Account management */
int  users_add(const char *name, const char *pass, bool can_sudo);
int  users_del(const char *name);
int  users_passwd(const char *name, const char *newpass);

/* Check whether the current user may run a privileged command */
bool users_can_run(const char *cmd);
void users_require_root(const char *cmd);

/* sudo: prompt for password, then elevate for one command */
bool users_sudo_auth(void);
void users_drop_elevation(void);
