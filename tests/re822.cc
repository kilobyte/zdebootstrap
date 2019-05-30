#include "../822.h"
#include <unistd.h>

int main()
{
    deb822 s;
    s.parse_file(0);
    s.fprint(stdout);

    return 0;
}
