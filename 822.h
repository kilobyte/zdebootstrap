#include <map>
#include <string>
#include <vector>

struct deb822
{
    void parse(const char *in);
    void fprint(FILE *f);

    std::vector<std::map<std::string, std::string>> contents;
};
