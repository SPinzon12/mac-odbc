/* Minimal experiment: export ONLY the ODBC 2.x entry points, load the real
 * driver into the global namespace at startup, and see whether iODBC can get
 * past the ODBC 3 surface on its own. */
#include <dlfcn.h>
#include <stdio.h>
#include <stdint.h>
#include <sql.h>
#include <sqlext.h>

static const char *DRIVER =
    "/Library/ODBC/psqlodbc.bundle/Contents/MacOS/psqlodbcw.so";

__attribute__((constructor))
static void load_driver(void)
{
    if (!dlopen(DRIVER, RTLD_NOW | RTLD_GLOBAL)) {
        fprintf(stderr, "shim: no pude cargar %s: %s\n", DRIVER, dlerror());
    }
}

SQLRETURN SQLAllocEnv(SQLHENV *ph)
{ return SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HENV, (SQLHANDLE *)ph); }
SQLRETURN SQLAllocConnect(SQLHENV e, SQLHDBC *ph)
{ return SQLAllocHandle(SQL_HANDLE_DBC, e, (SQLHANDLE *)ph); }
SQLRETURN SQLAllocStmt(SQLHDBC d, SQLHSTMT *ph)
{ return SQLAllocHandle(SQL_HANDLE_STMT, d, (SQLHANDLE *)ph); }
SQLRETURN SQLFreeEnv(SQLHENV e)     { return SQLFreeHandle(SQL_HANDLE_ENV, e); }
SQLRETURN SQLFreeConnect(SQLHDBC d) { return SQLFreeHandle(SQL_HANDLE_DBC, d); }
SQLRETURN SQLTransact(SQLHENV e, SQLHDBC d, SQLUSMALLINT f)
{ return d ? SQLEndTran(SQL_HANDLE_DBC, d, f) : SQLEndTran(SQL_HANDLE_ENV, e, f); }

SQLRETURN SQLError(SQLHENV e, SQLHDBC d, SQLHSTMT s, SQLCHAR *st,
                   SQLINTEGER *nat, SQLCHAR *msg, SQLSMALLINT n, SQLSMALLINT *l)
{
    if (s) return SQLGetDiagRec(SQL_HANDLE_STMT, s, 1, st, nat, msg, n, l);
    if (d) return SQLGetDiagRec(SQL_HANDLE_DBC,  d, 1, st, nat, msg, n, l);
    return SQLGetDiagRec(SQL_HANDLE_ENV, e, 1, st, nat, msg, n, l);
}

SQLRETURN SQLGetConnectOption(SQLHDBC d, SQLUSMALLINT o, PTR p)
{ return SQLGetConnectAttr(d, (SQLINTEGER)o, p, 0, NULL); }
SQLRETURN SQLSetConnectOption(SQLHDBC d, SQLUSMALLINT o, SQLULEN v)
{ return SQLSetConnectAttr(d, (SQLINTEGER)o, (PTR)(uintptr_t)v, SQL_IS_UINTEGER); }
SQLRETURN SQLGetStmtOption(SQLHSTMT s, SQLUSMALLINT o, PTR p)
{ return SQLGetStmtAttr(s, (SQLINTEGER)o, p, 0, NULL); }
SQLRETURN SQLSetStmtOption(SQLHSTMT s, SQLUSMALLINT o, SQLULEN v)
{ return SQLSetStmtAttr(s, (SQLINTEGER)o, (PTR)(uintptr_t)v, SQL_IS_UINTEGER); }
SQLRETURN SQLParamOptions(SQLHSTMT s, SQLULEN crow, SQLULEN *pirow)
{ (void)crow; if (pirow) *pirow = 0; return SQLBulkOperations(s, SQL_ADD); }
SQLRETURN SQLColAttributes(SQLHSTMT s, SQLUSMALLINT c, SQLUSMALLINT f,
                           PTR dsc, SQLSMALLINT m, SQLSMALLINT *pc, SQLLEN *pf)
{ return SQLColAttribute(s, c, f, dsc, m, pc, pf); }
