#include "../tqueue.h"
#include <stdio.h>

#define ERR(...) do return fprintf(stderr, __VA_ARGS__), 1; while (0)

static void worker(const char *job)
{
    printf("%s\n", job);
}

int main()
{
    tqueue q(worker, 1);

    char line[256];
    while (fgets(line, sizeof(line), stdin))
    {
        *strchrnul(line, '\n')=0;
        if (!line[0] || line[1]!=' ')
            ERR("Bad syntax in “%s”\n", line);

        switch (line[0])
        {
        case 'P':
            q.put(strdup(line+2));
            break;
        case 'R':
            {
                const char *sp = strchr(line+2, ' ');
                if (!sp || sp==line+2 || !sp[1])
                    ERR("Wanted two args in “%s”\n", line);
                q.req(std::string(line+2, sp-line-2), sp+1);
            }
            break;
        default:
            ERR("Bad command in “%s”\n", line);
        }
    }

    return 0;
}
