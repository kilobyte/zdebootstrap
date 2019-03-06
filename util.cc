#include <sys/types.h>
#include <sys/stat.h>
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
