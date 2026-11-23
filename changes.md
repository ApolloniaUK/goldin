# goldin: change history

Changes to Stéphane Sudre's goldin (0.0.7 → 0.0.8 → 0.1.0), made by Alan
Staniforth in September and October 2026.

- [goldin 0.0.8 → 0.1.0 (7 October 2026)](#goldin-008--010-7-october-2026)
  - [Changes](#goldin-010-changes)
  - [Bug fixes](#goldin-010-bug-fixes)
  - [Known unfixed bugs and limitations](#goldin-010-known-unfixed-bugs-and-limitations)
- [goldin 0.0.8](#goldin-008)
  - [Changes](#goldin-changes)
  - [Bug fixes](#goldin-bug-fixes)
  - [Known unfixed bugs and limitations](#goldin-known-unfixed-bugs-and-limitations)

---

## goldin 0.0.8 → 0.1.0 (7 October 2026)

goldin has been rewritten without the File Manager, keeping its output the
same: on HFS+ and APFS, the AppleDouble files that 0.1.0 writes are
byte-for-byte identical to those from 0.0.8, and its messages and exit
statuses are the same, except where listed below.

### goldin 0.1.0: changes

- **No deprecated APIs.** goldin no longer uses the File Manager (`FSRef`)
  calls, deprecated since Mac OS X 10.8, or CoreServices. Instead it uses:
  - `getattrlist` for Finder information and resource fork sizes
  - `getxattr` to read resource forks, and `removexattr` for `--strip`
  - `fts` to walk folders
  - `open`, `write`, `fchmod`, `fchown` and `unlink` for the AppleDouble files

  All of these exist from Mac OS X 10.4, so the 10.5 deployment target is
  unchanged. Nothing had to change in the Xcode project: CoreServices was
  linked automatically through clang modules, and isn't linked now.
- **Finder information is copied as read.** It already has the big-endian
  layout that AppleDouble files use, so the byte swapping of the old `FileInfo`
  and `FolderInfo` structures is gone. Tested: the result is identical, for
  files and folders, with every field set.
- **`-l` / `--linksplit`.** Splits files with more than one hard link too,
  giving each name its own AppleDouble file (byte-for-byte what SplitForks
  writes for them). Without it, hard links are still skipped, as goldin always
  has. With `--strip`, these files are split but keep their resource fork:
  stripping it through one name would remove it from the file under all its
  names, and those elsewhere may have no AppleDouble file.
- **`-w` / `--warnsymlink`.** A `PATH` that is a symbolic link, or goes
  through one, is still resolved and its target split (and with `--strip`,
  stripped), as SplitForks and goldin always have, since build systems may
  rely on it. With `-w`, goldin reports each item it splits this way on
  standard error ("Warning: split and stripped … , reached through the
  symbolic link "…""), without changing the exit status.
- **`-d` / `--dryrun`.** Checks a run without writing anything, and ignores
  `--strip`. It lists every file and folder that would be split, with paths
  shown from where their common path ends (the first line gives it). Under
  an item, each problem found gets a line indented by four spaces and
  beginning "WARNING: "; problems that would stop goldin end with "goldin
  stops here", and the dry run carries on so that all of them are listed. It
  checks, without writing, that:
  - the Finder information and the resource fork can be read (with a trial
    read), and the fork isn't over 4 GB;
  - the AppleDouble file's name (255 UTF-16 units on HFS+ and APFS, 31
    characters on HFS) and path aren't too long, and the item isn't the root
    of a volume;
  - the folder can be written to and isn't locked;
  - any existing AppleDouble file can be replaced;
  - the volume has room for all the AppleDouble files, in allocation blocks.

  Folders that can't be read, `PATH`s that can't be processed and, with `-w`,
  items reached through a symbolic link also get warnings. With `-v`, every
  item is listed, beginning "Process: " or "Ignored: ", with the reason in
  parentheses for ignored ones. It exits with status 0 if it found no
  problems and 1 if it found some. Tested: each problem it predicts is where
  a real run stops, and the disk-space estimate picked the file where a real
  run ran out of space.
- **Unreadable files without a resource fork no longer stop goldin on HFS+.**
  goldin 0.0.8 stopped with status 255 on any file it couldn't read on HFS+,
  even one with nothing to copy, because `FSOpenFork` failed (-5000). On APFS
  it didn't. Now, on both, only an unreadable file that *has* a resource fork
  stops goldin; one with only Finder information is split.
- **Error messages give the system's reason** (for example "Permission
  denied") where they used to end with a File Manager error number (for
  example -5000). "An unknown error occurred while writing the AppleDouble
  file of …" is now "Unable to write the AppleDouble file of … (reason)", and
  the message for permissions that can't be set now includes the reason.
- **Documentation.** goldin(1) now covers `-l`, `-w` and `-d`, and says that
  symbolic links inside a folder are neither split nor followed while a `PATH`
  that is a symbolic link is resolved to its target, that volumes mounted
  inside a folder are skipped, that the root of a volume can't be split
  itself, and that the hidden resource fork of a compressed file is left
  alone. The README's usage and notes cover the new options and links.
  `main.c` has a new change-history row and documents the AppleDouble layout
  goldin writes.

### goldin 0.1.0: bug fixes

- **A full disk was reported as an unknown error (-43).** goldin now reports
  "Unable to write the AppleDouble file of … (the disk is full)", because
  `write` returns `ENOSPC` where `FSWriteFork` returned -43. The partial
  `._` file is still deleted, and with `--strip` the resource fork is kept.
  (A known issue in 0.0.8.)
- **A failed copy-buffer allocation would crash goldin.** Resource forks are
  now copied through a fixed 1 MB buffer, so there is no allocation to fail.
  (A known issue in 0.0.8.)
- **Compressed files on APFS.** macOS keeps the data of a compressed file in a
  hidden resource fork. With `-a`, goldin 0.0.8 copied it into the AppleDouble
  file as if it were a real resource fork (goldout would then have added it to
  the file), even for compressed files with no Finder information. With
  `--strip` it then stopped with "Resource fork could not be stripped from …
  (-36)". goldin now leaves the hidden fork alone on APFS, as it already did on
  HFS+.
- **A volume mounted inside a folder got a bogus AppleDouble file.** goldin
  0.0.8 didn't descend into the volume, but wrote a `._` file for its mount
  point with meaningless Finder information (`lcmt`, `rhap`) and permissions
  000. Mounted volumes are now skipped.
- **Misleading error for the root of a volume.** When a `PATH` was the root of
  a volume and had Finder information, goldin 0.0.8 failed with "Unable to
  replace the existing AppleDouble file of … (-47)". It now says "Unable to
  create the AppleDouble file of … (it is the root of a volume)". The exit
  status is still 255.
- **Errors on very deep paths didn't name the file.** For a path longer than
  1,024 bytes, goldin 0.0.8 reported "An error occurred while getting the
  catalog information of a file or directory (-43)". The message now names the
  folder and says "File name too long".
- **Debug builds didn't compile.** The Debug configuration defines `DEBUG`, and
  goldin's `DEBUG` version of `logerror` ended with a semicolon, so
  `if (…) logerror(…); else …` was a syntax error.

### goldin 0.1.0: known unfixed bugs and limitations

- **The owner and group aren't copied unless goldin runs as root.** Only the
  original file's permissions are copied, as in 0.0.8.
  *Why:* goldin 0.0.8 never copied them for anyone but root, even when the user
  could have set the group (tested with a file in the `admin` group, which the
  user belongs to). 0.1.0 calls `fchown` only as root to keep that behaviour,
  which goldin(1) documents.
- **HFS, HFS+ and (with `-a`) APFS only.** goldin refuses other file systems
  with status 254.
  *Why:* on those file systems macOS stores resource forks in its own `._`
  files, which goldin's AppleDouble files would collide with. APFS needs `-a`
  so that, by default, goldin behaves as SplitForks does.
- **The root of a volume can't be split itself.** If a `PATH` is the root of a
  volume and has Finder information, goldin reports an error and stops (255).
  Its contents are split if it has none.
  *Why:* its AppleDouble file would have to go in a folder on another volume.
- **Paths longer than 1,024 bytes can't be processed.** goldin stops with
  status 255 at the first item whose path is too long.
  *Why:* this is the macOS limit for a path (`PATH_MAX`). goldin 0.0.8 stopped
  at the same item.

*Testing:* goldin 0.1.0 was compared with goldin 0.0.8 (built from commit
`ea72ef9`) on temporary HFS+ disk images and on APFS (`-a`), on identical test
trees. The trees included files with resource forks (one of 2.5 MB, more than
the copy buffer), Finder information and extended Finder information, folders,
symbolic links, hard links, empty resource forks, a FIFO, compressed files,
non-ASCII and 253- and 254-character names, setuid and group-owned files, and
existing `._` files. Every AppleDouble file was compared with `cmp`, along with
standard output, standard error, exit statuses and the AppleDouble files'
permissions. Also tested: `--norecurse`, several `PATH`s, symbolic-link and
hard-link `PATH`s, read-only and unreadable folders, unreadable files, a
missing `PATH`, no `PATH`, unknown options, a FAT image, APFS without `-a`, a
full disk, a volume root, a volume mounted inside the tree, a failed `--strip`
and paths over 1,024 bytes. A round trip (goldin `--strip`, then goldout)
restored every resource fork and all Finder information exactly, on both HFS+
and APFS. The Xcode Release and Debug builds succeed with no warnings, and the
Release build is signed as before.

---

## goldin 0.0.8

### goldin: changes

- **More than one path.** goldin now accepts one or more `PATH` arguments and
  processes them in turn. It used to accept exactly one.
- **`--norecurse`.** Splits the files and folders directly inside each `PATH`,
  but doesn't descend into subfolders.
- **`-a` / `--apfs`.** Allows APFS volumes as well as HFS and HFS+. Without it,
  goldin refuses APFS as before, now with a message suggesting `-a`. Other file
  systems are always refused, because macOS keeps resource forks there in its
  own `._` files, which goldin's AppleDouble files would collide with. Tested:
  the AppleDouble files goldin writes on APFS are byte-for-byte identical to
  those it writes on HFS+, and they convert back with goldout.
- **Long options.** Every option can be given in long form with `--`:
  `--help`, `--apfs`, `--strip`, `--verbose`, `--norecurse` and `--version`.
- **Help options.** `-h` and `--help` show the usage guide. `-u` is kept as an
  alternative for backwards compatibility.
- **Unreadable folders are reported and skipped.** They used to be skipped
  silently. This includes a `PATH` that is itself an unreadable folder.
  Processing carries on with the rest of the tree.
- **Every error message names the file it is about**, and the messages are
  worded more consistently.
- **Documented exit statuses:** 0 for success, 2 for an unknown option, 254 when
  a `PATH` is not on a supported volume (HFS or HFS+, or with `-a` also APFS),
  and 255 for any other error,
  including no `PATH`.
- **Documentation.** New goldin(1) man page, an expanded README,
  and an updated change history in `main.c`.
- **Deployment target.** The goldin target now builds for Mac OS X 10.5,
  matching goldout. It was 10.13.

### goldin: bug fixes

- **Short options didn't work.** The option string passed to `getopt_long` was
  `"vdt:F:"`, apparently copied from another tool, so `-h` and `-s` were
  rejected with "invalid option". It is now `"hsuv"`.
- **`getopt_long`'s return value was stored in a `char`.** It is now an `int`,
  which `getopt_long` returns.
- **Wrong exit statuses for help and usage errors.** `--help` exited with
  status 1. It now exits 0, and an unknown option exits 2.
- **`--strip` could destroy a resource fork that had never been copied.** If
  the new AppleDouble file couldn't be opened for writing, goldin carried on
  and, with `--strip`, deleted the original file's resource fork. It now
  reports the error, removes the empty AppleDouble file and stops.
- **Errors that stopped goldin without a message.** These all exited with
  status 255 and printed nothing:
  - the AppleDouble file couldn't be created, for example in a read-only
    folder, or an existing one couldn't be replaced
  - the AppleDouble file's permissions couldn't be set (the message was
    commented out)
  - the file couldn't be examined with `lstat`
  - `--strip` failed to remove the resource fork
  - `realpath` or `statfs` failed for any reason other than "not found"

  Each one now prints a message.
- **Catalog-information errors.** On a `PATH`, the error was reported but
  goldin carried on and exited 0. Inside a tree, the message didn't name the
  file. Both now name the file and stop with status 255.
- **Partial AppleDouble files were left behind.** A write error part-way
  through left an incomplete `._` file. It is now deleted.
- **The folder iterator could be closed twice** if reopening it after a split
  failed.
- **Wrong or unclear messages:**
  - "director" for "directory"
  - a 2 GB fork-size limit that is really 4 GB
  - "Your quota are exceeded"
  - a `pathconf` failure described as a "File System Reference" error
  - the longest file name that can be split given as 255 characters when it
    is 253, since two characters are needed for `._`

### goldin: known unfixed bugs and limitations

- **The owner and group aren't copied unless goldin runs as root.** The
  AppleDouble files are owned by the user who runs goldin. Only the original
  file's permissions are copied.
  *Why:* `FSSetCatalogInfo` silently skips an owner or group it isn't allowed
  to set and reports success, so goldin can't detect it. Tested with a
  root-owned file and with a file in the `wheel` group. This is documented in
  goldin(1).
- **HFS, HFS+ and (with `-a`) APFS only.** goldin refuses other file systems
  with status 254.
  *Why:* on those file systems macOS stores resource forks in its own `._`
  files, which goldin's AppleDouble files would collide with. APFS needs `-a`
  so that, by default, goldin behaves as SplitForks does.

*Fixed in 0.1.0:* three other known issues in 0.0.8: a full disk reported as
-43, a copy-buffer allocation that could crash goldin, and the use of
deprecated APIs. See [goldin 0.0.8 → 0.1.0](#goldin-008--010-7-october-2026).

---

*Testing:* changes were tested on macOS with the Xcode 16.2 toolchain, using
temporary HFS+ disk images for goldin 0.0.8, and both HFS+ and APFS for goldin
0.1.0 (see its section).
