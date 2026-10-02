# goldin: change history

Changes to Stéphane Sudre's goldin (0.0.7 → 0.0.8),
made by Alan Staniforth in September and October 2026.

- [goldin 0.0.8](#goldin-008)
  - [Changes](#goldin-changes)
  - [Bug fixes](#goldin-bug-fixes)
  - [Known unfixed bugs and limitations](#goldin-known-unfixed-bugs-and-limitations)

---

## goldin 0.0.8

### goldin: changes

- **More than one path.** goldin now accepts one or more `PATH` arguments and
  processes them in turn. It used to accept exactly one.
- **`--norecurse`.** Splits the files and folders directly inside each `PATH`,
  but doesn't descend into subfolders.
- **Long options.** Every option can be given in long form with `--`:
  `--help`, `--strip`, `--verbose`, `--norecurse` and `--version`.
- **Help options.** `-h` and `--help` show the usage guide. `-u` is kept as an
  alternative for backwards compatibility.
- **Unreadable folders are reported and skipped.** They used to be skipped
  silently. This includes a `PATH` that is itself an unreadable folder.
  Processing carries on with the rest of the tree.
- **Every error message names the file it is about**, and the messages are
  worded more consistently.
- **Documented exit statuses:** 0 for success, 2 for an unknown option, 254 when
  a `PATH` is not on an HFS or HFS+ volume, and 255 for any other error,
  including no `PATH`.
- **Documentation.** New goldin(1) man page, an expanded README,
  and an updated change history in `main.c`.
- **Deployment target.** The goldin target now builds for Mac OS X 10.8,
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

- **A full disk is reported as an unknown error (-43).** On current macOS,
  `FSWriteFork` returns -43 (`fnfErr`) rather than `dskFulErr` when the disk
  fills. goldin's "disk is full" message therefore never appears; it prints
  "An unknown error occurred while writing the AppleDouble file of … (-43)"
  instead. The partial file is still deleted.
  *Why:* the fault is in the File Manager, and -43 can mean other things, so
  mapping it to "disk full" could be misleading.
- **A failed copy-buffer allocation would crash goldin.** If every attempt to
  allocate the buffer for copying resource forks failed, goldin would read
  into a NULL pointer.
  *Why:* the allocation retries with smaller sizes down to 2 bytes, so in
  practice it can't fail.
- **The owner and group aren't copied unless goldin runs as root.** The
  AppleDouble files are owned by the user who runs goldin. Only the original
  file's permissions are copied.
  *Why:* `FSSetCatalogInfo` silently skips an owner or group it isn't allowed
  to set and reports success, so goldin can't detect it. Tested with a
  root-owned file and with a file in the `wheel` group. This is documented in
  goldin(1).
- **goldin uses deprecated APIs.** It is built on the File Manager `FSRef`
  calls, which have been deprecated since Mac OS X 10.8.
  *Why:* replacing them would mean rewriting most of goldin.
- **HFS and HFS+ only.** goldin refuses APFS and other file systems with
  status 254.
  *Why:* this is by design, as with SplitForks.

---

*Testing:* changes were tested on macOS with the Xcode 16.2 toolchain, using
temporary HFS+ disk images.
