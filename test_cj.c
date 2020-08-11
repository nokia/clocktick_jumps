/*
 * Copyright 2020 Nokia
 * Licensed under the BSD 3-Clause License.
 * SPDX-License-Identifier: BSD-3-Clause
*/

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <cmocka.h>
#include <wordexp.h>

#include "clocktick_jumps.h"

static void null_test_success(void **state) {
    (void) state; 
}

static void test_wordexp(void **state) {
    wordexp_t p;
    assert_return_code(wordexp("a b cd 1234 -1234 -p 3", &p, 0), 0);
    assert_int_equal(p.we_wordc, 7);
    char** argv = p.we_wordv;
    assert_string_equal(argv[0], "a");
    assert_string_equal(argv[1], "b");
    assert_string_equal(argv[2], "cd");
    assert_string_equal(argv[3], "1234");
    assert_string_equal(argv[4], "-1234");
    assert_string_equal(argv[5], "-p");
    assert_string_equal(argv[6], "3");
}

static void test_parse_command_line_defaults(void **state) {   
    struct command_line_arguments cl = default_arguments;  
    wordexp_t p;
    assert_return_code(wordexp("cj", &p, 0), 0);
    assert_return_code(parse_command_line(p.we_wordc, p.we_wordv, &cl), 0);
    assert_int_equal(cl.clocktype, 'r');
    assert_string_equal(*cl.clockname, "REALTIME");
    assert_int_equal(cl.reporttype, 'p');
    assert_int_equal(cl.time_interval_ns, one_million);
    assert_int_equal(cl.cpu_pin, 1);
}

static void test_parse_command_line_clocktype_rdtsc(void **state) {   
    struct command_line_arguments cl = default_arguments;  
    wordexp_t p;
    assert_return_code(wordexp("cj -c rdtsc", &p, 0), 0);
    assert_return_code(parse_command_line(p.we_wordc, p.we_wordv, &cl), 0);
    assert_int_equal(cl.clocktype, 't');
    assert_string_equal(*cl.clockname, "rdtsc");
}

static void test_parse_command_line_clocktype_rdtscp(void **state) {   
    struct command_line_arguments cl = default_arguments;  
    wordexp_t p;
    assert_return_code(wordexp("cj -c rdtscp", &p, 0), 0);
    assert_return_code(parse_command_line(p.we_wordc, p.we_wordv, &cl), 0);
    assert_int_equal(cl.clocktype, 'p');
    assert_string_equal(*cl.clockname, "rdtscp");
}

static void test_parse_command_line_time_interval(void **state) {   
    struct command_line_arguments cl = default_arguments;  
    wordexp_t p;
    assert_return_code(wordexp("cj -t 12345", &p, 0), 0);
    assert_return_code(parse_command_line(p.we_wordc, p.we_wordv, &cl), 0);
    assert_int_equal(cl.time_interval_ns, 12345);
}

static void test_parse_command_line_iterations(void **state) {   
    struct command_line_arguments cl = default_arguments;  
    wordexp_t p;
    assert_return_code(wordexp("cj -i 12345", &p, 0), 0);
    assert_return_code(parse_command_line(p.we_wordc, p.we_wordv, &cl), 0);
    assert_int_equal(cl.iterations, 12345);

    assert_return_code(wordexp("cj -r cumulative -i 12345 -c rdtscp -t 123456", &p, 0), 0);
    assert_return_code(parse_command_line(p.we_wordc, p.we_wordv, &cl), 0);
    assert_int_equal(cl.iterations, 12345);
    assert_int_equal(cl.time_interval_ns, 123456);
    assert_int_equal(cl.reporttype, 'c');
    assert_int_equal(cl.clocktype, 'p');

    assert_return_code(wordexp("./cj -c rdtscp -p 2 -i 30 -t 100000000 -r cumulative", &p, 0), 0);
    assert_return_code(parse_command_line(p.we_wordc, p.we_wordv, &cl), 0);
    assert_int_equal(cl.iterations, 30);
    assert_int_equal(cl.cpu_pin, 2);
    assert_int_equal(cl.time_interval_ns, 100000000);
    assert_int_equal(cl.reporttype, 'c');
    assert_int_equal(cl.clocktype, 'p');
}

static void test_parse_command_line_cpu_pin(void **state) {   
    struct command_line_arguments cl = default_arguments;  
    wordexp_t p;
    assert_return_code(wordexp("cj -p 3", &p, 0), 0);
    assert_return_code(parse_command_line(p.we_wordc, p.we_wordv, &cl), 0);
    assert_int_equal(cl.cpu_pin, 3);
}

static void test_parse_command_line_nonsense(void **state) {   
    struct command_line_arguments cl = default_arguments;  
    wordexp_t p;
 
    assert_return_code(wordexp("cj -x 3", &p, 0), 0);
    assert_int_equal(parse_command_line(p.we_wordc, p.we_wordv, &cl), -1);

    assert_return_code(wordexp("cj -r histogram", &p, 0), 0);
    assert_int_equal(parse_command_line(p.we_wordc, p.we_wordv, &cl), -1);

    assert_return_code(wordexp("cj -i -13", &p, 0), 0);
    assert_int_equal(parse_command_line(p.we_wordc, p.we_wordv, &cl), -1);
}

// Sanity check for get_tsc
static void test_get_tsc(void **state) {
    int64_t start, stop;
    struct timespec const t = {.tv_nsec = one_million}; 
    struct timespec rem;
    start = get_tsc_with_rdtsc();
    assert_return_code(nanosleep(&t, &rem), 0);
    stop = get_tsc_with_rdtsc();
    assert_in_range(cyc2ns(stop - start), 0.9 * one_million, 1.2 * one_million);
}

// Sanity check for get_tscp
static void test_get_tscp(void **state) {
    int64_t start, stop;
    struct timespec const t = {.tv_nsec = one_million}; 
    start = get_tsc_with_rdtscp();
    assert_return_code(nanosleep(&t, 0), 0);
    stop = get_tsc_with_rdtscp();
    assert_in_range(cyc2ns(stop - start), 0.9 * one_million, 1.2 * one_million);
}

// Same for get_clock_readtime
static void test_get_clock_realtime(void **state) {
    int64_t start, stop;
    struct timespec const t = {.tv_nsec = one_million}; 
    struct timespec rem;
    start = get_clock_realtime();
    nanosleep(&t, &rem);
    stop = get_clock_realtime();
    assert_in_range(stop - start, 0.9*one_million, 1.2*one_million);
}

int64_t mock_get_timevalue(bool restart) {
    int64_t v[] = {0, 1, 2, 4, 8, 16, 32, 64};
    int n = sizeof(v)/sizeof(uint64_t);
    int64_t v_last = v[n-1];
    static int index=-1;
    if (restart) {
           index = -1;
    } 
    index++;
    if (index < n) {
            return v[index];
    } else {
            return v_last + 10*(index-n+1);
    }
}

static void test_get_timevalue_in_ns(void **state) {
    int64_t t1, t2;
    int64_t c1, c2;
    const struct timespec req_100ms = {.tv_sec = 0, .tv_nsec = hundred_million};
    const struct timespec req_1ms   = {.tv_sec = 0, .tv_nsec = one_million};


    t1 = get_timevalue_in_ns('p');
    c1 = get_timevalue('p');
    assert_return_code(nanosleep(&req_100ms, 0) ,0);
    t2 = get_timevalue_in_ns('p');
    c2 = get_timevalue('p');
    printf("Differences are t2-t1 %" PRId64 " ns and c2-c1 %" PRId64 " cycles\n", t2-t1, c2-c1);
    printf("1 cyc in ns for p is %g\n", cyc2ns_multiplier);
    printf("Difference c2-c1 is %" PRId64 " in ns\n", cyc2ns(c2-c1));
    assert_in_range(t2-t1, 0.9 * hundred_million, 1.2 * hundred_million);
    assert_in_range(c1, t1/10, 10*t1);

    t1 = get_timevalue_in_ns('p');
    assert_return_code(nanosleep(&req_1ms, 0), 0);
    t2 = get_timevalue_in_ns('p');
    assert_in_range(t2-t1, 0.9 * one_million, 1.2 * one_million);
    
    t1 = get_timevalue_in_ns('t');
    assert_return_code(nanosleep(&req_1ms, 0), 0);
    t2 = get_timevalue_in_ns('t');
    assert_in_range(t2-t1, 0.9 * one_million, 1.2 * one_million);

    t1 = get_timevalue_in_ns('r');
    assert_return_code(nanosleep(&req_1ms, 0), 0);
    t2 = get_timevalue_in_ns('r');
    assert_in_range(t2-t1, 0.9 * one_million, 1.2 * one_million);
}

static void test_mock_get_timevalue(void **state) {
        assert_int_equal(mock_get_timevalue(true),   0);
        assert_int_equal(mock_get_timevalue(false),  1);
        assert_int_equal(mock_get_timevalue(false),  2);
        assert_int_equal(mock_get_timevalue(false),  4);
        assert_int_equal(mock_get_timevalue(false),  8);
        assert_int_equal(mock_get_timevalue(false), 16);
        assert_int_equal(mock_get_timevalue(false), 32);
        assert_int_equal(mock_get_timevalue(false), 64);
        assert_int_equal(mock_get_timevalue(false), 74);
        assert_int_equal(mock_get_timevalue(false), 84);
        // Check that restarting the counter works
        assert_int_equal(mock_get_timevalue(true), 0);
        assert_int_equal(mock_get_timevalue(false), 1);
}

static void test_run_percentile_test(void **state) {
    assert_int_equal(mock_get_timevalue(true), 0);
    int64_t *results = run_percentile_test(10, 'm');
    assert_int_equal(results[0], 1);
    assert_int_equal(results[1], 2);
    assert_int_equal(results[2], 4);
    assert_int_equal(results[3], 8);
    assert_int_equal(results[4], 16);
    assert_int_equal(results[5], 32);
    assert_int_equal(results[6], 10);
    assert_int_equal(results[7], 10);
    assert_int_equal(results[8], 10);
    assert_int_equal(results[9], 10);
}

static void test_run_highest_test(void **state) {
    assert_int_equal(mock_get_timevalue(true), 0);
    int64_t *results = run_highest_test(100, 'm', 10);
    assert_int_equal(results[0], 10);
    assert_int_equal(results[1], 10);
    assert_int_equal(results[2], 10);
    assert_int_equal(results[3], 10);
    assert_int_equal(results[4], 10);
    assert_int_equal(results[5], 10);
    assert_int_equal(results[6], 10);
    assert_int_equal(results[7], 10);
    assert_int_equal(results[8], 16);
    assert_int_equal(results[9], 32);

    results = run_highest_test(10000, 'm', 10);
    assert_int_equal(results[0], 10);
    assert_int_equal(results[1], 10);
    assert_int_equal(results[2], 10);
    assert_int_equal(results[3], 10);
    assert_int_equal(results[4], 10);
    assert_int_equal(results[5], 10);
    assert_int_equal(results[6], 10);
    assert_int_equal(results[7], 10);
    assert_int_equal(results[8], 10);
    assert_int_equal(results[9], 10);
}

static void test_run_cumulative_test(void **state) {
    assert_int_equal(mock_get_timevalue(true), 0);
    struct cumulative_test_results *results;
    results = run_cumulative_test_with_baseline(10, 0, 'm');
    assert_int_equal(results[0].diff, 0);
    assert_int_equal(results[0].timestamp, 1);
    assert_int_equal(results[1].diff, 1);
    assert_int_equal(results[2].diff, 2);
    assert_int_equal(results[3].diff, 4);
    assert_int_equal(results[4].diff, 8);
    assert_int_equal(results[5].diff, 16);
    assert_int_equal(results[9].diff, 10);
    assert_int_equal(results[10].diff, 0);
    assert_int_equal(results[10].timestamp, 0);
    
    assert_int_equal(mock_get_timevalue(true), 0);
    results = run_cumulative_test_with_baseline(10, 5, 'm');
    assert_int_equal(results[0].timestamp, 1);
    assert_int_equal(results[0].diff, 0);
    // next differences are 1, 2, 4
    assert_int_equal(results[1].timestamp, 8);
    assert_int_equal(results[1].diff, 3); // 8-5 = 3
    assert_int_equal(results[2].timestamp, 16);
    assert_int_equal(results[2].diff, 11); // 16-5
    assert_int_equal(results[10].diff, 0);
    assert_int_equal(results[10].timestamp, 5);
}

static void test_get_baseline(void **state) {
    assert_int_equal(mock_get_timevalue(true), 0);
    assert_in_range(get_baseline_time('m'), 8, 12); // should be ̃2*~10 loops
    // after this, mock clock increases in 10 counts
    assert_int_equal(get_baseline_time('m'), 10);   

   int64_t t1 = get_baseline_time('p');
   assert_true(t1 > 0);
   int64_t t2 = get_baseline_time('p');
   assert_true(t2 > 0);
   assert_in_range(t1, 0.9 * t2, 1.1 * t2); 

   printf("Sample run of get_baseline_time('p')\n");
   for (int i= 0; i<10; i++) {
        printf("%" PRId64 "\n", get_baseline_time('p'));
   } 
}

static void test_rdtsc_vs_rdtscp(void **state) {
    int64_t t1 = get_timevalue('p');
    int64_t t2 = get_timevalue('t');
    assert_in_range(t2, 0.9*t1, 1.1*t1);
}

static void test_cyc2ns(void **state) {
 // rdtscp
    int64_t t1=get_timevalue('p');
    const struct timespec req_1ms = {.tv_sec=0, .tv_nsec=one_million}; // 1 ms
    assert_return_code(nanosleep(&req_1ms, 0), 0);
    int64_t t2=get_timevalue('p');
    assert_in_range(cyc2ns(t2-t1), 0.9*one_million, 1.2*one_million);

    t1 = get_timevalue('p');
    const struct timespec req_1s = {.tv_sec=1, .tv_nsec=0};
    assert_return_code(nanosleep(&req_1s, 0), 0);
    t2 = get_timevalue('p');
    assert_in_range(cyc2ns(t2-t1), s2ns(1) * 0.9, s2ns(1) * 1.1);

// rdtsc   
    t1=get_timevalue('t');
    assert_return_code(nanosleep(&req_1ms, 0), 0);
    t2=get_timevalue('t');
    assert_in_range(cyc2ns(t2-t1), 0.9*one_million, 1.2*one_million);
}

static void test_find_highest_values(void **state) {
    struct cumulative_test_results results[10] = { 
            {0, 1},
            {1, 2},
            {2, 3},
            {3, 4},
            {4, 5},
            {5, 6},
            {6, 7},
            {7, 8},
            {8, 9},
            {9, 10}
    };
    int64_t highest_values1[1] = {0};
    find_highest_values(results, 2, highest_values1, 1);
    assert_int_equal(highest_values1[0], 2);

    int64_t highest_values2[5] = {0};
    find_highest_values(results, 5, highest_values2, 5);
    assert_int_equal(highest_values2[0], 1);
    assert_int_equal(highest_values2[1], 2);
    assert_int_equal(highest_values2[2], 3);
    assert_int_equal(highest_values2[3], 4);
    assert_int_equal(highest_values2[4], 5);

    int64_t highest_values3[5] = {0};
    find_highest_values(results, 10, highest_values3, 5);
    assert_int_equal(highest_values3[0],  6);
    assert_int_equal(highest_values3[1],  7);
    assert_int_equal(highest_values3[2],  8);
    assert_int_equal(highest_values3[3],  9);
    assert_int_equal(highest_values3[4], 10);

}

static void test_find_highest_cumulative_values(void **state) {
    struct cumulative_test_results results[10] = { 
            {0, 1},
            {1, 1},
            {2, 1},
            {3, 1},
            {4, 1},
            {5, 1},
            {6, 1},
            {7, 1},
            {8, 1},
            {9, 1}
    };
    int64_t highest_values1[1] = {0};
    find_highest_cumulative_values(results, 2, highest_values1, 1, 1);
    assert_int_equal(highest_values1[0], 2);

    int64_t highest_values2[5] = {0};
    find_highest_cumulative_values(results, 10, highest_values2, 5, 1);
    assert_int_equal(highest_values2[0], 1);
    assert_int_equal(highest_values2[1], 1);
    assert_int_equal(highest_values2[2], 1);
    assert_int_equal(highest_values2[3], 1);
    assert_int_equal(highest_values2[4], 2);

    int64_t highest_values3[5] = {0};
    find_highest_cumulative_values(results, 10, highest_values3, 5, 2);
    assert_int_equal(highest_values3[0], 0);
    assert_int_equal(highest_values3[1], 2);
    assert_int_equal(highest_values3[2], 2);
    assert_int_equal(highest_values3[3], 2);
    assert_int_equal(highest_values3[4], 3);

}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(null_test_success),
        cmocka_unit_test(test_wordexp),
        cmocka_unit_test(test_parse_command_line_defaults),
        cmocka_unit_test(test_parse_command_line_clocktype_rdtscp),
        cmocka_unit_test(test_parse_command_line_clocktype_rdtsc),
        cmocka_unit_test(test_parse_command_line_time_interval),
        cmocka_unit_test(test_parse_command_line_iterations),
        cmocka_unit_test(test_parse_command_line_cpu_pin),
        cmocka_unit_test(test_parse_command_line_nonsense),
        cmocka_unit_test(test_get_tsc),
        cmocka_unit_test(test_get_tscp),
        cmocka_unit_test(test_get_clock_realtime),
        cmocka_unit_test(test_get_timevalue_in_ns),
        cmocka_unit_test(test_mock_get_timevalue),
        cmocka_unit_test(test_run_percentile_test),
        cmocka_unit_test(test_run_highest_test),
        cmocka_unit_test(test_run_cumulative_test),
        cmocka_unit_test(test_rdtsc_vs_rdtscp),
        cmocka_unit_test(test_cyc2ns),
        cmocka_unit_test(test_get_baseline),
        cmocka_unit_test(test_find_highest_values),
        cmocka_unit_test(test_find_highest_cumulative_values),
    };
    initialize_cyc2ns_multiplier('p');
    return cmocka_run_group_tests(tests, NULL, NULL);
}
