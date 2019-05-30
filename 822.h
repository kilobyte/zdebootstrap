#include <map>
#include <string>
#include <plf_colony.h>

struct deb822
{
    void parse(const char *in);
    void fprint(FILE *f);
    void parse_file(int fd);

    plf::colony<std::map<std::string, std::string>> contents;
};
