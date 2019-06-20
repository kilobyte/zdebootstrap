#include "deb.h"
#include "zdebootstrap.h"

const char *target;
int orig_wd, target_wd;
char *abs_target;
int verbose;
plf::colony<const char*> path_excludes;
