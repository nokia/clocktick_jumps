extern char const *clock_name_r;
extern char const *clock_name_t;
extern char const *clock_name_p;

extern char const *reporttype_name_p;
extern char const *reporttype_name_h;
extern char const *reporttype_name_c;

void print_usage();

struct command_line_arguments {
    char clocktype;
    char const **clockname;
    int cpu_pin;
    char reporttype;
    char const **reportname;
    long int time_interval_us;
    long int iterations;
};

extern struct command_line_arguments default_arguments;

int parse_command_line(int argc, char **argv, struct command_line_arguments*);


