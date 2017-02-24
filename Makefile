#in makefiles, a hash indicates a comment instead
all:snap_cntl
snap_cntl:snap_controller.c
	gcc -Wall -pthread -o snap_cntl snap_controller.c -lpigpio -lrt -lev
