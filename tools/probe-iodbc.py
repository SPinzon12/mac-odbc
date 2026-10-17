#!/usr/bin/env python3
"""Reproduce Excel's ODBC path through the SAME iODBC framework, with real diagnostics."""
import ctypes, sys

IODBC = "/Library/Frameworks/iODBC.framework/Versions/3.52/iODBC"
HENV, HDBC = 1, 2
SQL_SUCCESS, SQL_SUCCESS_WITH_INFO, SQL_NO_DATA = 0, 1, 100
SQL_ERROR = -1

dm = ctypes.CDLL(IODBC)

def diag(handle, kind, rec=1):
    buf = ctypes.create_string_buffer(1024)
    state = ctypes.create_string_buffer(6)
    msglen = ctypes.c_short(0)
    native = ctypes.c_long(0)
    r = dm.SQLGetDiagRec(kind, handle, rec, state, ctypes.byref(native),
                         buf, 1024, ctypes.byref(msglen))
    return f"SQLGetDiagRec={r} state={state.value.decode(errors='replace')!r} msg={buf.value.decode(errors='replace')!r}"

env = ctypes.c_void_p()
print("SQLAllocHandle(ENV):", dm.SQLAllocHandle(HENV, ctypes.c_void_p(), ctypes.byref(env)))
print("SQLSetEnvAttr:", dm.SQLSetEnvAttr(env, 200, 3, 0))

print("\n--- drivers que iODBC lista ---")
desc = ctypes.create_string_buffer(256)
dl = ctypes.c_short(0)
attrs = ctypes.create_string_buffer(256)
al = ctypes.c_short(0)
i = 1
while True:
    r = dm.SQLDrivers(env, i, desc, 256, ctypes.byref(dl), attrs, 256, ctypes.byref(al))
    if r not in (SQL_SUCCESS, SQL_SUCCESS_WITH_INFO):
        print(f"(fin en rec={i}, r={r})")
        break
    print(f"  {desc.value.decode(errors='replace')}")
    i += 1

for label, conn in [
    ("ACTUAL (control)", "DSN=actual-psql"),
    ("PSQL bundled", sys.argv[1] if len(sys.argv) > 1 else "DSN=PSQL"),
]:
    print(f"\n=== {label} ===")
    print(conn.split("PWD=")[0] + ("PWD=***" if "PWD=" in conn else ""))
    dbc = ctypes.c_void_p()
    print("SQLAllocHandle(DBC):", dm.SQLAllocHandle(HDBC, env, ctypes.byref(dbc)))
    out = ctypes.create_string_buffer(1024)
    outlen = ctypes.c_short(0)
    c = conn.encode()
    r = dm.SQLDriverConnect(dbc, None, c, len(c), out, 1024, ctypes.byref(outlen), 0)
    print("SQLDriverConnect:", r, "(0=OK, -1=ERROR)")
    print("diag:", diag(dbc, HDBC))
    if r == SQL_SUCCESS:
        ncols = ctypes.c_short(0)
        print("  conectado OK")
