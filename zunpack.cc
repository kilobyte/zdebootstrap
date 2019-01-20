#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "deb_ar.h"
#include "deb_comp.h"
#include "zdebootstrap.h"

#define ERR(...) do {fprintf(stderr, __VA_ARGS__); exit(1);} while (0)

int main()
{
    target = "target";
    mkdir(target, 0777); // TODO: mkdir -p

    deb_ar ar("dash.deb");
    ar.check_deb_binary();
    ar.read_control();
    return 0;
}
