#!/usr/bin/env python3
"""End-to-end check of the patched driver through Excel's own iODBC framework.

Copies the connection settings of an existing DSN (default: PSQL, read from
~/Library/ODBC/odbc.ini) into a private temporary odbc.ini that points at the
driver under test, in three variants, and checks what an ANSI app like Excel
receives:

  CS     AnsiCharset = MACINTOSH  -> SQL_C_CHAR text arrives as Mac Roman
  PLAIN  no AnsiCharset           -> SQL_C_CHAR text arrives as UTF-8 (upstream)
  BOGUS  unknown charset name     -> ignored, same as PLAIN

    python3 -I tests/test-driver.py [driver.so] [DSN]

Exit code 0 when every check passes.
"""
import ctypes
import os
import re
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DRIVER = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "build/src/.libs/psqlodbcw.so")
DSN = sys.argv[2] if len(sys.argv) > 2 else "PSQL"
IODBC = "/Library/Frameworks/iODBC.framework/Versions/3.52/iODBC"

SQL_C_CHAR, SQL_C_WCHAR, SQL_NULL_DATA = 1, -8, -1
QUERY = ("select v from (values (1,'MIÉRCOLES'),(2,'año ñandú'),(3,'Łódź €'),(4,NULL))"
         " t(k,v) order by k")
TEXTS = ["MIÉRCOLES", "año ñandú", "Łódź €", None]
MAC_ROMAN = ["MIÉRCOLES", "año ñandú", "?ód? €", None]   # Ł and ź do not exist in Mac Roman
ROWS = 5000

failures = []


def check(label, got, want):
    ok = got == want
    print(f"  {'ok  ' if ok else 'FAIL'} {label}: {got!r}" + ("" if ok else f" (want {want!r})"))
    if not ok:
        failures.append(label)


def private_ini(directory):
    """Write odbc.ini with the DSN's settings (password included) as 0600."""
    src = open(os.path.expanduser("~/Library/ODBC/odbc.ini"), encoding="utf-8").read()
    m = re.search(r"\[%s\]\n(.*?)(?=\n\[|\Z)" % re.escape(DSN), src, re.S)
    if not m:
        sys.exit(f"DSN [{DSN}] not found in ~/Library/ODBC/odbc.ini")
    body = re.sub(r"(?m)^(Driver|AnsiCharset)\s*=.*\n?", "", m.group(1)).rstrip("\n")
    body = f"Driver = {DRIVER}\n{body}\n"
    ini = ("[ODBC Data Sources]\nCS = t\nPLAIN = t\nBOGUS = t\n\n"
           f"[CS]\n{body}AnsiCharset = MACINTOSH\n\n[PLAIN]\n{body}\n"
           f"[BOGUS]\n{body}AnsiCharset = NOPE-CHARSET\n")
    path = os.path.join(directory, "odbc.ini")
    fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    with os.fdopen(fd, "w", encoding="utf-8") as f:
        f.write(ini)
    return path


def run_case(dsn, want_char, encoding):
    dm = ctypes.CDLL(IODBC)
    env, dbc, st = ctypes.c_void_p(), ctypes.c_void_p(), ctypes.c_void_p()
    dm.SQLAllocHandle(1, None, ctypes.byref(env))
    dm.SQLSetEnvAttr(env, 200, ctypes.c_void_p(3), 0)
    dm.SQLAllocHandle(2, env, ctypes.byref(dbc))
    conn = f"DSN={dsn}".encode()
    print(f"== {dsn}")
    check("connect", dm.SQLDriverConnect(dbc, None, conn, len(conn), None, 0, None, 0), 0)
    if failures and failures[-1] == "connect":
        return

    def execute(sql):
        dm.SQLAllocHandle(3, dbc, ctypes.byref(st))
        dm.SQLExecDirect(st, sql.encode(), len(sql.encode()))

    def column(ctype, decode):
        execute(QUERY)
        out = []
        while dm.SQLFetch(st) in (0, 1):
            buf, ind = ctypes.create_string_buffer(256), ctypes.c_ssize_t()
            r = dm.SQLGetData(st, 1, ctype, buf, 256, ctypes.byref(ind))
            if r not in (0, 1):
                out.append(f"<SQLGetData error {r}>")
            else:
                out.append(None if ind.value == SQL_NULL_DATA else decode(buf.raw[:ind.value]))
        dm.SQLFreeHandle(3, st)
        return out

    check("SQL_C_CHAR", column(SQL_C_CHAR, lambda b: b.decode(encoding)), want_char)
    check("SQL_C_WCHAR", column(SQL_C_WCHAR, lambda b: b.decode("utf-32-le")), TEXTS)

    # Long values are read in pieces through a small buffer.
    execute(QUERY)
    dm.SQLFetch(st)
    parts = b""
    while True:
        buf, ind = ctypes.create_string_buffer(4), ctypes.c_ssize_t()
        if dm.SQLGetData(st, 1, SQL_C_CHAR, buf, 4, ctypes.byref(ind)) not in (0, 1):
            break
        parts += buf.value
    dm.SQLFreeHandle(3, st)
    check("SQL_C_CHAR chunked", parts.decode(encoding), want_char[0])

    execute(f"select g from generate_series(1,{ROWS}) g")
    n = 0
    while dm.SQLFetch(st) in (0, 1):
        n += 1
    dm.SQLFreeHandle(3, st)
    check("rows", n, ROWS)
    dm.SQLDisconnect(dbc)


def main():
    if len(sys.argv) > 3 and sys.argv[3] == "--child":
        # One process per case: iODBC caches the loaded driver per process.
        dsn, encoding = sys.argv[4], sys.argv[5]
        run_case(dsn, MAC_ROMAN if encoding == "mac_roman" else TEXTS, encoding)
        sys.exit(1 if failures else 0)

    if not os.path.isfile(DRIVER):
        sys.exit(f"driver not found: {DRIVER} (run scripts/build-driver.sh)")
    print(f"driver: {DRIVER}", flush=True)
    with tempfile.TemporaryDirectory() as tmp:
        env = dict(os.environ, ODBCINI=private_ini(tmp))
        failed = []
        for dsn, encoding in (("CS", "mac_roman"), ("PLAIN", "utf-8"), ("BOGUS", "utf-8")):
            r = subprocess.run([sys.executable, "-I", __file__, DRIVER, DSN, "--child", dsn, encoding],
                               env=env)
            if r.returncode:
                failed.append(dsn)
    print("\nPASS" if not failed else f"\nFAILED: {', '.join(failed)}")
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
