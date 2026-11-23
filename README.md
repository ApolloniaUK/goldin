## Synopsis

Goldin: a modern replacement for SplitForks.

## Motivation

If you have been using SplitForks and FixupResourceForks to enable version
control with modern version control systems (Git &c), this technique stopped
working when FixupResourceForks disappeared in macOS Sierra.

Goldin reimplements SplitForks (with a few enhancements) in case that
should also go away.

## Improvements over SplitForks

- it can recursively process a directory tree, or with `--norecurse` just the
  top level.
- it accepts more than one path.
- with `-a` or `--apfs`, it works on APFS volumes as well as HFS and HFS+.
- long options have been added and are called with `--`.
- the source code is available.

## Usage

	$ goldin [options] PATH ...
	
	  Options:
	  --help, -h, -u       Show this usage guide
	  --verbose, -v        Verbose mode
	  --strip, -s          Strip resource fork from source after splitting
	  --apfs, -a           Allow APFS volumes as well as HFS and HFS+
	  --linksplit, -l      Split hard links too (never stripped)
	  --warnsymlink, -w    Report items split through a symbolic link
	  --dryrun, -d         List what would be split, and any problems
	  --norecurse          Do not descend into subdirectories
	  --version            Show the version of this tool

If passed a directory, Goldin will process it recursively. With `--norecurse`,
only the files and folders directly inside it are split.

Goldin accepts one or more paths and processes them in turn. Folders that cannot
be read are reported and skipped; any other error stops Goldin.

Symbolic links inside a folder are neither split nor followed; a path that is a
symbolic link, or goes through one, is resolved, and its target is split. With
`-w` or `--warnsymlink`, Goldin reports each item it splits this way.

Files with more than one hard link are skipped unless `-l` or `--linksplit` is
given. Even then, `--strip` leaves their resource fork in place: stripping it
through one name would remove it from the file under all its names, and those
elsewhere may have no AppleDouble file.

`-d` or `--dryrun` checks a run without writing anything. It lists what would
be split, with a `WARNING:` line under each item for every problem found, such
as a resource fork that can't be read, a folder that can't be written to, a
name that is too long or a volume without enough free space. With `-v`, it
lists everything, marking each item `Process:` or `Ignored:`. It exits with
status 1 if it found any problems.

Like SplitForks, Goldin only works on HFS and HFS+ volumes unless `-a` or
`--apfs` is given, which allows APFS volumes as well. Other file systems are
always refused: on those, macOS keeps a file's resource fork and Finder
information in its own `._` file, which Goldin's AppleDouble file would collide
with.

`-u` is kept as an alternative to `-h` for backwards compatibility.

## Compatibility

Minimum OS requirement is Mac OS X 10.8 at this time.

## Tests

Description forthcoming.

## License

Copyright (c) 2006-2014, Stephane Sudre
Portions copyright (c) 2026, Alan Staniforth
All rights reserved.
 
Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
 
- Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
- Neither the name of the WhiteBox nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.
 
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 