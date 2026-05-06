program test_check

use cpdn_checkpid_mod

implicit none

integer :: pid
integer :: is_running

write(6,*) 'Enter PID to check: '
read(5,*)  pid

call cpdn_checkpid( pid, is_running )

write(6,*) 'is_running = ',is_running

end program
