#!/bin/bash
# Register psqlodbc (Homebrew) in the SYSTEM ODBC config that Excel reads.
# Run with: sudo bash setup-psql-odbc.sh
set -euo pipefail

SYS_ODBC_DIR=/Library/ODBC
STAMP=$(date +%Y%m%d-%H%M%S)

[[ $EUID -eq 0 ]] || { echo "Needs root: sudo bash $0" >&2; exit 1; }
[[ -f /usr/local/lib/psqlodbcw.so ]] || { echo "Driver missing: /usr/local/lib/psqlodbcw.so" >&2; exit 1; }

cp -p "$SYS_ODBC_DIR/odbcinst.ini" "$SYS_ODBC_DIR/odbcinst.ini.bak-$STAMP"
cp -p "$SYS_ODBC_DIR/odbc.ini"     "$SYS_ODBC_DIR/odbc.ini.bak-$STAMP"
echo "Backups written with suffix -$STAMP"

# --- drivers: keep the four Actual entries, add psql ---
cat > "$SYS_ODBC_DIR/odbcinst.ini" <<'EOF'
[ODBC Drivers]
Actual Access                = Installed
Actual Open Source Databases = Installed
Actual Oracle                = Installed
Actual SQL Server            = Installed
psql                         = Installed

[Actual Access]
Driver = /Library/ODBC/Actual Access.bundle/Contents/MacOS/ataccess.so
Setup  =

[Actual Open Source Databases]
Driver = /Library/ODBC/Actual Open Source Databases.bundle/Contents/MacOS/atopnsrc.so
Setup  =

[Actual Oracle]
Driver = /Library/ODBC/Actual Oracle.bundle/Contents/MacOS/atoradb.so
Setup  =

[Actual SQL Server]
Driver = /Library/ODBC/Actual SQL Server.bundle/Contents/MacOS/atsqlsrv.so
Setup  =

[psql]
Driver = /usr/local/lib/psqlodbcw.so
Setup  =
EOF

# --- DSNs: keep actual-psql (Actual), add PSQL (psqlodbc). No password stored.
cat > "$SYS_ODBC_DIR/odbc.ini" <<'EOF'
[ODBC Data Sources]
actual-psql = Actual Open Source Databases
PSQL        = psql

[actual-psql]
Driver      = /Library/ODBC/Actual Open Source Databases.bundle/Contents/MacOS/atopnsrc.so
Server      = <host>
Database    = <database>
Type        = PostgreSQL
SSLMODE     = DISABLED
SSLVERIFY   = NO
UserID      = <user>
UseKeychain = Yes

[PSQL]
Driver     = /usr/local/lib/psqlodbcw.so
Servername = <host>
Port       = 5432
Database   = <database>
Username   = <user>
SSLmode    = disable
ReadOnly   = Yes
EOF

chmod 644 "$SYS_ODBC_DIR/odbcinst.ini"
chmod 600 "$SYS_ODBC_DIR/odbc.ini"

echo
echo "Registered drivers:"
grep -A20 '^\[ODBC Drivers\]' "$SYS_ODBC_DIR/odbcinst.ini" | grep '=' || true
echo
echo "Registered DSNs:"
grep -A10 '^\[ODBC Data Sources\]' "$SYS_ODBC_DIR/odbc.ini" | grep '=' || true
