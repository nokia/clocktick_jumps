# Introduction

This program measures scheduling latencies at a low level, with as little involvement from the operating system as possible. The main use case is to run the program on a virtual machine to measure the latencies that the hardware (including BIOS), host operating system and virtualization layer cause.

It uses the [Time Stamp Counter on x86](https://en.wikipedia.org/wiki/Time_Stamp_Counter) -- tsc --  that is a hardware device in the i32 architecture. There is a special instruction to read the tsc counter (rdtsc) that is by default unprivileged, unlike reading almost any other register values. The kvm hypervisor and the newer Intel processors implement the rdtsc instruction in a virtual machine so that it reads the real hardware value. This operation also has very low overhead.

Another method to read time values in Linux in a virtual machine is to use the kvmclock. Linux allows one to choose from different clock sources -- by default, it has an algorithm to choose the best one. The kvm hypervisor implements the kvmclock clocksource which uses the host operating system provided time, which in turn could use tsc. The benefit is that this mechanism gives time differences in nanoseconds, but the overheads are higher.

## What is wrong with cyclictest?

[Cyclictest](https://rt.wiki.kernel.org/index.php/Cyclictest) works by setting a periodic time on Linux and then checking how much time has passed when the timer fires. It is commonly used to measure interrupt latencies. If the guest operating system has interrupts disabled, then the interrupt will not fire in time, which is one of the reasons why the timer interrupt may be delayed.

Even when cyclictest is running on an isolated core, its results will include the overheads of calling the interrupt handler and reading the time values using system calls. In testing with clockjumps, using the system calls has a much higher overhead than reading the tsc counter directly.


# Installation 

See the makefile:

- make cj will compile the program with gcc
- make cj2 will compile the program with clang 
- make cj_static will make a static version of the program which can be run on almost any Linux system
- make test will run unit tests
- make cj.asm will generate the assembly language version for inspection

The script run_measurements will run the tests with different options and report system configuration.
The script run_measurements_long runs some longer tests.


# Operation

The test application is written in C and it runs in a tight loop and reads a clock value in every iteration. The clock can be the POSIX standard get_clocktime function or the time-stamp counter (tsc) in the processor. There is a shell script that runs the C program and checks some basic facts, like that the tsc counter is monotonous.
  
The theory behind this is that if the clock increases monotonously and does not do sudden jumps, then any jumps must come from the program not being allowed to run. There are several ways this can happen:

- System Management Interrupts (SMIs): These override any other activies and there is no way to avoid them. They are part of the Intel architecture. The number and duration of SMIs depend on the BIOS of the server.

- Other processes running on the same core: These are avoided in the guest system by isolating the CPU cores where the application runs, and pinning the test program to an isolated core. Isolation means that the Linux scheduler does not run any other tasks on the same core.

- Scheduler interrupts. This is minimized with the guest kernel boot option "nohz_full"

- Interprocessor interrupts. These should be few with modern Linux kernels

- Memory access failures. The percentile test will allocate a large array and write the results there. These do not happen in other tests, so it makes sense to run the different tests and compare results.


# Report types

The main loop is always the same: it measures how much the clock jumps forward in a loop. These results are reported in different ways:

- percentile: This stores all values in an array of size _iterations_. It then sorts the results and displays the first 10 values in case there is something interesting in the beginning. After sorting, different percentiles like 50%, 90% etc are reported. Fifty per cent is the mean value of measurements and means that half of values are smaller and half are larger.

- highest: This runs the test for _iterations_ and takes the 10 highest values from those.

- cumulative: The cumulative is meant to tell if the clock jumps tend to cluster together. It calculates a baseline value about what would be an acceptable clock jump - currently, it calculates the average jump for a hundred million iterations and multiplies it by 2. It repeats the loop until there are _iterations_ jumps bigger than baseline and stores each jump and a timestamp.  The timestamps are converted to ns. Then, the program adds up all extra jumps (jump- baseline) for the first time_value nanoseconds, the second time_value nanoseconds, etc. The highest cumulative sums are then reported. 

In the cumulative case, it would be more natural to repeat the loop until a time value. However, the straightforward implementation would check time in each iteration, but the compilers did not like this approach. 


# Clock types

One clock type is the POSIX-defined real-time clock, which uses a system call. It is the most reliable and gives the results in nanoseconds. The other two alternatives read the time-stamp counter (tsc) in the CPU. They use different instructions mainly for comparison reasons:

- Since Pentium, all Intel CPUs support the rdtsc instruction for reading the tsc value

- Since Nehalem, Intel processors also support the rdtscp instruction which reads both the tsc value and also a value TSC_AUX that can be initiated to the value of the logical CPU. Rdtscp reads these as an atomic operation.

In early processors, tsc counted the CPU execution cycles, so when the CPU frequency changed, the tsc counter increased at a different speed. The tsc counter would stop when the CPU stopped. In newer processors, the tsc clock is monotonously increasing, does not stop and increases at a constant rate. The run_test script checks that the "constant tsc" and "non-stop tsc" features are enabled.

The tsc value can in principle be different on different CPUs on an SMP system. However, since the tsc counter is started when the CPU is booted, and different CPU packages uses a common clock source, it is not likely that it will get out of sync. For instance, Linux only checks that the tsc values seem to be consistent at boot time, and if they seem ok, it will use tsc as a clocksource.  The current clocksource is also reported in run_tests. 

For references, see

- [http://oliveryang.net/2015/09/pitfalls-of-TSC-usage/]
- [https://software.intel.com/en-us/forums/software-tuning-performance-optimization-platform-monitoring/topic/388964]


The main loop is the following:

```
        lfence
        next = (rdtsc value)        
        diff = next - prev
        prev = next
```

or

```
        cpuid
        next = (rdtscp value)
        diff = next - prev
        prev = next
```

The diff is then used for various purposes. The commands ffence and cpuid are to serialize the reading of tsc counter.

In C source, for the cumulative test, the code is as follows:

```
        while (index < number_of_iterations) {
            next = get_timevalue(clocktype);
            if (next-prev > baseline) {
                results[index].timestamp = prev;
                results[index].diff = (next-prev) - baseline;
                index++;
            }
            prev = next;
        }
```
 
A part of the assembly output is below. Although there is a test for the clocktype in the loop,
with optimization the code becomes as below. 

```
.L157:
// 88 "clocktick_jumps.h" 1
	lfence;rdtsc;movl %eax, %esi;movl %edx, %edi	# low, high
// 0 "" 2
	.loc 1 96 0
	
    movq	%rdi, %rax	# high, D.6018
	movl	%esi, %esi	# low, D.6018
	salq	$32, %rax	#, D.6018
	orq	%rsi, %rax	# D.6018, prev
	movq	%rax, %rdx	# prev, D.6018
	subq	%r10, %rdx	# prev, D.6018
	movq	%rax, %r10	# prev, prev
	addq	%rdx, %r9	# D.6018, sum
	subl	$1, %r8d	#, D.6023
	jne	.L157	#,
```


# Caveats

There is some variance in the results. For instance, the unit tests check that two runs are within 10 per cent of each other. Even this is not enough in some cases, so all results have some errors.  Also, the loop for rdtsc and rdtscp tests is different on purpose, and the rdtscp loop takes longer.  So the small values are not interesting.

The results are also a little different for different compilers.

:penguin:

