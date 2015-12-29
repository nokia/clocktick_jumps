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
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
