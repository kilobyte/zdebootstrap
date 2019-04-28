#include <map>
#include <string>
#include <vector>

struct deb822
{
    deb822(const char *in);
    void fprint(FILE *f);

    std::vector<std::map<std::string, std::string>> contents;
};
