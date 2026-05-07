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

    private
    public :: cpdn_checkpid


    interface
        function cpdn_is_process_running(pid) bind(c, name="cpdn_is_process_running")
            import :: c_int
            integer(c_int), value :: pid
            integer(c_int) :: cpdn_is_process_running
        end function cpdn_is_process_running
    end interface

    integer, parameter :: ierr = -99

    !  The control code progress filename is fixed.
    character(len=*), parameter :: cpdn_progfile = 'cpdn_progressfile.txt'

    !  CPDN control code namelist format in progress file.
    integer :: control_pid, upload_file_number, last_step, last_upload, model_completed
    real    :: prior_acc_cpu_time
    namelist /cpdn/ control_pid, prior_acc_cpu_time, upload_file_number, last_step, last_upload, model_completed

contains

    subroutine cpdn_read_progfile

    integer :: iunit = 0
    integer :: stat = 0

    open(newunit=iunit, file=cpdn_progfile, status='old', action='read', iostat=stat )
    if ( stat /= 0 ) then
        control_pid = ierr
    else
        read( iunit, cpdn, iostat=stat )
        if ( stat /= 0 ) control_pid = ierr
    endif
    close(iunit)

    return
    end subroutine


    subroutine cpdn_checkpid( is_running )

    integer, intent(out) :: is_running   ! 1 if yes, 0 if no, error reading progfile, or control_pid changed
    integer, save        :: last_pid = -1     ! control_pid read previously
    
    !  Read progfile each time to check if process id has changed on us
    !  If it has, that's an error; our controller has been replaced.
    call cpdn_read_progfile
    if ( control_pid == ierr ) then
        ! cpdn_progressfile.txt can't be read
        is_running = 0
        return
    else if ( last_pid == -1 ) then
        last_pid = control_pid
    else if ( last_pid /= control_pid ) then
        ! controller pid has changed! Prob because our controller died and a new task started.
        ! this is an error and model needs to finish immediately.
        is_running = 0
        return
    endif

    !   We have a valid control_pid to check
    !   Call C wrapper to check process is running
    is_running = cpdn_is_process_running( int(control_pid, c_int) )

    return
    end subroutine

end module cpdn_checkpid_mod
