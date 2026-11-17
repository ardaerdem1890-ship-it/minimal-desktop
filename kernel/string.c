/* Minimal Desktop - freestanding string/memory helpers */
#include "string.h"

void *memcpy(void *dst, const void *src, size_t n) {
    u8 *d = (u8 *)dst;
    const u8 *s = (const u8 *)src;
    while (n--) *d++ = *s++;
    return dst;
}

void *memset(void *dst, int c, size_t n) {
    u8 *d = (u8 *)dst;
    while (n--) *d++ = (u8)c;
    return dst;
}

void *memmove(void *dst, const void *src, size_t n) {
    u8 *d = (u8 *)dst;
    const u8 *s = (const u8 *)src;
    if (d < s) {
        while (n--) *d++ = *s++;
    } else if (d > s) {
        d += n;
        s += n;
        while (n--) *--d = *--s;
    }
    return dst;
}

int memcmp(const void *a, const void *b, size_t n) {
    const u8 *x = (const u8 *)a, *y = (const u8 *)b;
    while (n--) {
        if (*x != *y) return (int)*x - (int)*y;
        x++; y++;
    }
    return 0;
}

size_t strlen(const char *s) {
    size_t n = 0;
    while (s[n]) n++;
    return n;
}

size_t strnlen(const char *s, size_t max) {
    size_t n = 0;
    while (n < max && s[n]) n++;
    return n;
}

int strcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

int strncmp(const char *a, const char *b, size_t n) {
    while (n && *a && *a == *b) { a++; b++; n--; }
    if (!n) return 0;
    return (unsigned char)*a - (unsigned char)*b;
}

int strcasecmp(const char *a, const char *b) {
    while (*a && tolower((unsigned char)*a) == tolower((unsigned char)*b)) { a++; b++; }
    return tolower((unsigned char)*a) - tolower((unsigned char)*b);
}

char *strcpy(char *dst, const char *src) {
    char *d = dst;
    while ((*d++ = *src++)) {}
    return dst;
}

char *strncpy(char *dst, const char *src, size_t n) {
    size_t i = 0;
    for (; i < n && src[i]; i++) dst[i] = src[i];
    for (; i < n; i++) dst[i] = 0;
    return dst;
}

char *strcat(char *dst, const char *src) {
    char *d = dst + strlen(dst);
    while ((*d++ = *src++)) {}
    return dst;
}

char *strchr(const char *s, int c) {
    while (*s) {
        if (*s == (char)c) return (char *)s;
        s++;
    }
    return (c == 0) ? (char *)s : NULL;
}

char *strrchr(const char *s, int c) {
    const char *last = NULL;
    while (*s) {
        if (*s == (char)c) last = s;
        s++;
    }
    if (c == 0) return (char *)s;
    return (char *)last;
}

char *strstr(const char *hay, const char *needle) {
    size_t n = strlen(needle);
    if (!n) return (char *)hay;
    while (*hay) {
        if (!strncmp(hay, needle, n)) return (char *)hay;
        hay++;
    }
    return NULL;
}

char *strtok(char *s, const char *delim) {
    static char *save;
    if (s) save = s;
    if (!save) return NULL;
    while (*save && strchr(delim, *save)) save++;
    if (!*save) { save = NULL; return NULL; }
    char *start = save;
    while (*save && !strchr(delim, *save)) save++;
    if (*save) { *save = 0; save++; } else save = NULL;
    return start;
}

int atoi(const char *s) {
    int sign = 1, v = 0;
    while (isspace((unsigned char)*s)) s++;
    if (*s == '-') { sign = -1; s++; }
    else if (*s == '+') s++;
    while (isdigit((unsigned char)*s)) v = v * 10 + (*s++ - '0');
    return v * sign;
}

u32 strtoul(const char *s, char **end, int base) {
    u32 v = 0;
    bool neg = false;
    while (isspace((unsigned char)*s)) s++;
    if (*s == '-') { neg = true; s++; }
    else if (*s == '+') s++;
    if (base == 0) {
        if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) { base = 16; s += 2; }
        else if (s[0] == '0') { base = 8; s++; }
        else base = 10;
    } else if (base == 16 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        s += 2;
    }
    for (;;) {
        int d;
        unsigned char c = (unsigned char)*s;
        if (c >= '0' && c <= '9') d = c - '0';
        else if (c >= 'a' && c <= 'z') d = c - 'a' + 10;
        else if (c >= 'A' && c <= 'Z') d = c - 'A' + 10;
        else break;
        if (d >= base) break;
        v = v * (u32)base + (u32)d;
        s++;
    }
    if (end) *end = (char *)s;
    return neg ? (u32)(-(s32)v) : v;
}

static const char digits_lower[] = "0123456789abcdef";
static const char digits_upper[] = "0123456789ABCDEF";

static int fmt_utoa(u64 value, char *buf, int base, bool upper) {
    char tmp[65];
    int i = 0, n = 0;
    if (value == 0) { buf[0] = '0'; buf[1] = 0; return 1; }
    const char *digits = upper ? digits_upper : digits_lower;
    while (value) {
        tmp[i++] = digits[value % (u32)base];
        value /= (u32)base;
    }
    while (i) buf[n++] = tmp[--i];
    buf[n] = 0;
    return n;
}

int itoa(int value, char *buf, int base) {
    if (base == 10 && value < 0) {
        buf[0] = '-';
        return 1 + fmt_utoa((u64)(-(s64)value), buf + 1, 10, false);
    }
    return fmt_utoa((u64)(u32)value, buf, base, false);
}

int utoa(u32 value, char *buf, int base) {
    return fmt_utoa(value, buf, base, false);
}

int tolower(int c) { return (c >= 'A' && c <= 'Z') ? c + 32 : c; }
int toupper(int c) { return (c >= 'a' && c <= 'z') ? c - 32 : c; }
bool isdigit(int c) { return c >= '0' && c <= '9'; }
bool isalpha(int c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
bool isspace(int c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v'; }

u32 fnv1a(const void *data, u32 len) {
    const u8 *p = (const u8 *)data;
    u32 h = 2166136261u;
    while (len--) {
        h ^= *p++;
        h *= 16777619u;
    }
    return h;
}

static u32 rng_state = 0xC0FFEE42u;

u32 rand(void) {
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}

void srand(u32 seed) {
    rng_state = seed ? seed : 0xC0FFEE42u;
}
