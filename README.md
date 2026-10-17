# mac-odbc

A free PostgreSQL ODBC driver that Excel for Mac accepts: no row limit, and
accented text displayed correctly.

It is upstream [psqlodbc](https://github.com/postgresql-interfaces/psqlodbc)
18.00.0005, built against libiodbc and patched with one DSN option
(`AnsiCharset`), packaged as a self-contained bundle in `/Library/ODBC`.

## Why

Excel for Mac only reaches databases through iODBC. The common options fail:

| Driver | Problem |
|---|---|
| Actual Technologies (commercial) | Without a license key it returns only the **first 3 rows** |
| psqlodbc from Homebrew | Built for unixODBC: 2-byte `SQLWCHAR` (UTF-16). iODBC uses 4-byte `SQLWCHAR` (UTF-32), so Excel fails with `Specified driver could not be loaded` |

Building psqlodbc against libiodbc gives it the same ABI the Actual drivers
use. That is enough to load it and fetch every row.

One problem remains: Excel fetches text as `SQL_C_CHAR` and decodes those
bytes as **Mac Roman**, while psqlodbc's Unicode driver sends UTF-8.
`MIÉRCOLES` then shows up as `MI√âRCOLES`. PostgreSQL has no Mac Roman client
encoding, macOS has no Mac Roman locale, and psqlodbc's own locale conversion
is compiled out on macOS (Apple does not define `__STDC_ISO_10646__`). The
patch adds a DSN key that converts that text with iconv.

## Install

Requirements: Homebrew with `libiodbc libpq autoconf automake libtool`.

```sh
bash scripts/build-driver.sh          # download upstream, verify, patch, build
python3 -I tests/test-driver.py       # end-to-end check (uses your PSQL DSN)
sudo bash scripts/install-bundle.sh   # install /Library/ODBC/psqlodbc.bundle
```

`install-bundle.sh` keeps the previous bundle and `/Library/ODBC/odbc.ini` as
`*.bak-<timestamp>` and prints the rollback command.

Then create the DSN Excel will use, and quit Excel with Cmd+Q so it reloads
the driver.

## Managing DSNs

`scripts/odbc-dsn` creates, edits, tests, renames and deletes DSNs for this
driver. Run it without arguments for an interactive menu, or use a
subcommand. Every menu shows the same controls:

```
↑↓  |  Enter  |  M More  |  V Version  |  Q Quit
```

`M` shows the command reference and `V` the tool and driver versions; both
return to the menu on any key. In sub-menus (picking a DSN) `Q` goes back,
and the footer says `Q Back`.

```sh
scripts/odbc-dsn                       # interactive menu
scripts/odbc-dsn list
scripts/odbc-dsn add erp --host 10.0.0.9 --database erp --user bob   # asks the password
scripts/odbc-dsn test erp              # connects through Excel's iODBC
scripts/odbc-dsn edit erp --port 6543 --password
scripts/odbc-dsn rename erp erp_prod
scripts/odbc-dsn rm erp_prod
```

- New DSNs get `Port = 5432`, `SSLmode = disable` and `AnsiCharset = MACINTOSH`
  unless told otherwise.
- Passwords are typed hidden, or read from `ODBC_DSN_PASSWORD`, and are never
  printed (`show` masks them).
- Every change backs up `odbc.ini` as `odbc.ini.bak-<timestamp>` and rewrites
  the file in place, so the hard link Excel reads (see below) stays intact.
  The file is set to mode 600 because it holds passwords.
- After a rename, Excel workbooks connected to the old name must be pointed
  at the new one.

A DSN written by hand looks like this:

```ini
[PSQL]
Driver      = /Library/ODBC/psqlodbc.bundle/Contents/MacOS/psqlodbcw.so
Servername  = <host>
Port        = 5432
Database    = <database>
Username    = <user>
AnsiCharset = MACINTOSH
```

### Which odbc.ini Excel reads

Excel is sandboxed. Its user DSNs live in
`~/Library/Containers/com.microsoft.Excel/Data/Library/ODBC/odbc.ini`, which
here is a **hard link** to `~/Library/ODBC/odbc.ini`. User DSNs override system
DSNs in `/Library/ODBC/odbc.ini`.

- A stale user DSN pointing at another driver produces `Specified driver could
  not be loaded` even when the system DSN is correct.
- Edit that file **in place**. Tools that write a new file and rename it
  (`sed -i`, many editors) break the hard link, and Excel keeps the old copy.
- The container copy is protected by macOS privacy controls, so the terminal
  cannot read it; `stat -f '%i'` on both paths shows whether they are still
  linked.

## `AnsiCharset`

| Value | `SQL_C_CHAR` text |
|---|---|
| not set | UTF-8, exactly as upstream |
| `MACINTOSH` | Mac Roman, which is what Excel for Mac expects |
| any iconv name | that charset |
| unknown name | ignored, behaves as not set |

- Only text columns requested as `SQL_C_CHAR` are converted. `SQL_C_WCHAR`
  and the other types are unchanged.
- Characters that do not exist in the charset become `?` (for example `Ł`,
  `→`, emoji). macOS iconv ignores `//TRANSLIT`, so the row is never failed
  over one character.
- The key is read from the DSN when connecting, so it does not apply to
  DSN-less connection strings.

## Known limitations

- Column **names** and SQL text typed in Excel take iODBC's ANSI-to-Unicode
  path, not this conversion, so accents there are not handled yet.
- Characters outside the Basic Multilingual Plane (emoji) in ANSI query text
  are already mangled by that path before they reach the driver.
- There is no automated test without a reachable database:
  `tests/test-driver.py` needs a working DSN.

## Layout

```
patches/   psqlodbc-ansi-charset.patch: the only change to upstream (5 files)
scripts/   odbc-dsn (DSN manager), build-driver.sh, install-bundle.sh,
           export-csv.sh (CSV via psql, no ODBC)
tests/     test-driver.py: checks the driver through Excel's iODBC framework
           test-odbc-dsn.sh: odbc-dsn against a scratch, hard-linked odbc.ini
tools/     probe-iodbc.py (iODBC ctypes harness), logdlsym.c (dlsym interposer)
attic/     approaches that were tried and abandoned, kept for reference
build/     generated: downloaded source and build output (not tracked)
```

`attic/` holds:

- `unixodbc-bundle/`: bundling the Homebrew (unixODBC) driver. It loads,
  but iODBC rejects its UTF-16 ABI.
- `odbc2-shim/`: a shim in front of that driver that added ODBC 2.x entry
  points and converted UTF-32 to UTF-8 call by call. It connected, but every
  wide function would have needed the same bridge.
- `probes/`: one-off diagnostics from that investigation.
