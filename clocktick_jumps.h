/*
 * Copyright 2020 Nokia
 * Licensed under the BSD 3-Clause License.
 * SPDX-License-Identifier: BSD-3-Clause
*/

#include <time.h>
#include <sys/time.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

extern char const *clock_name_r;
extern char const *clock_name_t;
extern char const *clock_name_p;

extern char const *reporttype_name_p;
extern char const *reporttype_name_h;
extern char const *reporttype_name_c;

#define one_million       1000000LL
#define hundred_million 100000000LL
#define one_billion    1000000000LL

void print_usage(void);

struct command_line_arguments {
    char clocktype;
    char const **clockname;
    int cpu_pin;
    char reporttype;
    char const **reportname;
    int64_t time_interval_ns;
    uint64_t iterations;
};

struct cumulative_test_results {
    int64_t timestamp;
    int64_t diff;
};

extern struct command_line_arguments default_arguments;

int parse_command_line(int, char **, struct command_line_arguments*);
int64_t* run_percentile_test(uint64_t const, char const);
int64_t* run_highest_test(uint64_t const, char const, unsigned int const);
struct cumulative_test_results* run_cumulative_test_with_baseline(uint64_t const, int64_t const, char const);
struct cumulative_test_results* run_cumulative_test(uint64_t const, char const);
void find_highest_values(struct cumulative_test_results *, uint64_t, int64_t *, unsigned int const);
void find_highest_cumulative_values(struct cumulative_test_results *, uint64_t, int64_t *, unsigned int const, int64_t); 

int64_t cyc2ns(int64_t const);
int64_t ns2cyc(int64_t const);
extern double cyc2ns_multiplier;
extern bool cyc2ns_multiplier_initialized;
void initialize_cyc2ns_multiplier(char const);
bool clock_units_in_ns(char const);
int64_t get_timevalue(char const);
int64_t get_timevalue_in_ns(char const);
int64_t get_baseline_time(char const);
int64_t s2ns(int64_t const);
int64_t ns2s(int64_t const);

static inline int64_t get_tsc_with_rdtscp(void) {
    register uint32_t high, low;
    __asm__ volatile (
        "cpuid;"
        "rdtscp;"
        "movl %%eax, %[low];"
        "movl %%edx, %[high];"
        : [high] "=r"(high), [low]  "=r"(low)
        :
        : "eax", "ebx", "ecx", "edx");
    return (((int64_t) high << 32) | low);
}

static inline int64_t get_tsc_aux_with_rdtscp(uint32_t *tsc_aux) {
    register uint32_t high, low, aux;
    __asm__ volatile (
        "cpuid;"
        "rdtscp;"
        "movl %%eax, %[low];"
        "movl %%edx, %[high];"
        "movl %%ecx, %[aux]"
        : [high] "=r"(high), [low]  "=r"(low), [aux] "=r"(aux)
        :
        : "eax", "ebx", "ecx", "edx");  
    *tsc_aux = aux; 
    return (((int64_t) high << 32) | low);
}

static inline int64_t get_tsc_with_rdtsc(void) {
    register uint32_t high, low;
    __asm__ volatile (
        "lfence;"
        "rdtsc;"
        "movl %%eax, %[low];"
        "movl %%edx, %[high]"
        : [high] "=r"(high), [low]  "=r"(low)
        :
        : "eax", "ebx", "ecx", "edx");
    return (int64_t) ( ( (int64_t)  high) << 32 | low);
}

static inline int64_t get_clock_realtime(void) {
	struct timespec tp;
    clock_gettime(CLOCK_REALTIME, &tp);
    return tp.tv_nsec + s2ns(tp.tv_sec);
}
