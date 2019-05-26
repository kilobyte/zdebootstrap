#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <string>

#include "zdebootstrap.h"
#include "util.h"

int mkdir_p(const char *path)
{
    std::string p = path;

    for (size_t i=1; i < p.size(); i++)
        if (p[i] == '/')
            if (mkdir(p.substr(0,i).c_str(), 0755) && errno != EEXIST)
                return -1;

    if (mkdir(path, 0755) && errno != EEXIST)
        return -1;

    return 0;
}

char *strdupe(const std::string &s)
{
    size_t len = s.length();
    char *t = (char*)malloc(len+1);
    if (!t)
        ERR("Out of memory.\n");
    memcpy(t, s.c_str(), len);
    t[len]=0;
    return t;
}
