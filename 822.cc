#include <unistd.h>
#include <string.h>

#include "822.h"
#include "zdebootstrap.h"

static bool is_name_char(char c)
{
    return (c>='!' && c<='9') || (c>=';' && c<='~');
}

static void get_entry(std::map<std::string, std::string> &par, const char *&in)
{
    const char *nameB = in;
    while (is_name_char(*in))
        in++;

    if (nameB==in || *in!=':')
    {
        ERR("not a valid name:value in deb822: '%.*s'\n",
            (int)(strchrnul(nameB, '\n')-nameB), nameB);
    }

    const char *nameE = in++;
    in+=strspn(in, " \t");
    const char *valB = in;

    while (*in)
    {
        if (*in++!='\n')
            continue;

        while (*in=='#' && *(in = strchrnul(in, '\n')))
            in++;
        if (*in==' ' || *in=='\t')
            in++;
        else
            break;
    }

    const char *valE = in;
    if (valB<valE && valE[-1]=='\n')
        valE--;

    std::string name(nameB, nameE-nameB);
    if (par.count(name))
        ERR("duplicate name in deb822: '%.*s'\n", (int)(nameE-nameB), nameB);
    par.emplace(name, std::string(valB, (int)(valE-valB)));
}

void deb822::parse(const char *in)
{
    std::map<std::string, std::string> par;

    while (1)
    {
        while (*in=='#' && *(in = strchrnul(in, '\n')))
            in++;
        if (!*in)
            break;

        if (*in=='\n')
        {
            in++;
            if (!par.empty())
                contents.insert(par);
            par.clear();
        }
        else
            get_entry(par, in);
    }

    if (!par.empty())
        contents.insert(par);
}

static void print_del(std::map<std::string, std::string> &par, const std::string &k)
{
    auto it = par.find(k);
    if (it == par.cend())
        return;

    printf("%s: %s\n", it->first.c_str(), it->second.c_str());
    par.erase(it);
}

void deb822::fprint(FILE *f)
{
    bool cont = false;

    for (auto v=contents.begin(); v!=contents.end(); ++v)
    {
        if (cont)
            putc('\n', f);
        else
            cont = true;

        // Print common elements in customary order.
        print_del(*v, "Package");
        print_del(*v, "Essential");
        print_del(*v, "Status");
        print_del(*v, "Priority");
        print_del(*v, "Section");
        print_del(*v, "Installed-Size");
        print_del(*v, "Origin");
        print_del(*v, "Maintainer");
        print_del(*v, "Bugs");
        print_del(*v, "Architecture");
        print_del(*v, "Multi-Arch");
        print_del(*v, "Source");
        print_del(*v, "Version");
        print_del(*v, "Revision");
        print_del(*v, "Config-Version");
        print_del(*v, "Replaces");
        print_del(*v, "Provides");
        print_del(*v, "Depends");
        print_del(*v, "Pre-Depends");
        print_del(*v, "Recommends");
        print_del(*v, "Suggests");
        print_del(*v, "Breaks");
        print_del(*v, "Conflicts");
        print_del(*v, "Enhances");
        print_del(*v, "Conffiles");
        print_del(*v, "Filename");
        print_del(*v, "Size");
        print_del(*v, "MD5sum");
        print_del(*v, "MSDOS-Filename");
        print_del(*v, "Description");
        print_del(*v, "Triggers-Pending");
        print_del(*v, "Triggers-Awaited");

        // Place non-standard ones at the end.
        for (auto p=v->cbegin(); p!=v->cend(); ++p)
            printf("%s: %s\n", p->first.c_str(), p->second.c_str());
    }
}
