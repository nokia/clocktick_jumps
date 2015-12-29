test_it: test_cj.c clocktick_jumps.c clocktick_jumps.h
	gcc -DUNIT_TESTING -g -Wall test_cj.c clocktick_jumps.c -o test_it -lcmocka

test: test_it
	./test_it

cj: clocktick_jumps.c clocktick_jumps.h
	gcc -O3 -Wall -g clocktick_jumps.c -o cj


