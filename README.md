# DiskAnalyzer

DiskAnalyzer is a UNIX daemon + CLI pair that analyzes disk usage for a given directory. The daemon schedules analysis jobs with priorities, scans directories recursively, builds an in-memory tree of files/directories, and exposes job control (add, pause, resume, remove, info, list, print) through a UNIX domain socket. The `da` client sends requests to the daemon and prints responses.

## Key Features
- Background daemon that handles multiple analysis jobs.
- Recursive traversal with per-job progress and counters (files/dirs).
- Priority scheduling (1 = low, 2 = normal, 3 = high).
- Pause/resume and remove job controls.
- Conflict detection for nested paths (avoid duplicate sub-tree scans).
- Tree report with percentages and a simple bar graph.

## Architecture Overview
- `daemon` (`src/daemon/daemon.c`): main server process; daemonizes itself and listens on `/tmp/disk_analyzer.sock`.
- `da` (`src/client/da.c`): CLI client; sends `request_t` over the UNIX socket and prints `response_t`.
- `scheduler` (`scheduler.c/.h`): thread pool (MAX_THREADS = 4), job queue, priority selection, job state.
- `analyzer` (`src/analyzer.c`): recursive filesystem traversal, builds the result tree.
- `tree` (`src/tree.c`, `include/tree.h`): tree node implementation for file/directory results.
- `ipc` (`src/daemon/ipc.c`): UNIX socket setup.

Data flow: `da` -> UNIX socket -> `daemon` -> `scheduler` -> worker -> `analyzer` -> result tree -> `da -p <id>`.

## Build
```bash
make
```
Outputs: `./daemon` and `./da`.

Clean:
```bash
make clean
```

## Run
1. Start the daemon (it detaches and runs in the background):
```bash
./daemon
```

2. Use the CLI client:
```bash
./da -a /path/to/dir -p 3
./da -l
./da -i 0
./da -p 0
```

## CLI Usage (`da`)
```text
-a, --add <dir>       Add a new analysis job for <dir>
-p, --priority <1-3>  Set priority (only with -a)
                      OR, without -a: print report for job ID
-S, --suspend <id>    Suspend job by ID
-R, --resume <id>     Resume job by ID
-r, --remove <id>     Remove/cancel job by ID
-i, --info <id>       Print job status and statistics
-l, --list            List all active jobs
-h, --help            Show help
```

Note: `-p` is overloaded. With `-a`, it sets priority; without `-a`, it prints the report for a job ID.

## Examples
```bash
./da -a /home/user/my_repo -p 3
./da -l
./da -i 2
./da -p 2
./da -S 2
./da -R 2
./da -r 2
```

## Job Lifecycle and Status
Possible states: `PENDING`, `IN_PROGRESS`, `SUSPENDED`, `DONE`, `REMOVED`.

- A job starts in `PENDING` and moves to `IN_PROGRESS` when picked by a worker.
- `SUSPENDED` pauses the scan (the worker waits).
- `REMOVED` cancels the job.
- `DONE` means the analysis tree is ready and can be printed.

## Report Format
The report (`da -p <id>`) prints an indented tree:
```
/root_dir 100.0% (123456 bytes) ####################
 -/subdir  31.3% (38654 bytes) ######
 -/file    15.3% (18900 bytes) ###
```
Each line includes: name, percent of total, size in bytes, and a bar made of `#`.

## Path Conflict Rule
If a directory is already part of an existing analysis job, adding a subdirectory (or the same path) is rejected and the daemon reports the conflicting job ID. This prevents duplicate scans of the same subtree.

## Quick Test
The script below enqueues jobs for subdirectories in `/usr` (daemon must be running):
```bash
./tests/test.sh
```

## Project Structure
```
src/
  analyzer.c
  tree.c
  client/da.c
  daemon/daemon.c
  daemon/ipc.c
include/
  analyzer.h
  tree.h
  common.h
scheduler.c
scheduler.h
```

## Troubleshooting
- "Cannot connect to daemon": ensure `./daemon` is running and `/tmp/disk_analyzer.sock` exists.
- No report for `-p <id>`: job must be `DONE` before printing the tree.
