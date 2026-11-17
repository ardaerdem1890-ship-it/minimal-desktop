#pragma once
#include "types.h"

void *memcpy(void *dst, const void *src, size_t n);
void *memset(void *dst, int c, size_t n);
void *memmove(void *dst, const void *src, size_t n);
int   memcmp(const void *a, const void *b, size_t n);

size_t strlen(const char *s);
size_t strnlen(const char *s, size_t max);
int    strcmp(const char *a, const char *b);
int    strncmp(const char *a, const char *b, size_t n);
int    strcasecmp(const char *a, const char *b);
char  *strcpy(char *dst, const char *src);
char  *strncpy(char *dst, const char *src, size_t n);
char  *strcat(char *dst, const char *src);
char  *strchr(const char *s, int c);
char  *strrchr(const char *s, int c);
char  *strstr(const char *hay, const char *needle);
char  *strtok(char *s, const char *delim);

int    atoi(const char *s);
u32    strtoul(const char *s, char **end, int base);
int    itoa(int value, char *buf, int base);
int    utoa(u32 value, char *buf, int base);

int    tolower(int c);
int    toupper(int c);
bool   isdigit(int c);
bool   isalpha(int c);
bool   isspace(int c);

u32    fnv1a(const void *data, u32 len);
u32    rand(void);
void   srand(u32 seed);
