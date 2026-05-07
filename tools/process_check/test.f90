program test_check

use cpdn_checkpid_mod

implicit none

integer :: is_running

call cpdn_checkpid( is_running )
write(6,*) 'is_running = ',is_running

if ( is_running == 0 ) then
    write(6,*) 'Control process not running: Abort!'
endif

end program
