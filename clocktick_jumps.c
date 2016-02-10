#define _GNU_SOURCE
#include "/usr/include/sys/io.h"
#include <unistd.h>
#include <stdio.h>
#include <sched.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <sys/resource.h>
#include "clocktick_jumps.h"

#ifdef UNIT_TESTING
// Redefine main since unit tests have their own main
int example_main(int argc, char **argv);
int64_t mock_get_timevalue(bool);
#define main example_main
#endif  // UNIT_TESTING

static double percentiles[] = {0.50, 0.9, 0.99, 0.999, 0.9999, 0.99999, 0.999999};

char const *clock_name_r = "REALTIME";
char const *clock_name_t = "rdtsc";
char const *clock_name_p = "rdtscp";

char const *reporttype_name_p = "percentiles";
char const *reporttype_name_h = "highest";
char const *reporttype_name_c = "cumulative";

bool clock_units_in_ns(char const clocktype) {
        if (clocktype == 'r' || clocktype == 'm') {
                return true;
        } else if (clocktype == 't' || clocktype == 'p') {
                return false;
        } else {
                printf("Invalid clock type, exiting\n");
                exit(-1);
        }
}

double cyc2ns_multiplier = 0;
bool cyc2ns_multiplier_initialized = false;

struct command_line_arguments default_arguments = {
    .clocktype = 'r',\
    .clockname = &clock_name_r,\
    .cpu_pin = 1,\
    .reporttype = 'p',\
    .reportname = &reporttype_name_p,\
    .time_interval_ns = one_million,\
    .iterations = 1
};

int64_t s2ns(int64_t const secs) {
    return one_billion * secs;
}

int64_t ns2s(int64_t const ns) {
    return (int64_t) ((double) ns/ (double) one_billion);
}

static int int_comparison(const void *i, const void *j) {
    return (*(int64_t const*) i < *(int64_t const*) j) ? -1:1; 
}

//// Copied from Linux kernel
//static inline int64_t cpu_get_real_ticks(void)
//{
//    uint32_t low,high;
//    int64_t val;
//    __asm__ volatile("rdtsc" : "=a" (low), "=d" (high));
//    val = high;
//    val <<= 32;
//    val |= low;
//    return val;
//}
//
//// Copied from http://stackoverflow.com/questions/12631856/difference-between-rdtscp-rdtsc-memory-and-cpuid-rdtsc
////
//static inline int64_t get_rdtscp(void) {
//    int64_t tsc;
//    __asm__ volatile("rdtscp; "         // serializing read of tsc
//                     "shl $32,%%rdx;"   // shift higher 32 bits stored in rdx up
//                     "or %%rdx,%%rax"   // and or onto rax
//                     : "=a"(tsc)        // output to tsc variable
//                     :
//                     : "%rcx", "%rdx"); // rcx and rdx are clobbered
//    return tsc;
//}
//

int64_t cyc2ns(int64_t const cycles) {
        if (!cyc2ns_multiplier_initialized) {
            printf("Cycles to ns calculation not initialized\n");
            exit(-1);
        }
        return (int64_t) ((double) cycles * cyc2ns_multiplier); 
}

int64_t ns2cyc(int64_t const ns) {
        if (!cyc2ns_multiplier_initialized) {
            printf("Cycles to ns calculation not initialized\n");
            exit(-1);
        }
        return (int64_t) ((double) ns / cyc2ns_multiplier); 
}

void initialize_cyc2ns_multiplier(char const clocktype) {
        // Calculate one cyc in ns
        int64_t (*get_ticks)(void);
        if (clocktype == 'p') {
            get_ticks = &get_tsc_with_rdtscp;
        } else if (clocktype == 't') {
            get_ticks = &get_tsc_with_rdtsc;
        } else {
            printf("Unknown clock type in cyc2ns, exiting\n");
            exit(-1);
        }
        struct timespec tp;
        int64_t c1 = (*get_ticks)();
        clock_gettime(CLOCK_REALTIME, &tp);
        int64_t t1 = s2ns(tp.tv_sec) + tp.tv_nsec;
        const struct timespec req = {.tv_sec=1, .tv_nsec=0};
        nanosleep(&req, 0);
        int64_t c2 = (*get_ticks)();
        clock_gettime(CLOCK_REALTIME, &tp);
        int64_t t2 = s2ns(tp.tv_sec) + tp.tv_nsec;

        cyc2ns_multiplier = (double) (t2-t1)/(double) (c2-c1);
        cyc2ns_multiplier_initialized = true; 
}

struct timecounter {
    long long user_time;
    long long system_time;
    long long calendar_time;
};

static void get_timecounter(struct timecounter *tc) {    
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    tc->user_time     = usage.ru_utime.tv_sec * one_million + usage.ru_utime.tv_usec;
    tc->system_time   = usage.ru_stime.tv_sec * one_million + usage.ru_stime.tv_usec;

    struct timeval t;
    gettimeofday(&t, 0);
    tc->calendar_time = t.tv_sec * one_million + t.tv_usec;
}

static void print_timecounter_difference(char *text, struct timecounter *start, struct timecounter *end) {
    printf("%s %4.6f s user time, %4.6f s system time and %4.6f s calendar time\n", \
                    text, \
                    (end->user_time - start->user_time)/1e6, \
                    (end->system_time - start->system_time)/1e6, \
                    (end->calendar_time - start->calendar_time)/1e6);
}

static void print_ns_and_cyc_if_needed(int64_t ns, char const clocktype) {
    printf("%10ld", ns);
    if (clocktype == 't' || clocktype == 'p') {
            printf(" --%10ld", cyc2ns(ns));
    }
    printf(" ns\n");
}

int64_t get_timevalue(char const clocktype) {
    if (clocktype == 'r') {
        return get_clock_realtime();
    } else if (clocktype == 't') {
       return get_tsc_with_rdtsc();
    } else if (clocktype == 'p') {
        return get_tsc_with_rdtscp();
    } 

#ifdef UNIT_TESTING
    if (clocktype == 'm') {
        return mock_get_timevalue(false);
    }
#endif //UNIT_TESTING 

    printf("Unknown clocktype in get_timevalue, exiting");
    exit(-1);
}

int64_t get_timevalue_in_ns(char const clocktype) {
    if (clock_units_in_ns(clocktype)) {
            return get_timevalue(clocktype);
    } else {
            int64_t cycles = get_timevalue(clocktype);
            return cyc2ns(cycles);
    }
}

int64_t* run_percentile_test(uint64_t const number_of_iterations, char const clocktype) {
    // malloc is ok since we will overwrite the memory
    int64_t *results = malloc(number_of_iterations * sizeof(uint64_t));
    int64_t prev, next;
    prev = get_timevalue(clocktype);
    for (uint64_t i = 0; i < number_of_iterations; i++) {
        next = get_timevalue(clocktype);
        results[i] = next - prev;
        prev = next;
    }
    return results;
}

int64_t* run_highest_test(uint64_t const number_of_iterations, char const clocktype, uint const n) {
        int64_t prev, next, diff;
        int64_t *results = calloc(n+1, sizeof(int64_t));
        prev = get_timevalue(clocktype);
        for (uint64_t i = 0; i < number_of_iterations; i++) {
	        next = get_timevalue(clocktype); 
            diff = next - prev;
            prev = next;
            if (diff > results[0]) {  // results[0] is the smallest value of the 10
                results[0] = diff;
                qsort(results, n, sizeof(int64_t), &int_comparison);
            }
        }
        return results;
}

struct cumulative_test_results* run_cumulative_test_with_baseline(uint64_t const number_of_iterations, int64_t const baseline, char const clocktype) {
        int64_t prev, next;
        struct cumulative_test_results *results = calloc(number_of_iterations+1, sizeof(struct cumulative_test_results));
        // Misuse last value for baseline
        results[number_of_iterations].timestamp = baseline;
 
        prev = get_timevalue(clocktype);
        // Use first value for start time
        results[0].timestamp = prev;
        uint64_t index=1;
        while (index < number_of_iterations) {
            next = get_timevalue(clocktype);
            if (next-prev > baseline) {
                results[index].timestamp = prev;
                results[index].diff = (next-prev) - baseline;
                index++;
            }
            prev = next;
        }
        return results;
}

int64_t get_baseline_time(char const clocktype) {
    int64_t sum = 0;
    int64_t prev, next;
    prev = get_timevalue(clocktype);
    for (int i = 0; i < one_million; i++) {
        next = get_timevalue(clocktype);
        sum +=  next - prev;
        prev = next;
    }
    return (int64_t) ((double) sum/(double) one_million);
}

static int64_t get_baseline(char const clocktype) {
    return 2*get_baseline_time(clocktype);   
}

struct cumulative_test_results* 
run_cumulative_test(uint64_t const number_of_iterations, char const clocktype) {
    int64_t const baseline = get_baseline(clocktype);
    if (baseline == 0) {
        printf("Calculating baseline failed, exiting\n");
        exit(-1);
    }
    return run_cumulative_test_with_baseline(number_of_iterations, baseline, clocktype);
}


void find_highest_values(struct cumulative_test_results *results, uint64_t nbr_results, int64_t *highest_values, unsigned int const nbr_highest_values) {
    for (uint64_t i = 0; i < nbr_results; i++) {
        if (results[i].diff > highest_values[0]) {
            highest_values[0] = results[i].diff;
            qsort(highest_values, nbr_highest_values, sizeof(int64_t), &int_comparison);
        }
    };
    qsort(highest_values, nbr_highest_values, sizeof(int64_t), &int_comparison);
}


void find_highest_cumulative_values(struct cumulative_test_results *results, uint64_t nbr_results, int64_t *highest_values, unsigned int const nbr_highest_values, int64_t time_interval) {
        int64_t start = results[0].timestamp;
        int64_t sum = 0;
        for (uint64_t i = 0; i < nbr_results; i++) {
            sum += results[i].diff;
            if (results[i].timestamp >= start + time_interval) {
                start = results[i].timestamp;
                if (sum > highest_values[0]) {
                        highest_values[0] = sum;
                        qsort(highest_values, nbr_highest_values, sizeof(int64_t), &int_comparison);
                }
                sum = 0;
            }
        }
        qsort(highest_values, nbr_highest_values, sizeof(int64_t), &int_comparison);
}

void print_usage() {
    char *result;
    asprintf(&result, "Usage:");
    asprintf(&result, "%s \n-c clocktype: supported types are rdtsc, rdtscp, and REALTIME", result);
    asprintf(&result, "%s \n    (REALTIME refers to the clock type in POSIX function clock_gettime)", result);
    asprintf(&result, "%s \n    default is %s", result, *default_arguments.clockname);
    asprintf(&result, "%s \n-p c: pin the process to CPU number c", result);
    asprintf(&result, "%s \n-r reporttype: report percentiles, highest, or cumulative", result);
    asprintf(&result, "%s \n-t time_interval: how long to run each iteration (in ns) for cumulative test", result);
    asprintf(&result, "%s \n    default is %li", result, default_arguments.time_interval_ns);
    asprintf(&result, "%s \n-i iterations: how many iterations to run", result);
    printf("%s\n", result);
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
                cl->time_interval_ns = strtoll(optarg, &endptr, 10);
                if (errno != 0) {  
                    printf("Parsing command line failed for time interval (%s)\n", optarg);
                    return -1;
                }
                if (endptr == optarg) { 
                    printf("Parsing command line failed for time interval, no digits found\n");
                    return -1;
                }
                if (cl->time_interval_ns < 0 || cl->time_interval_ns == 0) {
                    printf("Invalid time interval %s\n", optarg);
                    return -1;
                }
            }
            break;
        case 'i':
            {
                char *endptr;
                errno = 0;
                int64_t i = strtoll(optarg, &endptr, 10);
                if (errno != 0 || *endptr != '\0') {
                    printf("Parsing command line failed for iterations (%s)\n", optarg);
                    return -1;
                };
                if (i == 0 || i < 0) {
                    printf("Invalid number of iterations %s\n", optarg);
                    return -1;
                } 
                cl->iterations = (uint64_t) i;
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
                      
    if (!clock_units_in_ns(cl.clocktype)) {
        initialize_cyc2ns_multiplier(cl.clocktype);
    }

    // Set a real-time priority
    int max_scheduling_priority = sched_get_priority_max(SCHED_FIFO);
    struct sched_param scheduling_parameter;
    scheduling_parameter.sched_priority = max_scheduling_priority;
    sched_setscheduler(this_process_id, SCHED_FIFO, &scheduling_parameter);  

    struct timecounter start_testrun, end_testrun;

    get_timecounter(&start_testrun);
    if (cl.reporttype == 'p') {
        uint64_t iterations = cl.iterations;
        char clocktype = cl.clocktype;
        int64_t *results = run_percentile_test(iterations, cl.clocktype);
        get_timecounter(&end_testrun);
   
        // Analyze results
        printf("\nFirst 10 values are:\n");
        for (int i=0; i<10; i++) {
            print_ns_and_cyc_if_needed(results[i], clocktype);  
        }

        qsort(results, iterations, sizeof(uint64_t), &int_comparison);

        printf("\nLargest 10 values are:\n");
        for (unsigned int i=0; i<10; i++) {
            int64_t c = results[iterations-1-i];
            print_ns_and_cyc_if_needed(c, clocktype);
        }   

        printf("\nPercentiles are:\n");
        int number_of_percentiles = sizeof(percentiles)/sizeof(double);
        for (int i=0; i<number_of_percentiles; i++) {
                int index_for_percentile = (int) (iterations * percentiles[i]); 
                printf("%f : ", percentiles[i]);
                print_ns_and_cyc_if_needed(results[index_for_percentile], cl.clocktype);
        }   
    } else if (cl.reporttype == 'h') {
        int64_t *results = run_highest_test(cl.iterations, cl.clocktype, 10);
        get_timecounter(&end_testrun);
        
        printf("Largest 10 values are:\n");
        for (int i=0; i<10; i++) {
            print_ns_and_cyc_if_needed(results[10-1-i], cl.clocktype);
        }   
    } else if (cl.reporttype == 'c') {
        struct cumulative_test_results *results = run_cumulative_test(cl.iterations, cl.clocktype);
        int64_t baseline = results[cl.iterations].timestamp;
        get_timecounter(&end_testrun);
        printf("Baseline for cumulative test is %" PRId64 " ns\n", baseline);
        printf("Multiplier for cycles to ns is %g\n", cyc2ns_multiplier); 
    
        // Timestamps may be in cyc, need to convert to ns 
        if (!clock_units_in_ns(cl.clocktype)) {
            for (uint64_t i = 0; i<cl.iterations; i++) {
                results[i].timestamp = cyc2ns(results[i].timestamp);
            }
        }
        
        enum  { nbr_highest_values = 10 };
        int64_t highest_values[nbr_highest_values] = {0};
        int64_t highest_cum_values[nbr_highest_values] = {0};

        int64_t timespan = results[cl.iterations-1].timestamp - results[0].timestamp;
        printf("Test span was  %" PRId64 " ns (% " PRId64 " us, %" PRId64 " ms)\n", timespan, timespan/1000, (int64_t) (timespan/one_million));
        printf("There are %" PRId64 " intervals of length %" PRId64 " ns (%" PRId64 " us, %" PRId64 " ms)\n", timespan/cl.time_interval_ns, cl.time_interval_ns, (int64_t) (cl.time_interval_ns/1000), (int64_t) (cl.time_interval_ns/one_million)); 

        find_highest_values(results, cl.iterations, highest_values, nbr_highest_values);
        printf("Largest %d individual values are\n", nbr_highest_values);
        for (unsigned int i=0; i< nbr_highest_values; i++) {
                printf("%16" PRId64 " ns (%8" PRId64" us)\n", highest_values[nbr_highest_values -1 -i], (int64_t) ((highest_values[nbr_highest_values-1-i]/1000)));
        }
        printf("\n");

        find_highest_cumulative_values(results, cl.iterations, highest_cum_values, nbr_highest_values, cl.time_interval_ns);
        printf("Largest %d cumulative values within %" PRIu64 " ns are:\n", nbr_highest_values, cl.time_interval_ns);
        for (unsigned int i = 0; i < nbr_highest_values; i++) {
                printf("%16" PRId64 " ns (%8" PRId64 " us)\n", highest_cum_values[nbr_highest_values - 1 - i], (int64_t) (highest_cum_values[nbr_highest_values-1-i]/1000));
        }

    } else {
        printf("Unknown report type, exiting\n");
        exit(-1);
    }
    print_timecounter_difference("Test run took ", &start_testrun, &end_testrun);
    return 0;
}
