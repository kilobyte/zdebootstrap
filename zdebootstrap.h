#pragma once

extern const char *target;
extern int orig_wd;

#include <stdio.h>
#define ERR(...) do {fprintf(stderr, __VA_ARGS__); exit(1);} while (0)
