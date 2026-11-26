# BOINC Exit Behaviour Notes

This note records what the BOINC client does with task exits and how the
current CPDN controller interacts with those paths. The controller exit path is
critical because BOINC restart/completion decisions depend on client state at
the moment the process exits, not just on the controller's chosen exit code.

## BOINC Client Rules

BOINC client source references below were checked against the local
`$HOME/github/boinc` checkout at `client_release/8.0/8.0.2`. They describe that
implementation, not a guarantee about every deployed client version. The remote
clients in the overwrite-error logs were not identified.

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

## Executable overwrite incident and evidence

Two supplied task logs identify controller commit `06d51d04`. Both show a model
launch, an observed BOINC quit request five seconds later, and a restart that
fails to overwrite `wrf_4.6.1_urban.exe` with Linux `ETXTBSY` (Text file busy).
One task fails again six seconds after the first retry.

The confirmed controller defect was that the BOINC shutdown exit route skipped
child termination and delegated it to the client. A missed descendant in BOINC's
saved process list is a strong explanation for a surviving model. The exact
interleaving remains unproven: the logs record when the controller noticed quit,
not when the client sent it or enumerated descendants. Five seconds is the
controller polling interval, and the repeated failure does not by itself rule
out delayed termination. An unreaped zombie alone does not keep the executable
image busy.

The corrective implementation records current-run process ownership and attempts
termination on every controlled exit. It does not recover old orphans, and a
bounded confirmation deadline cannot guarantee that a process delayed in the
kernel has released its executable before a rapid restart.

## Controller ownership and startup checks

The controller owns termination of its model and helper processes and their
ordinary descendants whenever it can execute cleanup. This is model-independent
and applies on Linux, macOS, and Windows. BOINC client cleanup is an additional
fallback, not the owner of this responsibility.

Before staging startup archives and immediately before launching a model,
`check_boinc_before_launch()` reads the latest received BOINC status and writes
the status flags, phase, and decision to stderr. Observed quit, abort, or
heartbeat loss prevents launch. Suspension defers launch until resume or
shutdown. Standalone operation skips client polling and logs that fact.

`boinc_get_status()` copies flags maintained by BOINC's background message
processing; it does not synchronously drain pending client IPC. A clear snapshot
therefore does not prove that no shutdown message is queued. The immediate
postlaunch poll can observe a request received across process creation, but a
request not yet received can remain invisible to both closely spaced checks.

Suspension observed after launch acts on the actual child. Observed shutdown
goes through the same final cleanup as any other exit. The checks reduce
unnecessary launches; controller-owned cleanup handles late-observed requests.
The normal main-loop poll interval is five seconds, and synchronous work can
delay the next controller-side check.

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
If the deadline expires, the controller logs the unconfirmed outcome and
continues its selected exit path without another indefinite wait. This bounds
cleanup confirmation, not total shutdown latency or the time the OS takes to
terminate a process.

`TaskState::child_cleanup` records the model cleanup result. Controller-error
recovery and final exit share this result, so repeated cleanup calls do not
restart the wait budget. Synchronous external helpers use the same bounded
cleanup and return its structured result in `TimedProcessResult::cleanup`.
They are cleaned up on completion, timeout, or a wait failure, including any
remaining ordinary descendants. Their execution timeout remains separate from
the termination-confirmation budget. OpenIFS diagnostics currently have a
30-second execution timeout and do not poll BOINC status while running. A quit
arriving during that call can wait for completion or timeout before `main()`
initiates model cleanup. Other synchronous operations can also delay polling.

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
| Normal post-loop completion route | `boinc_finish()` with the previously selected model-success result | Model finalization and uploads precede remaining-descendant cleanup; cleanup failure does not override the completion code |

BOINC client state still determines how it classifies an exit, as described
above. In particular, a plain zero exit is not task completion, and client
`QUIT_PENDING` handling can override application exit status.

## Lifecycle review decisions

Review confirmed that the current OpenIFS and WRF models use threads for
parallel execution and do not create model subprocess descendants. The shared
process-group/Job Object containment remains in place, but findings 1 and 2
require no further code changes for these models.

1. **Normal-completion ordering: accepted for current models.** After observing
   leader exit, `main()` runs model finalization, success checks, output/log
   collection, final uploads and file-settle waits before `finish_task_exit()`
   attempts owned-process cleanup. A failed/expired final cleanup result does
   not override a selected successful completion code. This is unchanged:
   there are no model subprocess descendants continuing to write after the
   current models' process and its threads exit. Reassess this ordering before
   integrating a model with subprocess workers.
2. **Error-finalization policy: accepted for current models.**
   `recover_after_controller_error()` skips remaining upload finalization on
   failed or expired model cleanup. A POSIX group containing only zombie
   descendants can keep the group-presence check positive, but that descendant
   scenario does not arise from the current thread-only model workers. Review
   requires no change to this policy for OpenIFS or WRF. Reassess staged-evidence
   preservation if a future model creates subprocess descendants.
3. **Diagnostics acceptance: corrected.** The OpenIFS diagnostics caller now
   explicitly checks `TimedProcessResult::cleanup`. Failed or expired cleanup
   rejects the output even when leader execution succeeded and the output file
   was refreshed. Rejection logs the cleanup status, elapsed time and available
   failure detail, removes `trickle_data` (logging any removal failure), and does
   not report diagnostics success. Execution failure and stale output are also
   rejected. Diagnostics remain optional, so rejection does not abort the model
   task or change the existing attempted-run return convention.

Do not infer whole-process-set quiescence from leader exit, execution success,
or submission of a termination signal. These are separate observations.

## Limits and regression coverage

Cleanup cannot execute after an uncatchable controller kill or power loss.
POSIX descendants that deliberately leave the owned process group are outside
its guarantee. Recovery or automatic termination of orphans from an earlier
controller run is outside the scope of this change.

`TaskLifecycleTest` exercises the startup guard and shared final exit helper,
including running/suspended children, immediate executable overwrite after
cleanup, descendant cleanup after leader exit, and isolation from another launch.
Its deadline case uses a zero wait budget and checks that final exit does not
replace the stored result. It does not exercise the production nonzero-deadline
waiting loop through expiry. Linux descendant tests use a subreaper and explicit
reaping threads, so they do not cover persistent zombie-only group presence.

The guard and final helper are tested separately; the tests do not deliver real
BOINC IPC across process creation through `main()`. `LaunchProcessTest` and
`RunProcessWithTimeoutTest` cover the underlying model/helper seams.
`ExternalDiagnosticsTest` directly exercises the result-acceptance path with
successful execution and fresh output combined with failed/expired cleanup,
checking rejection, payload removal and diagnostic context. It also verifies
accepted cleanup outcomes, execution failure and stale-output handling. These
cases use supplied results rather than platform-dependent process failure
injection, so the acceptance regression is deterministic across platforms.

The corrective implementation passed the Linux CTest suite and a separate ASan
unit suite plus a short ASan functional run. In this environment, leak detection
was disabled and ordinary launch helpers were used because instrumented helper
startup conflicts with the launcher's unlimited-stack setting. Those results do
not establish native macOS or Windows behaviour for this change; the native
workflows still need execution, including platform-specific failure paths.

Keep this note, AGENTS.md, and the Doxygen comments aligned when changing
controller process ownership or BOINC exit semantics.
