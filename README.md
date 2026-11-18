# BSDSF24M002-OS-A03: Custom UNIX Shell in C

A command-line shell built incrementally for the Operating Systems course
(Programming Assignment 03, PUCIT). Each feature was developed on its own
branch and published as a tagged release.

## Build and run

Requires `gcc`, `make` and the GNU Readline development library
(`sudo apt install libreadline-dev`).

```bash
make            # builds bin/myshell
./bin/myshell   # prompt: FCIT>
make clean      # removes obj/ and bin/
```

## Features

| Tag | Feature |
|---|---|
| v1.0-base | Base shell: fork, exec, wait cycle |
| v2 | Built-ins: `exit`, `cd`, `help`, `jobs` |
| v3 | Command history: `history`, `!n` |
| v4 | GNU Readline: line editing, arrow-key history, Tab completion |
| v5 | I/O redirection (`<`, `>`) and a pipe (`\|`) |
| v6 | Command chaining (`;`), background jobs (`&`), zombie reaping |
| v7 | `if / then / else / fi` driven by exit status, quote handling |
| v8 | Shell variables: `NAME=value`, `$NAME`, `set` |

## Examples

```
FCIT> ls -l > files.txt
FCIT> sort < files.txt | head -n 3
FCIT> echo one ; sleep 2 ; echo two
FCIT> sleep 10 &
FCIT> jobs
FCIT> MSG="Hello, World!"
FCIT> echo $MSG
FCIT> if grep "root" /etc/passwd > /dev/null
> then
> echo "Found root"
> else
> echo "Not found"
> fi
```

## Known limitations

- One pipe per command line, and pipelines cannot run in the background.
- `if` blocks cannot be nested, and `then` must be on its own line.
- A `;` inside quotes still splits the line, and quoted operators
  (`">"`, `"|"`) are still treated as operators.
- Built-in commands always count as successful when used as an `if` condition.
- No wildcard (`*`) expansion. Variable values containing spaces need quotes:
  `NAME="two words"`.

## Project layout

```
src/        main.c, shell.c, execute.c, history.c, jobs.c, vars.c
include/    shell.h
Makefile
```
