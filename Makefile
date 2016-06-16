test_it: test_cj.c clocktick_jumps.c clocktick_jumps.h
	gcc -O3 -DUNIT_TESTING -g -Wall test_cj.c clocktick_jumps.c -o test_it -lcmocka

test: test_it
	./test_it

cj: clocktick_jumps.c clocktick_jumps.h
	gcc -O3 -Wall -g clocktick_jumps.c -o cj

cj_static: clocktick_jumps.c clocktick_jumps.h
	gcc -static -static-libgcc -O3 -Wall -g -lc clocktick_jumps.c -o cj_static

cj2: clocktick_jumps.c clocktick_jumps.h
	clang -g -Weverything -fdiagnostics-format=vi clocktick_jumps.c -o cj2

cj.asm: clocktick_jumps.c
	gcc -O3 -g -c -Wa,-a,-ad -fverbose-asm clocktick_jumps.c > cj.asm


