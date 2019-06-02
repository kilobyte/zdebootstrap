#include <map>
#include <string>
#include <plf_colony.h>

typedef std::map<std::string, std::string> par822;

struct deb822
{
    void parse(const char *in);
    void fprint(FILE *f);
    void parse_file(int fd);

    plf::colony<par822> contents;
};
