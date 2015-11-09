#define _GNU_SOURCE
#include "/usr/include/sys/io.h"
#include <unistd.h>
#include <stdio.h>
#include <sched.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>
#include <sys/resource.h>
#include <sys/time.h>

double percentiles[] = {0.50, 0.9, 0.99, 0.999, 0.9999, 0.99999, 0.999999};

const long int one_million = 1000000;

long int s2ns(long int secs) {
    return (long int) 1e9 * secs;
}

float ns2s(long int ns) {
    return ns/1e9;
}

int int_comparison(const void *i, const void *j) {
    return *(long int*)i - *(long int*)j;
}

// Copied from Linux kernel
static inline int64_t cpu_get_real_ticks(void)
{
    uint32_t low,high;
    int64_t val;
    __asm__ volatile("rdtsc" : "=a" (low), "=d" (high));
    val = high;
    val <<= 32;
    val |= low;
    return val;
}

// Copied from http://stackoverflow.com/questions/12631856/difference-between-rdtscp-rdtsc-memory-and-cpuid-rdtsc
//
static inline int64_t get_rdtscp(void) {
    int64_t tsc;
    __asm__ volatile("rdtscp; "         // serializing read of tsc
                     "shl $32,%%rdx;"   // shift higher 32 bits stored in rdx up
                     "or %%rdx,%%rax"   // and or onto rax
                     : "=a"(tsc)        // output to tsc variable
                     :
                     : "%rcx", "%rdx"); // rcx and rdx are clobbered
    return tsc;
}

long int cyc2ns(long int ns, int clocktype) {
    static double m = 0;
    if (m == 0) {
        int64_t (*get_ticks)(void);
        if (clocktype == 'p') {
            get_ticks = &get_rdtscp;
        } else if (clocktype == 't') {
            get_ticks = &cpu_get_real_ticks;
        } else {
            printf("Unknown clock type in cyc2ns, exiting\n");
            exit(-1);
        }
        struct timespec tp;
        long int c1 = (*get_ticks)();
        clock_gettime(CLOCK_REALTIME, &tp);
        long int t1 = s2ns(tp.tv_sec) + tp.tv_nsec;
        sleep(1);
        long int c2 = (*get_ticks)();
        clock_gettime(CLOCK_REALTIME, &tp);
        long int t2 = s2ns(tp.tv_sec) + tp.tv_nsec;

        m = (double) (t2-t1)/(double) (c2-c1);
    }
    return ns * m;
}

struct timecounter {
    long long user_time;
    long long system_time;
    long long calendar_time;
};

void get_timecounter(struct timecounter *tc) {    
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    tc->user_time     = usage.ru_utime.tv_sec * one_million + usage.ru_utime.tv_usec;
    tc->system_time   = usage.ru_stime.tv_sec * one_million + usage.ru_stime.tv_usec;

    struct timeval t;
    gettimeofday(&t, 0);
    tc->calendar_time = t.tv_sec * one_million + t.tv_usec;
}

void print_timecounter_difference(char *text, struct timecounter *start, struct timecounter *end) {
    printf("%s %4.6f s user time, %4.6f s system time and %4.6f s calendar time\n", \
                    text, \
                    (end->user_time - start->user_time)/1e6, \
                    (end->system_time - start->system_time)/1e6, \
                    (end->calendar_time - start->calendar_time)/1e6);
}

void print_ns_and_cyc_if_needed(long int ns, int clocktype) {
    printf("%10ld", ns);
    if (clocktype == 't' || clocktype == 'p') {
            printf(" --%10ld", cyc2ns(ns, clocktype));
    }
    printf(" ns\n");
}

long int* run_histogram_test(long int number_of_iterations, int clocktype) {
        long int *results = malloc(number_of_iterations * sizeof(long int));
        if (clocktype == 'r') {
            struct timespec tp, tp_previous;
            clock_gettime(CLOCK_REALTIME, &tp_previous);
            for (long int i = 0; i < number_of_iterations; i++) {
	            clock_gettime(CLOCK_REALTIME, &tp);
                results[i] = (long int) tp.tv_nsec - tp_previous.tv_nsec;
                tp_previous.tv_nsec = tp.tv_nsec;
            }
        } else if (clocktype == 't') {
           int64_t prev = cpu_get_real_ticks();
            for (long int i = 0; i < number_of_iterations; i++) {
                int64_t cur = cpu_get_real_ticks();
	            results[i] = (long int) cur - prev;
                prev = cur;
            }
        } else if (clocktype == 'p') {
           int64_t prev = get_rdtscp();
            for (long int i = 0; i < number_of_iterations; i++) {
                int64_t cur = get_rdtscp();
	            results[i] = (long int) cur - prev;
                prev = cur;
            }
        } else {
            printf("clocktype wrong in run_histogram_tests, quitting\n");
            exit(-1);
        }
        return results;
}

long int* run_long_test(long int number_of_iterations, int clocktype, int n) {
        long int diff;
        long int *results = malloc(n * sizeof(long int));
        if (clocktype == 'r') {
            struct timespec tp, tp_previous;
            clock_gettime(CLOCK_REALTIME, &tp_previous);
            for (long int i = 0; i < number_of_iterations; i++) {
	            clock_gettime(CLOCK_REALTIME, &tp);
                diff = (long int) tp.tv_nsec - tp_previous.tv_nsec;
                tp_previous.tv_nsec = tp.tv_nsec;
                if (diff > results[0]) {  // results[0] is the smallest value of the 10
                        results[0] = diff;
                        qsort(results, n, sizeof(long int), &int_comparison);
                }
            }
        } else if (clocktype == 't') {
            int64_t prev = cpu_get_real_ticks();
            for (long int i = 0; i < number_of_iterations; i++) {
                int64_t cur = cpu_get_real_ticks();
	            diff = (long int) cur - prev;
                prev = cur;
                if (diff > results[0]) {
                        results[0] = diff;
                        qsort(results, n, sizeof(long int), &int_comparison);
                }
            }
         } else if (clocktype == 'p') {
            int64_t prev = get_rdtscp();
            for (long int i = 0; i < number_of_iterations; i++) {
                int64_t cur = get_rdtscp();
	            diff = (long int) cur - prev;
                prev = cur;
                if (diff > results[0]) {
                        results[0] = diff;
                        qsort(results, n, sizeof(long int), &int_comparison);
                }
            }
        } else {
            printf("clocktype wrong in run_long_test, quitting\n");
            exit(-1);
        }
        return results;
}



int main(int argc, char **argv) {
    // Usage:
    // -c clocktype: rdtsc, rdtscp, REALTIME etc
    // -p c: pin process to cpu c
    // -i n: number of iterations
    // -h: histogram - keep all measurement values and calculate percentages
    // -l: run a long test, keep top 10 measurements
    
    int opt;
    int cpu_used = 1;
    long number_of_iterations = 10000;

    int measurement_type = 'h';
    char *measurement_type_name = "histogram";

    int clocktype = 'r';
    char *clock_name_r = "REALTIME";
    char *clock_name_t = "rdtsc";
    char *clock_name_p = "rdtscp";
    char *current_clock_name = clock_name_r;

    while ((opt = getopt(argc, argv, "c:p:i:hl")) != -1) {
        switch (opt) {
        case 'c':
            current_clock_name = optarg;
            if (!strcmp(optarg, clock_name_r)) {
                clocktype = 'r';
            } else if (!strcmp(optarg, clock_name_t)) {
                clocktype = 't';
            } else if (!strcmp(optarg, clock_name_p)) {
                clocktype = 'p'; 
            } else {
                printf("Unknown clock type %s", optarg);
                exit(-1);
            }
            break;
        case 'p':
            cpu_used = atoi(optarg);
            break;
        case 'i':
            number_of_iterations = atoi(optarg);
            break;
        case 'h':
            measurement_type = 'h';
            measurement_type_name = "histogram";
            break;
        case 'l':
            measurement_type = 'l';
            measurement_type_name = "long run";
            break;
        default: /* '?' */
            fprintf(stderr, "Usage: %s [-t nsecs] [-n] name\n", argv[0]);
            exit(EXIT_FAILURE);
        }
    }
    printf("\nRunning test %s with clock %s for %li iterations while pinning to processor %i\n", \
                    measurement_type_name, \
                    current_clock_name, \
                    number_of_iterations, \
                    cpu_used);
    
    if (clocktype == 'r') {
        struct timespec res;
        clock_getres(CLOCK_REALTIME, &res);
        printf("Clock resolution for CLOCK_REALTIME is %li nanoseconds\n", res.tv_nsec);
    } else if (clocktype == 't' || clocktype == 'p') {
        printf("tsc values are in units of clock ticks\n");
    }

    // Pin process to CPU cpu_used
    const int this_process_id = 0;    
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(cpu_used, &set);
    sched_setaffinity(this_process_id, sizeof(cpu_set_t), &set);
                      
    // Set a real-time priority
    int max_scheduling_priority = sched_get_priority_max(SCHED_FIFO);
    struct sched_param scheduling_parameter;
    scheduling_parameter.sched_priority = max_scheduling_priority;
    sched_setscheduler(this_process_id, SCHED_FIFO, &scheduling_parameter);  

    assert(sizeof(long int) >= sizeof(int64_t));
    
    struct timecounter start, end;

    get_timecounter(&start);
    if (measurement_type == 'h') {
        long int *results = run_histogram_test(number_of_iterations, clocktype);
        get_timecounter(&end);
   
        // Analyze results
        printf("\nFirst 10 values are:\n");
        for (int i=0; i<10; i++) {
            print_ns_and_cyc_if_needed(results[i], clocktype);  
        }

        qsort(results, number_of_iterations, sizeof(long int), &int_comparison);

        printf("\nLargest 10 values are:\n");
        for (int i=0; i<10; i++) {
            long int c = results[number_of_iterations-1-i];
            print_ns_and_cyc_if_needed(c, clocktype);
        }   

        printf("\nPercentiles are:\n");
        int number_of_percentiles = sizeof(percentiles)/sizeof(double);
        for (int i=0; i<number_of_percentiles; i++) {
                int index_for_percentile = number_of_iterations * percentiles[i]; 
                printf("%f : ", percentiles[i]);
                print_ns_and_cyc_if_needed(results[index_for_percentile], clocktype);
        }   
    } else if (measurement_type == 'l') {
        long int *results = run_long_test(number_of_iterations, clocktype, 10);
        get_timecounter(&end);
        
        printf("Largest 10 values are:\n");
        for (int i=0; i<10; i++) {
            print_ns_and_cyc_if_needed(results[10-1-i], clocktype);
        }   
    }
    print_timecounter_difference("Test run took ", &start, &end);
    return 0;
}
