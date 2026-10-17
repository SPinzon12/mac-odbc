/* ODBC 2.x adapter + call tracer for psqlodbc under iODBC.
 *
 * - Trampolines (trampolines.s, generated) re-export the driver's own surface.
 * - The ODBC 2.x entry points are implemented on top of the driver.
 * - The entry points iODBC touches on the way to a connection are wrapped in
 *   C with tracing, so we can see exactly where the driver manager stops. */
#include <stdio.h>
#include <stdint.h>
#include <wchar.h>
#include <dlfcn.h>
#include <sql.h>
#include <sqlext.h>
#include "shim_ptrs.h"

#define REAL_DRIVER "/Library/ODBC/psqlodbc.bundle/Contents/MacOS/psqlodbcw.so"
#define TRACE_FILE  "/tmp/odbc_trace.log"

#define TRACE(...) do { FILE *_f = fopen(TRACE_FILE, "a"); \
    if (_f) { fprintf(_f, __VA_ARGS__); fclose(_f); } } while (0)

__attribute__((constructor))
static void load_driver(void)
{
    void *drv = dlopen(REAL_DRIVER, RTLD_NOW | RTLD_LOCAL);
    TRACE("%s shim load: driver=%s %s\n", __func__, REAL_DRIVER, drv ? "ok" : "FAILED");
    if (!drv) {
        fprintf(stderr, "odbc2full: cannot load %s: %s\n", REAL_DRIVER, dlerror());
        return;
    }
    shim_init(drv);
    TRACE("%s shim_init done: fp_SQLDriverConnect=%p fp_SQLAllocHandle=%p\n",
          __func__, (void *)fp_SQLDriverConnect, (void *)fp_SQLAllocHandle);
}

/* ---- typed views over the trampoline pointer table ---- */
typedef SQLRETURN (*fn3_alloc)(SQLSMALLINT, SQLHANDLE, SQLHANDLE *);
typedef SQLRETURN (*fn3_free)(SQLSMALLINT, SQLHANDLE);
typedef SQLRETURN (*fn3_setenvattr)(SQLHENV, SQLINTEGER, SQLPOINTER, SQLINTEGER);
typedef SQLRETURN (*fn3_getenvattr)(SQLHENV, SQLINTEGER, SQLPOINTER, SQLINTEGER, SQLINTEGER *);
typedef SQLRETURN (*fn3_drvconn)(SQLHDBC, SQLHWND, SQLCHAR *, SQLSMALLINT, SQLCHAR *, SQLSMALLINT, SQLSMALLINT *, SQLSMALLINT);
typedef SQLRETURN (*fn3_drvconnW)(SQLHDBC, SQLHWND, SQLWCHAR *, SQLSMALLINT, SQLWCHAR *, SQLSMALLINT, SQLSMALLINT *, SQLSMALLINT);
typedef SQLRETURN (*fn3_connect)(SQLHDBC, SQLCHAR *, SQLSMALLINT, SQLCHAR *, SQLSMALLINT, SQLCHAR *, SQLSMALLINT);
typedef SQLRETURN (*fn3_connectW)(SQLHDBC, SQLWCHAR *, SQLSMALLINT, SQLWCHAR *, SQLSMALLINT, SQLWCHAR *, SQLSMALLINT);
typedef SQLRETURN (*fn3_setcattr)(SQLHDBC, SQLINTEGER, SQLPOINTER, SQLINTEGER);
typedef SQLRETURN (*fn3_getcattr)(SQLHDBC, SQLINTEGER, SQLPOINTER, SQLINTEGER, SQLINTEGER *);
typedef SQLRETURN (*fn3_getinfo)(SQLHDBC, SQLUSMALLINT, SQLPOINTER, SQLSMALLINT, SQLSMALLINT *);
typedef SQLRETURN (*fn3_getinfoW)(SQLHDBC, SQLUSMALLINT, SQLPOINTER, SQLSMALLINT, SQLSMALLINT *);
typedef SQLRETURN (*fn3_diag)(SQLSMALLINT, SQLHANDLE, SQLSMALLINT, SQLCHAR *, SQLINTEGER *, SQLCHAR *, SQLSMALLINT, SQLSMALLINT *);
typedef SQLRETURN (*fn3_endtran)(SQLSMALLINT, SQLHANDLE, SQLSMALLINT);
typedef SQLRETURN (*fn3_colattr)(SQLHSTMT, SQLUSMALLINT, SQLUSMALLINT, SQLPOINTER, SQLSMALLINT, SQLSMALLINT *, SQLLEN *);
typedef SQLRETURN (*fn3_bulk)(SQLHSTMT, SQLSMALLINT);

/* ---------- traced ODBC 3 entry points ---------- */

SQLRETURN SQLAllocHandle(SQLSMALLINT t, SQLHANDLE in, SQLHANDLE *out)
{
    TRACE("→ SQLAllocHandle(type=%d)\n", t);
    SQLRETURN r = ((fn3_alloc)fp_SQLAllocHandle)(t, in, out);
    TRACE("← SQLAllocHandle(type=%d) = %d *out=%p\n", t, r, out ? (void *)*out : NULL);
    return r;
}

SQLRETURN SQLFreeHandle(SQLSMALLINT t, SQLHANDLE h)
{
    TRACE("→ SQLFreeHandle(type=%d h=%p)\n", t, (void *)h);
    return ((fn3_free)fp_SQLFreeHandle)(t, h);
}

SQLRETURN SQLSetEnvAttr(SQLHENV e, SQLINTEGER a, SQLPOINTER v, SQLINTEGER l)
{
    TRACE("→ SQLSetEnvAttr(attr=%d val=%p)\n", (int)a, v);
    return ((fn3_setenvattr)fp_SQLSetEnvAttr)(e, a, v, (SQLINTEGER)l);
}

SQLRETURN SQLGetEnvAttr(SQLHENV e, SQLINTEGER a, SQLPOINTER v, SQLINTEGER l, SQLINTEGER *s)
{
    TRACE("→ SQLGetEnvAttr(attr=%d)\n", (int)a);
    return ((fn3_getenvattr)fp_SQLGetEnvAttr)(e, a, v, l, s);
}

SQLRETURN SQLSetConnectAttr(SQLHDBC d, SQLINTEGER a, SQLPOINTER v, SQLINTEGER l)
{
    TRACE("→ SQLSetConnectAttr(attr=%d val=%p)\n", (int)a, v);
    return ((fn3_setcattr)fp_SQLSetConnectAttr)(d, a, v, l);
}

SQLRETURN SQLSetConnectAttrW(SQLHDBC d, SQLINTEGER a, SQLPOINTER v, SQLINTEGER l)
{
    TRACE("→ SQLSetConnectAttrW(attr=%d val=%p)\n", (int)a, v);
    return ((fn3_setcattr)fp_SQLSetConnectAttrW)(d, a, v, l);
}

SQLRETURN SQLGetConnectAttr(SQLHDBC d, SQLINTEGER a, SQLPOINTER v, SQLINTEGER l, SQLINTEGER *s)
{
    TRACE("→ SQLGetConnectAttr(attr=%d)\n", (int)a);
    return ((fn3_getcattr)fp_SQLGetConnectAttr)(d, a, v, l, s);
}

SQLRETURN SQLGetConnectAttrW(SQLHDBC d, SQLINTEGER a, SQLPOINTER v, SQLINTEGER l, SQLINTEGER *s)
{
    TRACE("→ SQLGetConnectAttrW(attr=%d)\n", (int)a);
    return ((fn3_getcattr)fp_SQLGetConnectAttrW)(d, a, v, l, s);
}

SQLRETURN SQLDriverConnect(SQLHDBC d, SQLHWND w, SQLCHAR *cs, SQLSMALLINT cl,
                           SQLCHAR *o, SQLSMALLINT ob, SQLSMALLINT *ol, SQLUSMALLINT dc)
{
    TRACE("→ SQLDriverConnect(ANSI) cs='%s' len=%d\n", cl >= 0 ? (char *)cs : (char *)cs, cl);
    SQLRETURN r = ((fn3_drvconn)fp_SQLDriverConnect)(d, w, cs, cl, o, ob, ol, dc);
    TRACE("← SQLDriverConnect = %d\n", r);
    return r;
}

/* ---- iODBC W-ABI bridge ----------------------------------------------
 * iODBC on macOS was built with 4-byte SQLWCHAR (wchar_t / UTF-32). psqlodbc
 * builds against unixODBC where SQLWCHAR is 2-byte UTF-16. Every W call from
 * iODBC therefore arrives in UTF-32; the driver reads it as UTF-16 garbage.
 * Bridge by converting to UTF-8 and using the driver's ANSI entry points. */

static size_t u32len(const uint32_t *s) { size_t n = 0; while (s && s[n]) n++; return n; }

static int u32_to_utf8(const uint32_t *s, size_t n32, char *out, size_t cap)
{
    size_t o = 0;
    for (size_t i = 0; i < n32 && o + 4 < cap; i++) {
        uint32_t c = s[i];
        if (c < 0x80)        out[o++] = (char)c;
        else if (c < 0x800) { out[o++] = (char)(0xC0 | (c >> 6)); out[o++] = (char)(0x80 | (c & 0x3F)); }
        else if (c < 0x10000) { out[o++] = (char)(0xE0 | (c >> 12)); out[o++] = (char)(0x80 | ((c >> 6) & 0x3F)); out[o++] = (char)(0x80 | (c & 0x3F)); }
        else { out[o++] = (char)(0xF0 | (c >> 18)); out[o++] = (char)(0x80 | ((c >> 12) & 0x3F)); out[o++] = (char)(0x80 | ((c >> 6) & 0x3F)); out[o++] = (char)(0x80 | (c & 0x3F)); }
    }
    out[o] = 0;
    return (int)o;
}

static int utf8_to_u32(const char *s, uint32_t *out, size_t cap32)
{
    size_t o = 0, i = 0;
    while (s[i] && o + 1 < cap32) {
        unsigned char b = (unsigned char)s[i];
        uint32_t c;
        if (b < 0x80)               { c = b; i += 1; }
        else if ((b & 0xE0) == 0xC0){ c = ((uint32_t)(b & 0x1F) << 6) | (s[i+1] & 0x3F); i += 2; }
        else if ((b & 0xF0) == 0xE0){ c = ((uint32_t)(b & 0x0F) << 12) | ((s[i+1] & 0x3F) << 6) | (s[i+2] & 0x3F); i += 3; }
        else                        { c = ((uint32_t)(b & 0x07) << 18) | ((s[i+1] & 0x3F) << 12) | ((s[i+2] & 0x3F) << 6) | (s[i+3] & 0x3F); i += 4; }
        out[o++] = c;
    }
    out[o] = 0;
    return (int)o;
}

SQLRETURN SQLDriverConnectW(SQLHDBC d, SQLHWND w, SQLWCHAR *cs, SQLSMALLINT cl,
                            SQLWCHAR *o, SQLSMALLINT ob, SQLSMALLINT *ol, SQLUSMALLINT dc)
{
    char in8[4096];
    size_t n = cl >= 0 ? (size_t)cl : u32len((uint32_t *)cs);
    u32_to_utf8((uint32_t *)cs, n, in8, sizeof in8);
    TRACE("→ SQLDriverConnectW(bridge) cs='%s'\n", in8);
    char out8[1024] = {0};
    SQLSMALLINT out8len = 0;
    SQLRETURN r = ((fn3_drvconn)fp_SQLDriverConnect)(d, w, (SQLCHAR *)in8, SQL_NTS,
                                                     (SQLCHAR *)out8, (SQLSMALLINT)sizeof out8,
                                                     &out8len, dc);
    TRACE("← SQLDriverConnectW(bridge) = %d\n", r);
    if (o && ob > 0) {
        int u = utf8_to_u32(out8, (uint32_t *)o, (size_t)ob);
        if (ol) *ol = (SQLSMALLINT)u;
    } else if (ol) *ol = 0;
    return r;
}

SQLRETURN SQLConnectW(SQLHDBC d, SQLWCHAR *dsn, SQLSMALLINT l1,
                      SQLWCHAR *uid, SQLSMALLINT l2, SQLWCHAR *pwd, SQLSMALLINT l3)
{
    char a[256], b[256], c[256];
    u32_to_utf8((uint32_t *)dsn, l1 >= 0 ? l1 : (SQLSMALLINT)u32len((uint32_t *)dsn), a, sizeof a);
    u32_to_utf8((uint32_t *)uid, l2 >= 0 ? l2 : (SQLSMALLINT)u32len((uint32_t *)uid), b, sizeof b);
    u32_to_utf8((uint32_t *)pwd, l3 >= 0 ? l3 : (SQLSMALLINT)u32len((uint32_t *)pwd), c, sizeof c);
    TRACE("→ SQLConnectW(bridge) dsn='%s' uid='%s'\n", a, b);
    return ((fn3_connect)fp_SQLConnect)(d, (SQLCHAR *)a, SQL_NTS, (SQLCHAR *)b, SQL_NTS,
                                        (SQLCHAR *)c, SQL_NTS);
}

SQLRETURN SQLGetDiagRecW(SQLSMALLINT ht, SQLHANDLE h, SQLSMALLINT rec, SQLWCHAR *state,
                         SQLINTEGER *nat, SQLWCHAR *msg, SQLSMALLINT bl, SQLSMALLINT *tl)
{
    char st8[8] = {0}, msg8[1024] = {0};
    SQLINTEGER n8 = 0;
    SQLSMALLINT l8 = 0;
    SQLRETURN r = ((fn3_diag)fp_SQLGetDiagRec)(ht, h, rec, (SQLCHAR *)st8, &n8,
                                               (SQLCHAR *)msg8, (SQLSMALLINT)sizeof msg8, &l8);
    if (r == SQL_SUCCESS || r == SQL_SUCCESS_WITH_INFO) {
        uint32_t *st32 = (uint32_t *)state;
        for (int i = 0; i < 6 && st8[i]; i++) st32[i] = (uint8_t)st8[i];
        st32[5] = 0; /* keep it NUL-safe even for a full 5-char state */
        if (msg && bl > 0) {
            int u = utf8_to_u32(msg8, (uint32_t *)msg, (size_t)bl);
            if (tl) *tl = (SQLSMALLINT)u;
        }
        if (nat) *nat = n8;
        TRACE("← SQLGetDiagRecW(bridge) state='%s' msg='%.120s'\n", st8, msg8);
    }
    return r;
}

SQLRETURN SQLConnect(SQLHDBC d, SQLCHAR *sn, SQLSMALLINT sl, SQLCHAR *u, SQLSMALLINT ul,
                     SQLCHAR *p, SQLSMALLINT pl)
{
    TRACE("→ SQLConnect(ANSI)\n");
    return ((fn3_connect)fp_SQLConnect)(d, sn, sl, u, ul, p, pl);
}

SQLRETURN SQLGetInfo(SQLHDBC d, SQLUSMALLINT t, SQLPOINTER v, SQLSMALLINT l, SQLSMALLINT *s)
{
    TRACE("→ SQLGetInfo(ANSI, type=%d)\n", t);
    return ((fn3_getinfo)fp_SQLGetInfo)(d, t, v, l, s);
}

SQLRETURN SQLGetInfoW(SQLHDBC d, SQLUSMALLINT t, SQLPOINTER v, SQLSMALLINT l, SQLSMALLINT *s)
{
    TRACE("→ SQLGetInfoW(type=%d)\n", t);
    return ((fn3_getinfoW)fp_SQLGetInfoW)(d, t, v, l, s);
}

SQLRETURN SQLGetDiagRec(SQLSMALLINT ht, SQLHANDLE h, SQLSMALLINT rec, SQLCHAR *st,
                        SQLINTEGER *nat, SQLCHAR *msg, SQLSMALLINT bl, SQLSMALLINT *tl)
{
    TRACE("→ SQLGetDiagRec(ANSI, type=%d rec=%d)\n", ht, rec);
    return ((fn3_diag)fp_SQLGetDiagRec)(ht, h, rec, st, nat, msg, bl, tl);
}

/* ---------- ODBC 2.x entry points ---------- */

SQLRETURN SQLAllocEnv(SQLHENV *ph)          { return SQLAllocHandle(SQL_HANDLE_ENV,  SQL_NULL_HENV,  (SQLHANDLE *)ph); }
SQLRETURN SQLAllocConnect(SQLHENV e, SQLHDBC *ph) { return SQLAllocHandle(SQL_HANDLE_DBC, e, (SQLHANDLE *)ph); }
SQLRETURN SQLAllocStmt(SQLHDBC d, SQLHSTMT *ph)   { return SQLAllocHandle(SQL_HANDLE_STMT, d, (SQLHANDLE *)ph); }
SQLRETURN SQLFreeEnv(SQLHENV e)              { return ((fn3_free)fp_SQLFreeHandle)(SQL_HANDLE_ENV,  e); }
SQLRETURN SQLFreeConnect(SQLHDBC d)          { return ((fn3_free)fp_SQLFreeHandle)(SQL_HANDLE_DBC,  d); }

SQLRETURN SQLTransact(SQLHENV e, SQLHDBC d, SQLUSMALLINT f)
{
    if (d) return ((fn3_endtran)fp_SQLEndTran)(SQL_HANDLE_DBC, d, f);
    return ((fn3_endtran)fp_SQLEndTran)(SQL_HANDLE_ENV, e, f);
}

SQLRETURN SQLError(SQLHENV e, SQLHDBC d, SQLHSTMT s,
                   SQLCHAR *state, SQLINTEGER *native, SQLCHAR *msg,
                   SQLSMALLINT buflen, SQLSMALLINT *len)
{
    if (s) return ((fn3_diag)fp_SQLGetDiagRec)(SQL_HANDLE_STMT, s, 1, state, native, msg, buflen, len);
    if (d) return ((fn3_diag)fp_SQLGetDiagRec)(SQL_HANDLE_DBC,  d, 1, state, native, msg, buflen, len);
    return ((fn3_diag)fp_SQLGetDiagRec)(SQL_HANDLE_ENV,  e, 1, state, native, msg, buflen, len);
}

static SQLINTEGER opt_to_attr(SQLUSMALLINT o)
{
    switch (o) {
    case SQL_ACCESS_MODE:       return SQL_ATTR_ACCESS_MODE;
    case SQL_AUTOCOMMIT:        return SQL_ATTR_AUTOCOMMIT;
    case SQL_LOGIN_TIMEOUT:     return SQL_ATTR_LOGIN_TIMEOUT;
    case SQL_ODBC_CURSORS:      return SQL_ATTR_ODBC_CURSORS;
    case SQL_PACKET_SIZE:       return SQL_ATTR_PACKET_SIZE;
    case SQL_QUIET_MODE:        return SQL_ATTR_QUIET_MODE;
    case SQL_TRANSLATE_DLL:     return SQL_ATTR_TRANSLATE_LIB;
    case SQL_TRANSLATE_OPTION:  return SQL_ATTR_TRANSLATE_OPTION;
    case SQL_TXN_ISOLATION:     return SQL_ATTR_TXN_ISOLATION;
    case SQL_CURRENT_QUALIFIER: return SQL_ATTR_CURRENT_CATALOG;
    case SQL_MAX_LENGTH:        return SQL_ATTR_MAX_LENGTH;
    case SQL_QUERY_TIMEOUT:     return SQL_ATTR_QUERY_TIMEOUT;
    case SQL_USE_BOOKMARKS:     return SQL_ATTR_USE_BOOKMARKS;
    default: return (SQLINTEGER)o;
    }
}

SQLRETURN SQLGetConnectOption(SQLHDBC d, SQLUSMALLINT o, PTR p)
{ return ((fn3_getcattr)fp_SQLGetConnectAttr)(d, opt_to_attr(o), p, 0, NULL); }

SQLRETURN SQLSetConnectOption(SQLHDBC d, SQLUSMALLINT o, SQLULEN v)
{ return ((fn3_setcattr)fp_SQLSetConnectAttr)(d, opt_to_attr(o), (PTR)(uintptr_t)v, SQL_IS_UINTEGER); }

/* ponytail: string-valued connect options stay ANSI here; none are on the
 * PostgreSQL connect path. */
SQLRETURN SQLSetConnectOptionW(SQLHDBC d, SQLUSMALLINT o, SQLULEN v)
{ return ((fn3_setcattr)fp_SQLSetConnectAttrW)(d, opt_to_attr(o), (PTR)(uintptr_t)v, SQL_IS_UINTEGER); }

SQLRETURN SQLGetStmtOption(SQLHSTMT s, SQLUSMALLINT o, PTR p)
{ return ((fn3_getcattr)fp_SQLGetStmtAttr)(s, opt_to_attr(o), p, 0, NULL); }

SQLRETURN SQLSetStmtOption(SQLHSTMT s, SQLUSMALLINT o, SQLULEN v)
{ return ((fn3_setcattr)fp_SQLSetStmtAttr)(s, opt_to_attr(o), (PTR)(uintptr_t)v, SQL_IS_UINTEGER); }

SQLRETURN SQLParamOptions(SQLHSTMT s, SQLULEN crow, SQLULEN *pirow)
{
    (void)crow;
    if (pirow) *pirow = 0;
    return ((fn3_bulk)fp_SQLBulkOperations)(s, SQL_ADD);
}

SQLRETURN SQLColAttributes(SQLHSTMT s, SQLUSMALLINT col, SQLUSMALLINT fdesc,
                           PTR desc, SQLSMALLINT cbmax, SQLSMALLINT *pcb, SQLLEN *pf)
{ return ((fn3_colattr)fp_SQLColAttribute)(s, col, fdesc, desc, cbmax, pcb, pf); }

SQLRETURN SQLColAttributesW(SQLHSTMT s, SQLUSMALLINT col, SQLUSMALLINT fdesc,
                            PTR desc, SQLSMALLINT cbmax, SQLSMALLINT *pcb, SQLLEN *pf)
{ return ((fn3_colattr)fp_SQLColAttributeW)(s, col, fdesc, desc, cbmax, pcb, pf); }
