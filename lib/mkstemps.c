// mkstemps implementation based on https://github.com/mirror/mingw-w64/blob/master/mingw-w64-crt/misc/mkstemp.c

#if defined(_WIN32) || (!defined(__GNUC__) && !defined(__clang__))
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

#include <fcntl.h>
#include <share.h>

int __cdecl mkstemps (char *template_name, int suffix_len)
{
    int i, j, fd, len, index;

    /* These are the (62) characters used in temporary filenames. */
    static const char letters[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

    /* The last six characters of template must be "XXXXXX" */
    if (template_name == NULL || (len = strlen (template_name) - suffix_len) < 6
            || memcmp (template_name + (len - 6), "XXXXXX", 6)) {
        errno = EINVAL;
        return -1;
    }

    /* User may supply more than six trailing Xs */
    for (index = len - 6; index > 0 && template_name[index - 1] == 'X'; index--);

    /*
        Like OpenBSD, mkstemp() will try at least 2 ** 31 combinations before
        giving up.
     */
    for (i = 0; i >= 0; i++) {
        for(j = index; j < len; j++) {
            template_name[j] = letters[rand () % 62];
        }
        fd = _sopen(template_name,
                _O_RDWR | _O_CREAT | _O_EXCL | _O_BINARY,
                _SH_DENYRW, _S_IREAD | _S_IWRITE);
        if (fd != -1) return fd;
        if (fd == -1 && errno != EEXIST) return -1;
    }

    return -1;
}
#endif
