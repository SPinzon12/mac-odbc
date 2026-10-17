/* Interpose dlsym so we can see exactly which symbols iODBC asks the driver
 * for, and which ones come back NULL. */
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>

static void *(*real_dlsym)(void *, const char *);
static FILE *logf;

__attribute__((constructor))
static void init(void)
{
    real_dlsym = dlsym(RTLD_NEXT, "dlsym");
    const char *p = getenv("DLSYM_LOG");
    logf = p ? fopen(p, "a") : stderr;
}

void *dlsym(void *handle, const char *symbol)
{
    void *r = real_dlsym(handle, symbol);
    if (logf && symbol && symbol[0] == 'S' && symbol[1] == 'Q' && symbol[2] == 'L')
        fprintf(logf, "dlsym(%p, %-28s) = %p%s\n", handle, symbol, r, r ? "" : "   <-- NULL");
    if (logf) fflush(logf);
    return r;
}
