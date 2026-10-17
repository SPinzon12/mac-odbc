/* iODBC hands back wide diagnostics; call the W entry point directly. */
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <sql.h>
#include <sqlext.h>

SQLRETURN SQLGetDiagRecW(SQLSMALLINT, SQLHANDLE, SQLSMALLINT, SQLWCHAR *,
                         SQLINTEGER *, SQLWCHAR *, SQLSMALLINT, SQLSMALLINT *);

static void dumpw(SQLSMALLINT htype, SQLHANDLE h)
{
    SQLWCHAR state[8], text[512];
    SQLINTEGER native;
    SQLSMALLINT len;
    for (SQLSMALLINT rec = 1; rec <= 6; rec++) {
        SQLRETURN r = SQLGetDiagRecW(htype, h, rec, state, &native, text, 512, &len);
        printf("      rec=%d rc=%ld state='%ls' native=%d\n      text='%ls'\n",
               rec, (long)r, (wchar_t *)state, (int)native, (wchar_t *)text);
        if (r != SQL_SUCCESS && r != SQL_SUCCESS_WITH_INFO) break;
    }
}

int main(int argc, char **argv)
{
    SQLHENV env; SQLHDBC dbc; SQLCHAR out[1024]; SQLSMALLINT outlen = 0;
    if (argc < 2) return 2;
    SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HENV, &env);
    SQLSetEnvAttr(env, SQL_ATTR_ODBC_VERSION, (void *)SQL_OV_ODBC3, 0);
    SQLAllocHandle(SQL_HANDLE_DBC, env, &dbc);
    printf("=== %s ===\n", argc > 2 ? argv[2] : "case");
    SQLRETURN r = SQLDriverConnect(dbc, NULL, (SQLCHAR *)argv[1], SQL_NTS,
                                   out, sizeof out, &outlen, SQL_DRIVER_NOPROMPT);
    printf("SQLDriverConnect = %ld\n", (long)r);
    if (r == SQL_SUCCESS || r == SQL_SUCCESS_WITH_INFO) printf("out=%s\n", out);
    else dumpw(SQL_HANDLE_DBC, dbc);
    return 0;
}
