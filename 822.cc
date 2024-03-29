#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include "822.h"
#include "zdebootstrap.h"


const std::string& field822(const par822& par, const std::string& key,
    const std::string& none)
{
    const auto f=par.find(key);
    if (f==par.cend())
        return none;
    return f->second;
}

/****************************************************************************/

static bool is_name_char(char c)
{
    return (c>='!' && c<='9') || (c>=';' && c<='~');
}

static void get_entry(par822 &par, const char *&in)
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
    par822 par;

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

static void print_kv(FILE *f, const std::pair<std::string, std::string> &p)
{
    const char *val = p.second.c_str();
    fprintf(f, *val=='\n'? "%s:%s\n":"%s: %s\n", p.first.c_str(), val);
}

static void print_del(FILE *f, par822 &par, const std::string &k)
{
    auto it = par.find(k);
    if (it == par.cend())
        return;

    print_kv(f, *it);
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
        print_del(f, *v, "Package");
        print_del(f, *v, "Essential");
        print_del(f, *v, "Status");
        print_del(f, *v, "Priority");
        print_del(f, *v, "Section");
        print_del(f, *v, "Installed-Size");
        print_del(f, *v, "Origin");
        print_del(f, *v, "Maintainer");
        print_del(f, *v, "Bugs");
        print_del(f, *v, "Architecture");
        print_del(f, *v, "Multi-Arch");
        print_del(f, *v, "Source");
        print_del(f, *v, "Version");
        print_del(f, *v, "Revision");
        print_del(f, *v, "Config-Version");
        print_del(f, *v, "Replaces");
        print_del(f, *v, "Provides");
        print_del(f, *v, "Depends");
        print_del(f, *v, "Pre-Depends");
        print_del(f, *v, "Recommends");
        print_del(f, *v, "Suggests");
        print_del(f, *v, "Breaks");
        print_del(f, *v, "Conflicts");
        print_del(f, *v, "Enhances");
        print_del(f, *v, "Conffiles");
        print_del(f, *v, "Filename");
        print_del(f, *v, "Size");
        print_del(f, *v, "MD5sum");
        print_del(f, *v, "MSDOS-Filename");
        print_del(f, *v, "Description");
        print_del(f, *v, "Triggers-Pending");
        print_del(f, *v, "Triggers-Awaited");

        // Place non-standard ones at the end.
        for (auto p=v->cbegin(); p!=v->cend(); ++p)
            print_kv(f, *p);
    }
}

// fd gets closed when finished
void deb822::parse_file(int fd)
{
    struct stat st;
    char *mem;
    if (!fstat(fd, &st) && S_ISREG(st.st_mode)
        && st.st_size&4095 // even MAP_PRIVATE mappings don't provide pages after
                           // file's length; 4096 is smallest arch page size
        && (mem=(char*)mmap(0, st.st_size+1, PROT_READ|PROT_WRITE, MAP_PRIVATE,
        fd, 0))!=MAP_FAILED)
    {
        mem[st.st_size]=0;
        parse(mem);
        munmap(mem, st.st_size+1);
        close(fd);
        return;
    }

    FILE *f=fdopen(fd, "r");
    if (!f)
        ERR("fdopen failed: %m\n");

    std::string entry;
    char line[4096];
    while (fgets(line, sizeof(line), f))
    {
        if (*line)
            entry+=line;
        else
            parse(entry.c_str()),
            entry.clear();
    }
    parse(entry.c_str());
    fclose(f);
}
