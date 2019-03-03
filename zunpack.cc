#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "deb.h"
#include "zdebootstrap.h"

#define ERR(...) do {fprintf(stderr, __VA_ARGS__); exit(1);} while (0)

const char *target;
int orig_wd;


int main()
{
    if ((orig_wd = open(".", O_RDONLY|O_DIRECTORY|O_PATH|O_CLOEXEC)) == -1)
        ERR("can't open current working directory\n");

    target = "target";
    mkdir(target, 0777); // TODO: mkdir -p
    if (chdir(target))
        ERR("can't chdir to '%s': %m\n", target);

    deb ar("dash.deb");
    ar.unpack();
    return 0;
}
