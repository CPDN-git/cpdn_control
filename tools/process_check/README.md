
How to check the CPDN control process from within a fortran model
===================================================================

This small code is designed to help fortran based models detect
when the CPDN control process is not running. This can happen if 
there is a bug in the code, like a memory fault. If this happens
it often leaves the model running alone with no control, causing
corruption in the slot directory when the client tries to restart
the task (or another task).

Implementation
--------------

1. Place the two code files:

- cpdn_process_check.c
- cpdn_checkpid.f90

into a new code directory called 'cpdn' in the model src.

cpdn_checkpid.f90 is a fortran module that wraps the C code 
which does the process id check.

The fortran module code will attempt to read the CPDN progressfile
which is formatted as a fortran namelist which contains the PID
of the controller id.

2. Add the new source directory 'cpdn' to the build system.

3. Implement the call at the start of the model's main timestep loop.
Something like:

```
    subroutine time_step
    ....
    integer :: cpdn_is_running = 0

    .....
    call cpdn_checkpid( cpdn_is_running )
    if ( cpdn_is_running == 0 ) then
       print*,' ERROR: CPDN control process is NOT running. Aborting..'
       STOP 'ABORT'    ! or perhaps "call abort"
    endif

    do timestep = 1, nsteps
    ... 
    end do
    ....
    end subroutine
```

Error conditions
----------------

The call to cpdn_checkpid() will read the cpdn_progressfile.txt on each call.
This is to detect if the control_pid has changed.

is_running is set to zero and indicates an error if:

- cpdn_progressfile.txt is not found or not readable.
- an error occurred attempting to read an existing cpdn_progressfile.txt.
- the CPDN control process id changed from the last time the file was read.
- the process identified by the PID in the cpdn_progressfile.txt is not running.
