/* Reproduce Excel's path: drive the SAME iODBC framework and print the real
 * SQLGetDiagRec output instead of trusting a Python ctypes harness. */
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <sql.h>
#include <sqlext.h>

static void dump(SQLSMALLINT htype, SQLHANDLE h, const char *tag)
{
    SQLCHAR state[6], text[1024];
    SQLWCHAR wstate[6], wtext[512];
    SQLINTEGER native;
    SQLSMALLINT len;
    SQLSMALLINT rec = 1;
    printf("   [%s] diag:\n", tag);
    while (SQLGetDiagRec(htype, h, rec, state, &native, text, sizeof text, &len) == SQL_SUCCESS) {
        printf("      ANSI   state='%s' native=%d text='%.*s'\n",
               state, (int)native, (int)len, text);
        printf("      hex    state:"); for (int i=0;i<6;i++) printf(" %02x", state[i]);
        printf("\n      hex    text :"); for (int i=0;i<48;i++) printf(" %02x", text[i]);
        printf("\n");
        /* some DMs hand back UTF-16 through the ANSI entry point */
        memcpy(wstate, state, 12);
        memcpy(wtext, text, 96);
        printf("      como UTF-16: state='%ls' text='%.200ls'\n", (wchar_t *)wstate, (wchar_t *)wtext);
        rec++;
        if (rec > 6) break;
    }
    if (rec == 1) printf("      (sin registros)\n");
}

static int try_connect(SQLHENV env, const char *label, const char *conn)
{
    SQLHDBC dbc = SQL_NULL_HDBC;
    SQLCHAR out[1024];
    SQLSMALLINT outlen = 0;
    SQLAllocHandle(SQL_HANDLE_DBC, env, &dbc);
    printf("\n=== %s ===\n", label);
    SQLRETURN r = SQLDriverConnect(dbc, NULL, (SQLCHAR *)conn, SQL_NTS,
                                   out, sizeof out, &outlen, SQL_DRIVER_NOPROMPT);
    printf("   SQLDriverConnect = %ld%s\n", (long)r,
           r == SQL_SUCCESS ? " (SQL_SUCCESS)" :
           r == SQL_SUCCESS_WITH_INFO ? " (SQL_SUCCESS_WITH_INFO)" :
           r == SQL_ERROR ? " (SQL_ERROR)" : "");
    if (r == SQL_SUCCESS || r == SQL_SUCCESS_WITH_INFO)
        printf("   out = %s\n", out);
    else
        dump(SQL_HANDLE_DBC, dbc, label);
    SQLDisconnect(dbc);
    SQLFreeHandle(SQL_HANDLE_DBC, dbc);
    return (int)r;
}

int main(int argc, char **argv)
{
    SQLHENV env = SQL_NULL_HENV;
    if (argc < 2) { fprintf(stderr, "uso: %s <connstr> [label]\n", argv[0]); return 2; }
    SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HENV, &env);
    SQLSetEnvAttr(env, SQL_ATTR_ODBC_VERSION, (void *)SQL_OV_ODBC3, 0);
    int rc = try_connect(env, argc > 2 ? argv[2] : "case", argv[1]);
    SQLFreeHandle(SQL_HANDLE_ENV, env);
    return rc == SQL_SUCCESS || rc == SQL_SUCCESS_WITH_INFO ? 0 : 1;
}
