#define _GNU_SOURCE
#include "/usr/include/sys/io.h"
#include <unistd.h>
#include <stdio.h>
#include <sched.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <sys/resource.h>
#include "clocktick_jumps.h"

#ifdef UNIT_TESTING
// Redefine main since unit tests have their own main
int example_main(int argc, char **argv);
long int mock_get_timevalue(bool);
#define main example_main
#endif  // UNIT_TESTING

double percentiles[] = {0.50, 0.9, 0.99, 0.999, 0.9999, 0.99999, 0.999999};

char const *clock_name_r = "REALTIME";
char const *clock_name_t = "rdtsc";
char const *clock_name_p = "rdtscp";

char const *reporttype_name_p = "percentiles";
char const *reporttype_name_h = "highest";
char const *reporttype_name_c = "cumulative";

struct command_line_arguments default_arguments = {
    .clocktype = 'r',\
    .clockname = &clock_name_r,\
    .cpu_pin = 1,\
    .reporttype = 'p',\
    .reportname = &reporttype_name_p,\
    .time_interval_us = 1000000,\
    .iterations = 1
};

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

long int get_timevalue(int const clocktype) {
    if (clocktype == 'r') {
        return get_clock_realtime();
    } else if (clocktype == 't') {
       return cpu_get_real_ticks();
    } else if (clocktype == 'p') {
        return get_tsc();
    } 

#ifdef UNIT_TESTING
    if (clocktype == 'm') {
        return mock_get_timevalue(false);
    }
#endif //UNIT_TESTING 

    printf("Unknown clocktype in get_timevalue, exiting");
    exit(-1);
}

long int get_timevalue_in_ns(int const clocktype) {
    if (clocktype == 'r') {
        return get_clock_realtime();
    } else if (clocktype == 't') {
        long int ticks = get_timevalue('t');
        return cyc2ns(ticks, 't');
    } else if (clocktype == 'p') {
        long int ticks = get_timevalue('p');
        return cyc2ns(ticks, 'p');
    }

#ifdef UNIT_TESTING
    if (clocktype == 'm') {
        return get_timevalue('m');
    }
#endif //UNIT_TESTING    
 
    printf("Unknown clock type in get_timevalue_in_us\n");
    exit(-1);
}


long int* run_percentile_test(long int number_of_iterations, int const clocktype) {
    // malloc is ok since we will overwrite the memory
    long int *results = malloc(number_of_iterations * sizeof(long int));
    long int prev, next;
    prev = get_timevalue(clocktype);
    for (long int i = 0; i < number_of_iterations; i++) {
        next = get_timevalue(clocktype);
        results[i] = (long int) next - prev;
        prev = next;
    }
    return results;
}

long int* run_highest_test(long int number_of_iterations, int clocktype, int n) {
        long int prev, next, diff;
        long int *results = calloc(n, sizeof(long int));
        prev = get_timevalue(clocktype);
        for (long int i = 0; i < number_of_iterations; i++) {
	        next = get_timevalue(clocktype); 
            diff = next - prev;
            prev = next;
            if (diff > results[0]) {  // results[0] is the smallest value of the 10
                results[0] = diff;
                qsort(results, n, sizeof(long int), &int_comparison);
            }
        }
        return results;
}

long int* run_cumulative_test_with_baseline(long int number_of_iterations, long int time_interval, long int baseline, int const clocktype) {
        long int start, prev, next;
        long int *results = calloc(number_of_iterations, sizeof(long int));

        for (long int i = 0; i < number_of_iterations; i++) {
            start = get_timevalue_in_ns(clocktype);
            prev = start;
            while( (next=get_timevalue_in_ns(clocktype)) - start <= time_interval ) {
                long int diff = next - prev;
                prev = next;
                if (diff > baseline) {
                    results[i] += (diff-baseline);
                }
            }
        }
        return results;
}

long int* run_cumulative_test(long int number_of_iterations, long int time_interval, int const clocktype) {
    long int baseline;
    long int count=0;
    long int start = get_timevalue_in_ns(clocktype);
    while ( get_timevalue_in_ns(clocktype) - start < time_interval) {
            count++;
    }
    baseline = (long int) time_interval/(long double) count;
    return run_cumulative_test_with_baseline(number_of_iterations, time_interval, baseline, clocktype);
}

void print_usage() {
    char *result;
    asprintf(&result, "Usage:");
    asprintf(&result, "%s \n-c clocktype: supported types are rdtsc, rdtscp, and REALTIME", result);
    asprintf(&result, "%s \n    (REALTIME refers to the clock type in POSIX function clock_gettime)", result);
    asprintf(&result, "%s \n-p c: pin the process to CPU number c", result);
    asprintf(&result, "%s \n-r reporttype: report percentiles, highest, or cumulative", result);
    asprintf(&result, "%s \n-t time_interval: how long to run each iteration (in us) for cumulative test", result);
    asprintf(&result, "%s \n-i iterations: how many iterations to run", result);
    printf(result);
}

int parse_command_line(int argc, char **argv, struct command_line_arguments *cl) {
    int opt;
    #ifdef UNIT_TESTING
    optind=1; // setting optind to 1 makes this function idempotent
    #endif // UNIT_TESTING
    while ((opt = getopt(argc, argv, "c:p:r:t:i:")) != -1) {
        switch (opt) {
        case 'c':
            if (!strcmp(optarg, clock_name_r)) {
                cl->clocktype = 'r';
                cl->clockname =  &clock_name_r;
            } else if (!strcmp(optarg, clock_name_t)) {
                cl->clocktype = 't';
                cl->clockname = &clock_name_t;
            } else if (!strcmp(optarg, clock_name_p)) {
                cl->clocktype = 'p'; 
                cl->clockname = &clock_name_p;
            } else {
                printf("Unknown clock type %s", optarg);
                return(-1);
            }
            break;
        case 'p':
            cl->cpu_pin = atoi(optarg);
            if (cl->cpu_pin < 0 || cl->cpu_pin > 1024) {
                printf("CPU pin %s out of range", optarg);
                return -1;
            }
            break;
        case 'r':
            if (!strcmp(optarg, reporttype_name_p)) {
                cl->reporttype = 'p';
                cl->reportname = &reporttype_name_p;
            } else if (!strcmp(optarg, reporttype_name_h)) {
                cl->reporttype = 'h';
                cl->reportname = &reporttype_name_h;
            } else if (!strcmp(optarg, reporttype_name_c)) {
                cl->reporttype = 'c';
                cl->reportname = &reporttype_name_c;
            } else {
                printf("Unknown report type %s", optarg);
                return -1;
            }
            break;
        case 't':
            {
                char *endptr;
                errno = 0;
                cl->time_interval_us = strtol(optarg, &endptr, 10);
                if (errno == !0) {  
                    printf("Parsing command line failed for time interval (%s)", optarg);
                    return -1;
                }
                if (endptr == optarg) { 
                    printf("Parsing command line failed for time interval, no digits found");
                    return -1;
                }
                if (cl->time_interval_us < 0 || cl->time_interval_us == 0) {
                    printf("Invalid time interval %s", optarg);
                    return -1;
                }
            }
            break;
        case 'i':
            cl->iterations = atol(optarg);
            if (cl->iterations < 0 || cl->iterations == 0) {
                printf("Invalid number of iterations %s", optarg);
                return -1;
            } 
            break;
        default: /* '?' */
            print_usage();
            return -1;
        }
    }
    return 0; // everything cool
}

int main(int argc, char **argv) {  
    struct command_line_arguments cl = default_arguments;
    int r = parse_command_line(argc, argv, &cl);
    if (r < 0) {
            printf("Parsing command line arguments failed\n");
            exit(EXIT_FAILURE);
    }

    printf("\nRunning test %s with clock %s for %li iterations while pinning to processor %i\n", \
        *cl.reportname, *cl.clockname, cl.iterations, cl.cpu_pin);
    
    if (cl.clocktype == 'r') {
        struct timespec res;
        clock_getres(CLOCK_REALTIME, &res);
        printf("Clock resolution for CLOCK_REALTIME is %li nanoseconds\n", res.tv_nsec);
    } else if (cl.clocktype == 't' || cl.clocktype == 'p') {
        printf("tsc values are in units of clock ticks\n");
    }

    // Pin process to CPU cpu_pin
    const int this_process_id = 0;    
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(cl.cpu_pin, &set);
    sched_setaffinity(this_process_id, sizeof(cpu_set_t), &set);
                      
    // Set a real-time priority
    int max_scheduling_priority = sched_get_priority_max(SCHED_FIFO);
    struct sched_param scheduling_parameter;
    scheduling_parameter.sched_priority = max_scheduling_priority;
    sched_setscheduler(this_process_id, SCHED_FIFO, &scheduling_parameter);  

    assert(sizeof(long int) >= sizeof(int64_t));
    
    struct timecounter start, end;

    get_timecounter(&start);
    if (cl.reporttype == 'p') {
        long int iterations = cl.iterations;
        char clocktype = cl.clocktype;
        long int *results = run_percentile_test(iterations, cl.clocktype);
        get_timecounter(&end);
   
        // Analyze results
        printf("\nFirst 10 values are:\n");
        for (int i=0; i<10; i++) {
            print_ns_and_cyc_if_needed(results[i], clocktype);  
        }

        qsort(results, iterations, sizeof(long int), &int_comparison);

        printf("\nLargest 10 values are:\n");
        for (int i=0; i<10; i++) {
            long int c = results[iterations-1-i];
            print_ns_and_cyc_if_needed(c, clocktype);
        }   

        printf("\nPercentiles are:\n");
        int number_of_percentiles = sizeof(percentiles)/sizeof(double);
        for (int i=0; i<number_of_percentiles; i++) {
                int index_for_percentile = iterations * percentiles[i]; 
                printf("%f : ", percentiles[i]);
                print_ns_and_cyc_if_needed(results[index_for_percentile], cl.clocktype);
        }   
    } else if (cl.reporttype == 'h') {
        long int *results = run_highest_test(cl.iterations, cl.clocktype, 10);
        get_timecounter(&end);
        
        printf("Largest 10 values are:\n");
        for (int i=0; i<10; i++) {
            print_ns_and_cyc_if_needed(results[10-1-i], cl.clocktype);
        }   
    }
    print_timecounter_difference("Test run took ", &start, &end);
    return 0;
}
