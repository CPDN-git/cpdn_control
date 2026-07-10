# BOINC Exit Behaviour Notes

This note records what the BOINC client does with task exits and how the
current CPDN controller interacts with those paths. The controller exit path is
critical because BOINC restart/completion decisions depend on client state at
the moment the process exits, not just on the controller's chosen exit code.

## BOINC Client Rules

BOINC client source references below are from `$HOME/github/boinc`.

### `QUIT_PENDING` takes precedence over finish-file handling

When the client is exiting, detaching a project, or resetting a project it sends
`<quit/>` to the app and sets the task state to `PROCESS_QUIT_PENDING`.

Relevant code:

- `client/app_control.cpp`: `ACTIVE_TASK::request_exit()`
- `client/app_control.cpp`: `ACTIVE_TASK_SET::exit_tasks()`

If the task exits while the client still considers it `PROCESS_QUIT_PENDING`,
BOINC ignores the app exit status and ignores finish-file semantics. Instead it
marks the task `PROCESS_UNINITIALIZED` and sets `will_restart = true`.

Relevant code:

- `client/app_control.cpp`: `ACTIVE_TASK::handle_exited_app()`

Practical consequence:

- a task may call `boinc_finish(0)` and still be restarted if BOINC handles the
  live exit while the task is still `QUIT_PENDING`

### Normal nonzero exit is terminal

If the task exits normally with a nonzero process exit status and BOINC is not
in `PROCESS_QUIT_PENDING`, the client marks the task `PROCESS_EXITED`, reports
an error, and does not restart it.

Relevant code:

- `client/app_control.cpp`: `ACTIVE_TASK::handle_exited_app()`

Practical consequence:

- plain nonzero exit is already enough to make startup/controller failure
  terminal in the ordinary running case

### Zero exit without a finish file is treated as premature and restartable

If the task exits with status `0` and there is no `boinc_finish_called` finish
file and no `boinc_temporary_exit` file, BOINC treats the exit as premature and
restartable.

Relevant code:

- `client/app_control.cpp`: `ACTIVE_TASK::handle_exited_app()`
- `client/app_control.cpp`: `ACTIVE_TASK::handle_premature_exit()`

Practical consequence:

- plain `return 0` is not a completion signal under BOINC

### `boinc_finish(0)` writes `boinc_finish_called`

`boinc_finish(status)` writes `boinc_finish_called`, records the status in that
file, and exits.

Relevant code:

- `api/boinc_api.cpp`: `boinc_finish_message()`
- `api/boinc_api.h`: `BOINC_OPTIONS::main_program` comment

The BOINC API comment states:

- nonzero `boinc_finish(status)` means unrecoverable error
- zero `boinc_finish(status)` means successful finish

However, the client-side `QUIT_PENDING` path described above can still override
that in practice if it handles the live exit first.

### Finish files are also checked on client startup

When the BOINC client starts up, it scans active tasks for `boinc_finish_called`
files before restarting them. This is intended for the case where the task
finished while the client itself was shutting down.

Relevant code:

- `client/client_state.cpp`: `check_for_finished_jobs()`
- `client/cs_apps.cpp`: `ACTIVE_TASK_SET::check_for_finished_jobs()`

Practical consequence:

- the same task can call `boinc_finish(0)` and later be treated as completed if
  BOINC handles the finish file on startup rather than via the live
  `QUIT_PENDING` exit path

### Hung after writing finish file

If the client sees `boinc_finish_called` but the process remains present for
five minutes, BOINC aborts the job as hung in `boinc_finish()`.

Relevant code:

- `client/app_control.cpp`: finish-file timeout handling

## Current CPDN Controller Behaviour

Current controller source references are from this repository.

### BOINC `QUIT` now takes an explicit restartable no-finish path

The current controller polls BOINC state in the main loop and, on quit, builds
an explicit `boinc_quit` exit decision with process exit code `0` and
`should_call_boinc_finish=false`. The controller cleans up the child and
returns from `main()` without writing a `boinc_finish_called` file.

Relevant code:

- `src/cpdn_main.cpp`: `make_boinc_shutdown_exit_decision(...)`
- `src/cpdn_main.cpp`: `shutdown_task(...)`
- `src/cpdn_main.cpp`: `exit_task(...)`

Practical consequence:

- `QUIT` no longer depends on BOINC racing between live `QUIT_PENDING` handling
  and finish-file handling
- the controller now leaves BOINC to treat the exit as a restartable premature
  exit rather than a finished result

### Current code has multiple exit routes

The current controller can exit from:

- early startup/setup failures
- BOINC quit/abort/no-heartbeat
- progress-file write failure
- scheduled upload/final upload failure
- post-loop completion

Those routes currently converge imperfectly:

- child termination is centralized in the final exit helper
- BOINC quit still bypasses post-loop diagnostics by design
- `boinc_finish(...)` is now reserved for genuine timestep-loop completion

## Current Contract

The current controller contract is:

1. `QUIT` becomes an explicit restartable non-finish path
2. `boinc_finish(...)` is reserved for genuine timestep-loop completion only
3. `check_model_success()` is run only after natural loop completion
4. non-completion failures become terminal non-finish exits
5. one helper owns final child termination and exit logging

This note should be kept in sync with the controller exit-path refactor because
it documents the contract the controller must respect when running under BOINC.
