#include "../822.h"
#include <unistd.h>

int main()
{
    std::string txt;
    char buf[4096];
    ssize_t r;

    while ((r = read(0, buf, sizeof(buf))) > 0)
        txt.append(buf, r);
    if (r<0)
        return perror("re822: read"), 1;

    deb822 s;
    s.parse(txt.c_str());
    s.fprint(stdout);

    return 0;
}
