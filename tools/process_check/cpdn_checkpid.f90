!
!  Fortran module & subroutine for use in CPDN models
!  to check periodically if the controller process is running.
!
!    Glenn Carver, CPDN, May/2026
!
!  Wrapper around C function to check process id.
!  Process id can be obtained from the progress file namelist.
!
!   Usage
!   -----
!   Insert call to cpdn_checkpid inside the model's timestep loop
!   before the first executable statement.
!   The abort condition is if 'standalone' and 'is_running' are
!   both false.
!   e.g. 
!        subroutine do_timestep
!        use cpdn_checkpid_mod
!        ....
!        logical :: is_running, is_standalone
!        ...
!        do istep = 1, total_steps
!        call cpdn_checkpid(is_running,is_standalone)
!        if ( .not. is_standalone .and. .not. is_running ) then
!           STOP 'CPDN control process failed. Aborting...'
!        endif
!        end do
!        end subroutine
!
!   See test.f90 for an example test code and README.md file.
!
!   Note! Do not run this in a multi-threaded region of the code.
!        Not (yet) threadsafe.
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


    subroutine cpdn_checkpid( is_running, alone )

    logical, intent(out) :: is_running          ! .T. if yes, .F. if no, or error reading progfile, or control_pid changed
    logical, intent(out) :: alone               ! .T. if running standalone, .F. if under control by cpdn_controller.
    integer, save        :: last_pid = -1       ! control_pid read previously
    logical, save        :: lexist = .false.
    logical, save        :: lfirst = .true.
    logical, save        :: lstandalone = .true.    ! if no progfile found on first try, assume no cpdn_control process
    integer              :: iret

    !  Initially assume running alone.
    is_running = .false.
    alone = .true.

    !  Check progress file exists.
    !  If first time in and no progfile, assume we are running standalone.
    inquire( file=cpdn_progfile, exist=lexist )

    if (lfirst) then
        lfirst = .false.
        lstandalone = .not. lexist
    endif
    alone = lstandalone

    if ( lstandalone ) then
        return
    endif

    !  If there was initially a progfile but now there isn't, return error
    if ( .not.lstandalone .and. .not.lexist ) then
        is_running = .false.
        alone = .false.         ! .false. because we were not initially running standalone
        return
    endif

    !  Read progfile each time to check if process id has changed on us
    !  If it has, that's an error; our controller has been replaced.
    call cpdn_read_progfile

    if ( control_pid == ierr ) then
        is_running = .false.                    ! cpdn_progressfile.txt can't be read
        return
    else if ( last_pid == -1 ) then
        last_pid = control_pid
    else if ( last_pid /= control_pid ) then
        ! controller pid has changed! Probably our controller died and a new task started.
        ! this is an error and model needs to finish immediately.
        is_running = .false.
        return
    endif

    !   We have a valid control_pid to check
    !   Call C wrapper to check process is running
    iret = cpdn_is_process_running( int(control_pid, c_int) )
    is_running = iret .eq. 1

    return
    end subroutine

end module cpdn_checkpid_mod
