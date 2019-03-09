#include <unistd.h>
#include <string.h>

#include "822.h"
#include "zdebootstrap.h"

static bool is_name_char(char c)
{
    return (c>='!' && c<='9') || (c>=';' && c<='~');
}

static void get_entry(std::unordered_map<std::string, std::string> &par, const char *&in)
{
    const char *nameB = in;
    while (is_name_char(*in))
        in++;

    if (nameB==in || *in!=':')
        ERR("not a valid name:value in deb822: '%.64s'\n", nameB);

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

deb822::deb822(const char *in)
{
    std::unordered_map<std::string, std::string> par;

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
                contents.push_back(par);
            par.clear();
        }
        else
            get_entry(par, in);
    }

    if (!par.empty())
        contents.push_back(par);
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

        for (auto p=v->cbegin(); p!=v->cend(); ++p)
            printf("%s: %s\n", p->first.c_str(), p->second.c_str());
    }    
}
