/* Adapter that gives an ODBC 3-only driver the ODBC 2.x entry points that
 * iODBC expects. Links against the real psqlodbc; dlsym() on this handle walks
 * into the dependency, so the ODBC 3 surface comes along for free. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sql.h>
#include <sqlext.h>

SQLRETURN SQLAllocEnv(SQLHENV *ph)
{ return SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HENV, (SQLHANDLE *)ph); }

SQLRETURN SQLAllocConnect(SQLHENV e, SQLHDBC *ph)
{ return SQLAllocHandle(SQL_HANDLE_DBC, e, (SQLHANDLE *)ph); }

SQLRETURN SQLAllocStmt(SQLHDBC d, SQLHSTMT *ph)
{ return SQLAllocHandle(SQL_HANDLE_STMT, d, (SQLHANDLE *)ph); }

SQLRETURN SQLFreeEnv(SQLHENV e)
{ return SQLFreeHandle(SQL_HANDLE_ENV, e); }

SQLRETURN SQLFreeConnect(SQLHDBC d)
{ return SQLFreeHandle(SQL_HANDLE_DBC, d); }

SQLRETURN SQLTransact(SQLHENV e, SQLHDBC d, SQLUSMALLINT f)
{
    if (d) return SQLEndTran(SQL_HANDLE_DBC, d, f);
    return SQLEndTran(SQL_HANDLE_ENV, e, f);
}

SQLRETURN SQLError(SQLHENV e, SQLHDBC d, SQLHSTMT s,
                   SQLCHAR *state, SQLINTEGER *native, SQLCHAR *msg,
                   SQLSMALLINT buflen, SQLSMALLINT *len)
{
    if (s) return SQLGetDiagRec(SQL_HANDLE_STMT, s, 1, state, native, msg, buflen, len);
    if (d) return SQLGetDiagRec(SQL_HANDLE_DBC,  d, 1, state, native, msg, buflen, len);
    return SQLGetDiagRec(SQL_HANDLE_ENV,  e, 1, state, native, msg, buflen, len);
}

/* ODBC 2 connection/statement option numbers -> ODBC 3 attribute numbers. */
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
{ return SQLGetConnectAttr(d, opt_to_attr(o), p, 0, NULL); }

SQLRETURN SQLSetConnectOption(SQLHDBC d, SQLUSMALLINT o, SQLULEN v)
{ return SQLSetConnectAttr(d, opt_to_attr(o), (PTR)(uintptr_t)v, SQL_IS_UINTEGER); }

SQLRETURN SQLGetStmtOption(SQLHSTMT s, SQLUSMALLINT o, PTR p)
{ return SQLGetStmtAttr(s, opt_to_attr(o), p, 0, NULL); }

SQLRETURN SQLSetStmtOption(SQLHSTMT s, SQLUSMALLINT o, SQLULEN v)
{ return SQLSetStmtAttr(s, opt_to_attr(o), (PTR)(uintptr_t)v, SQL_IS_UINTEGER); }

SQLRETURN SQLParamOptions(SQLHSTMT s, SQLULEN crow, SQLULEN *pirow)
{
    (void)crow;
    if (pirow) *pirow = 0;
    return SQLBulkOperations(s, SQL_ADD);
}

SQLRETURN SQLColAttributes(SQLHSTMT s, SQLUSMALLINT col, SQLUSMALLINT fdesc,
                           PTR desc, SQLSMALLINT cbmax, SQLSMALLINT *pcb,
                           SQLLEN *pf)
{ return SQLColAttribute(s, col, fdesc, desc, cbmax, pcb, pf); }
