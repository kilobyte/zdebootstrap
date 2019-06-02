#include "deb.h"
#include "zdebootstrap.h"

const char *target;
int orig_wd;
int verbose;
plf::colony<const char*> path_excludes;
