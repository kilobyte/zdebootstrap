#include <map>
#include <string>
#include <plf_colony.h>

struct deb822
{
    void parse(const char *in);
    void fprint(FILE *f);

    plf::colony<std::map<std::string, std::string>> contents;
};
