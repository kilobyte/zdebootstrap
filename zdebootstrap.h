#pragma once
#include <plf_colony.h>

extern const char *target;
extern int orig_wd;
extern int verbose;
extern plf::colony<const char*> path_excludes;

#include <stdio.h>
#define ERR(...) do {fprintf(stderr, __VA_ARGS__); exit(1);} while (0)
