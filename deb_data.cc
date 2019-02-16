#include <archive.h>
#include <archive_entry.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <string.h>

#include "deb_data.h"

#include <stdio.h>
#define ERR(...) do {fprintf(stderr, __VA_ARGS__); exit(1);} while (0)


deb_data::deb_data(deb_ar *parent_ar)
{
    ar = parent_ar;

    if (!(arc = archive_read_new()))
        ERR("can't create a new libarchive object: %s\n", archive_error_string(arc));

    archive_read_support_filter_gzip(arc);
    archive_read_support_filter_bzip2(arc);
    archive_read_support_filter_lzma(arc);
    archive_read_support_filter_xz(arc);
    archive_read_support_format_tar(arc);

    // TODO: free earlier allocs on error
    if (!(aw = archive_write_disk_new()))
        ERR("can't create a new libarchive write object\n");

    archive_write_disk_set_options(aw, (geteuid()? 0 : ARCHIVE_EXTRACT_OWNER)
        |ARCHIVE_EXTRACT_PERM
        |ARCHIVE_EXTRACT_TIME
        |ARCHIVE_EXTRACT_UNLINK
        |ARCHIVE_EXTRACT_SECURE_NODOTDOT
        |ARCHIVE_EXTRACT_SECURE_NOABSOLUTEPATHS
        |ARCHIVE_EXTRACT_CLEAR_NOCHANGE_FFLAGS
        |ARCHIVE_EXTRACT_SECURE_SYMLINKS);

    if (archive_read_open(arc, ar, 0, deb_ar_comp_read, 0))
    {
        ERR("can't open deb data: '%s': %s\n", ar->filename,
            archive_error_string(arc));
    }

    struct archive_entry *ent;
    int err;
    while (!(err = archive_read_next_header(arc, &ent)))
    {
        printf("%s\n", archive_entry_pathname(ent));
        if ((err = archive_read_extract2(arc, ent, aw)))
        {
            ERR("error extracting deb entry '%s' from '%s': %s\n",
                archive_entry_pathname(ent), ar->filename, archive_error_string(arc));
        }
    }

    if (err != ARCHIVE_EOF)
    {
        ERR("can't list deb data: '%s': %s\n", ar->filename,
            archive_error_string(arc));
    }
}

deb_data::~deb_data()
{
    archive_read_free(arc);
    archive_write_free(aw);
}
