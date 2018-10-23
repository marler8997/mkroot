#define current_error errno ? errno : 1
typedef int err_t;

static const err_t err_pass = 0;
static const err_t err_fail = 1;

#define logf(fmt, ...)   printf(fmt "\n", ##__VA_ARGS__)
#define errf(fmt, ...) fprintf(stderr, "Error: " fmt "\n", ##__VA_ARGS__)
#define errnof(fmt, ...) fprintf(stderr, "Error(%d) " fmt ": %s\n", errno, ##__VA_ARGS__, strerror(errno))
