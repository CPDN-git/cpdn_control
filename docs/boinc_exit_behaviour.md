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

## Controller ownership and startup checks

The controller owns termination of its model and helper processes and their
ordinary descendants whenever it can execute cleanup. This is model-independent
and applies on Linux, macOS, and Windows. BOINC client cleanup is an additional
fallback, not the owner of this responsibility.

Before staging startup archives and immediately before launching a model,
`check_boinc_before_launch()` refreshes BOINC status and writes the status flags,
phase, and decision to stderr. Quit, abort, or heartbeat loss prevents launch.
Suspension defers launch until resume or shutdown. Standalone operation skips
client polling and logs that fact.

A further poll immediately after launch covers requests received between the
last check and process creation. Suspension at this point acts on the launched
child. A shutdown request goes through the same final cleanup as any other exit.
The checks cannot make process creation atomic with arrival of a BOINC message;
controller-owned cleanup is what closes that remaining race.

Relevant code:

- `src/cpdn_control.cpp`: `check_boinc_before_launch()`
- `src/cpdn_main.cpp`: startup staging, prelaunch guard, postlaunch poll

## Owned process sets and bounded termination

Linux and macOS launches establish a private process group before returning a
successful launch handle. The model's ordinary descendants inherit this group.
Windows launches assign the child to a Job Object before resuming it. Cleanup
uses these recorded identities, never a machine-wide search for WRF or another
executable name. Processes from another BOINC slot must not be targeted.

Ownership outlives the model leader. POSIX polling uses `waitid(..., WNOWAIT)`
to observe an exit without reaping the leader. This reserves the leader's PID
and group identity until cleanup has sent its signal to surviving descendants.
Windows retains the process and Job Object handles until final cleanup.

`shutdown_child_process()` requests immediate termination (`SIGKILL` on POSIX,
`TerminateJobObject()` on Windows), then checks termination with nonblocking
polls and short sleeps. There is one monotonic wait deadline of two seconds for
the entire owned process set, not two seconds per descendant. The wait ends as
soon as termination is confirmed. There is no additional graceful-signal wait.

On POSIX, the direct child is reaped after the group has been signalled. Group
presence checks can conservatively remain positive for zombie descendants until
their parent or system reaper collects them; such a deadline expiry is logged as
unconfirmed cleanup rather than a claim that an executable is still running.
On Windows, confirmation requires both leader exit and zero active Job Object
processes.

A cleanup result distinguishes no owned process, confirmed termination,
termination failure, and deadline expiry. Stderr includes the reason, PID/group,
whether termination was requested, elapsed milliseconds, and any failure detail.
A successful signal submission alone is never reported as confirmed exit.
If the deadline expires, the controller logs the unconfirmed outcome and exits;
it does not wait indefinitely during host shutdown.

`TaskState::child_cleanup` records the model cleanup result. Controller-error
recovery and final exit share this result, so repeated cleanup calls do not
restart the wait budget. Synchronous external helpers use the same bounded
cleanup and return its structured result in `TimedProcessResult::cleanup`.
They are cleaned up on completion, timeout, or a wait failure, including any
remaining ordinary descendants. Their execution timeout remains separate from
the termination-confirmation budget.

Relevant code:

- `lib/process_control.h`, `lib/process_control.cpp`
- `lib/process_control_posix.cpp`, `lib/process_control_windows.cpp`
- `lib/utils.cpp`: `run_process_with_timeout()`
- `src/task_lifecycle.cpp`: `stop_child_for_shutdown()`

## Final exit routes and BOINC semantics

All controlled exits pass through `finish_task_exit()` in
`src/task_lifecycle.cpp`. Every route attempts owned child cleanup, including
BOINC quit, abort, and heartbeat loss. Handles are released only after that
attempt, and the BOINC critical section is unwound before returning.

| Cause | Controller exit | Upload/completion behaviour |
| --- | --- | --- |
| BOINC quit | Return 0, no finish file | Restartable; no upload finalization |
| Heartbeat loss | Return 0, no finish file | Restartable; no upload finalization |
| BOINC abort | Return 1, no finish file | No upload finalization |
| Startup or immediate controller failure | Return nonzero, no finish file | Attempt owned cleanup |
| Controller-error recovery | Return nonzero, no finish file | Stop model before finalizing uploads; skip finalization if termination is unconfirmed |
| Natural timestep-loop completion | `boinc_finish()` with model-success result | Success checked and final uploads handled by normal completion flow |

BOINC client state still determines how it classifies an exit, as described
above. In particular, a plain zero exit is not task completion, and client
`QUIT_PENDING` handling can override application exit status.

## Limits and regression coverage

Cleanup cannot execute after an uncatchable controller kill or power loss.
POSIX descendants that deliberately leave the owned process group are outside
its guarantee. Recovery or automatic termination of orphans from an earlier
controller run is outside the scope of this change.

`TaskLifecycleTest` exercises pending startup shutdown, suspension/resume,
status-read failure, the actual BOINC final exit route with running/suspended
children, immediate executable overwrite after exit, descendant cleanup after
leader exit, isolation from another launch, and deadline expiry without granting
final exit another wait budget. `LaunchProcessTest` and
`RunProcessWithTimeoutTest` cover the underlying model/helper process seams.

Keep this note, AGENTS.md, and the Doxygen comments aligned when changing
controller process ownership or BOINC exit semantics.
