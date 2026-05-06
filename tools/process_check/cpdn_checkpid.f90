!
!  Fortran module & subroutine for use in CPDN models
!  to check periodically if the controller process is running.
!
!  Wrapper around C function to check process id.
!  Process id can be obtained from the progress file namelist.
!
!    Glenn Carver, CPDN, May/2026

module cpdn_checkpid_mod

    use, intrinsic :: iso_c_binding, only: c_int
    implicit none

    interface
        function cpdn_is_process_running(pid) bind(c, name="cpdn_is_process_running")
            import :: c_int
            integer(c_int), value :: pid
            integer(c_int) :: cpdn_is_process_running
        end function cpdn_is_process_running
    end interface

contains

    subroutine cpdn_checkpid( test_pid, is_running )

    integer,intent(in)  :: test_pid
    integer,intent(out) :: is_running   ! 1 if yes, 0 if no
    
    is_running = cpdn_is_process_running(int(test_pid, c_int))
    if ( is_running == 1 ) then
        print *, "Process ", test_pid, " is running."
    else
        print *, "Process ", test_pid, " is NOT running."
    end if

    return
    end subroutine

end module cpdn_checkpid_mod
