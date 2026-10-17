#!/bin/bash
# Export a PostgreSQL table or query straight to CSV with psql. No ODBC, no Excel.
# Connection settings come from an ODBC DSN in ~/Library/ODBC/odbc.ini
# (default PSQL; override with DSN=<name>).
#
#   ./export-csv.sh mi_tabla                    -> mi_tabla.csv
#   ./export-csv.sh mi_tabla salida.csv
#   ./export-csv.sh --sql "SELECT * FROM v WHERE activo"  reporte.csv
set -euo pipefail

PSQL=/opt/homebrew/opt/libpq/bin/psql
INI=~/Library/ODBC/odbc.ini
DSN=${DSN:-PSQL}

# Value of key $1 inside the [$DSN] section of $INI.
dsn_get() {
  awk -v sec="[$DSN]" -v key="$1" '
    /^\[/ { insec = ($0 == sec); next }
    insec {
      split($0, kv, "=")
      k = kv[1]; gsub(/[ \t]/, "", k)
      if (k == key) { v = substr($0, index($0, "=") + 1); gsub(/^[ \t]+|[ \t]+$/, "", v); print v; exit }
    }' "$INI"
}

[[ -x $PSQL ]] || { echo "psql no encontrado" >&2; exit 1; }
[[ -f $INI ]] || { echo "No existe $INI" >&2; exit 1; }

HOST=$(dsn_get Servername)
PORT=$(dsn_get Port)
DB=$(dsn_get Database)
USER=$(dsn_get Username)
export PGPASSWORD=$(dsn_get Password)
for v in HOST DB USER PGPASSWORD; do
  [[ -n ${!v} ]] || { echo "Falta $v en el DSN [$DSN] de $INI" >&2; exit 1; }
done

CONN=(-h "$HOST" -p "${PORT:-5432}" -d "$DB" -U "$USER" -w)

if [[ "${1:-}" == "--sql" ]]; then
  [[ -n ${2:-} ]] || { echo "Falta la consulta" >&2; exit 1; }
  SQL=$2; OUT=${3:-export.csv}
else
  [[ -n ${1:-} ]] || { echo "Uso: $0 <tabla> [salida.csv]  |  $0 --sql \"SELECT ...\" [salida.csv]"; exit 1; }
  SQL="SELECT * FROM $1"; OUT=${2:-${1//./_}.csv}
fi

"$PSQL" "${CONN[@]}" -v ON_ERROR_STOP=1 \
  -c "\copy ($SQL) TO STDOUT WITH CSV HEADER" > "$OUT"

echo "Escrito: $OUT"
echo "Filas (sin encabezado): $(($(wc -l < "$OUT") - 1))"
