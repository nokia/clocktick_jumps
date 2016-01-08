#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
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
    assert_int_equal(cl.time_interval_us, 1000000);
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
    assert_int_equal(cl.time_interval_us, 12345);
}

static void test_parse_command_line_iterations(void **state) {   
    struct command_line_arguments cl = default_arguments;  
    wordexp_t p;
    assert_return_code(wordexp("cj -i 12345", &p, 0), 0);
    assert_return_code(parse_command_line(p.we_wordc, p.we_wordv, &cl), 0);
    assert_int_equal(cl.iterations, 12345);
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

// Basic sanity check that start_cycle and stop_cycle do something and that time moves forward
static void test_start_stop_cycle(void **state) {
    uint64_t start, stop;
    struct timespec const t = {.tv_nsec = 100}; // sleep 100 nanoseconds
    struct timespec rem;                        // remaining time
    start = start_cycle();
    nanosleep(&t, &rem);
    stop = stop_cycle();
    assert_true(stop > start);
}   

// Sanity check for get_tsc
static void test_get_tsc(void **state) {
    uint64_t start, stop;
    struct timespec const t = {.tv_nsec = 100}; 
    struct timespec rem;
    start = get_tsc();
    nanosleep(&t, &rem);
    stop = get_tsc();
    assert_in_range(stop - start, 90, 1000000);
}

// Same for get_clock_readtime
static void test_get_clock_realtime(void **state) {
    uint64_t start, stop;
    struct timespec const t = {.tv_nsec = one_million}; 
    struct timespec rem;
    start = get_clock_realtime();
    nanosleep(&t, &rem);
    stop = get_clock_realtime();
    assert_in_range(stop - start, 0.9*one_million, 1.2*one_million);
}

long int mock_get_timevalue(bool restart) {
    long int v[] = {0, 1, 2, 4, 8, 16, 32, 64};
    int n = sizeof(v)/sizeof(long int);
    long int v_last = v[n-1];
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
    long int *results = run_percentile_test(10, 'm');
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
    long int *results = run_highest_test(100, 'm', 10);
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
    long int *results = run_cumulative_test_with_baseline(10, 100, 10, 'm');
    assert_int_equal(results[0], 28);  // 32 and 16 go over, (32-10)+(16-10) = 28
    assert_int_equal(results[1], 0);
    assert_int_equal(results[2], 0);
    assert_int_equal(results[3], 0);
    assert_int_equal(results[4], 0);
    assert_int_equal(results[5], 0);
    assert_int_equal(results[6], 0);
    assert_int_equal(results[7], 0);
    assert_int_equal(results[8], 0);
    assert_int_equal(results[9], 0);

    results = run_cumulative_test_with_baseline(10, 100, 100, 'm');
    assert_int_equal(results[0], 0);
    assert_int_equal(results[1], 0);
    assert_int_equal(results[2], 0);
    assert_int_equal(results[3], 0);
    assert_int_equal(results[4], 0);
    assert_int_equal(results[5], 0);
    assert_int_equal(results[6], 0);
    assert_int_equal(results[7], 0);
    assert_int_equal(results[8], 0);
    assert_int_equal(results[9], 0);

    results = run_cumulative_test_with_baseline(10, 100, 5, 'm');
    // Executed nine times (start=0, 10, 20, ..., 90
    assert_int_equal(results[0], 50);
    assert_int_equal(results[1], 50);
    assert_int_equal(results[2], 50);
    assert_int_equal(results[3], 50);
    assert_int_equal(results[4], 50);
    assert_int_equal(results[5], 50);
    assert_int_equal(results[6], 50);
    assert_int_equal(results[7], 50);
    assert_int_equal(results[8], 50);
    assert_int_equal(results[9], 50);

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
        cmocka_unit_test(test_start_stop_cycle),
        cmocka_unit_test(test_get_tsc),
        cmocka_unit_test(test_get_clock_realtime),
        cmocka_unit_test(test_mock_get_timevalue),
        cmocka_unit_test(test_run_percentile_test),
        cmocka_unit_test(test_run_highest_test),
        cmocka_unit_test(test_run_cumulative_test),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
