program test_check
!
!  Small test program to illustrate use of cpdn_checkpid.
!
!    Glenn Carver, CPDN, May/2026

use cpdn_checkpid_mod

implicit none

logical :: is_running
logical :: is_standalone
integer :: i, imax

i = 1
imax = 10

do
    call sleep(3)

    call cpdn_checkpid( is_running, is_standalone )
    print*, 'is_running = ',is_running
    print*, 'is_standalone = ',is_standalone

    if ( is_standalone ) then
        print*,' Model running standalone with no cpdn process.'
    else if ( .not. is_standalone .and. is_running ) then
        print*, 'control proc ok'
    else if ( .not. is_standalone .and. .not. is_running ) then
        print*, 'Progfile found but control process not running: Abort!'
        stop 'program aborted'
    endif

    i = i + 1
    if ( i > imax ) stop 'finished'

end do

end program
