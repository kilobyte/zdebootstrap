#pragma once
#include <fcntl.h>

#ifdef O_PATH
# define O_PATH_RD O_PATH
#else
# define O_PATH_RD O_RDONLY
#endif

void mk_target(void);
int open_in_target(const char *path, int flags);
