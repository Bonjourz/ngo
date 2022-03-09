#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include "test.h"

// ============================================================================
// Helper macros
// ============================================================================

#define S           (1000 * 1000 * 1000)
#define MS          (1000 * 1000)
#define US          (1000)
#define NS          (1)

// ============================================================================
// Global variables
// ============================================================================

// The time obtained from Occlum is not very precise.
// Here we take 1 millisecond as the time precision of Occlum.
const int SUCCESS = 1;
const int FAIL = -1;

static struct timespec OS_TIME_PRECISION = {
    .tv_sec = 0,
    .tv_nsec = 1 * MS,
};

// ============================================================================
// Helper functions
// ============================================================================

static inline void validate_timespec(const struct timespec *tv) {
    assert(tv->tv_sec >= 0 && tv->tv_nsec >= 0 && tv->tv_nsec < S);
}


// retval = (a < b) ? -1 : ((a > b) ? 1 : 0)
static int timespec_cmp(const struct timespec *a, const struct timespec *b) {
    validate_timespec(a);
    validate_timespec(b);

    if (a->tv_sec < b->tv_sec) {
        return -1;
    } else if (a->tv_sec > b->tv_sec) {
        return 1;
    } else {
        return a->tv_nsec < b->tv_nsec ? -1 :
               (a->tv_nsec > b->tv_nsec ? 1 : 0);
    }
}

// diff = | a - b |
static void timespec_diff(const struct timespec *a, const struct timespec *b,
                          struct timespec *diff) {
    validate_timespec(a);
    validate_timespec(b);

    const struct timespec *begin, *end;
    if (timespec_cmp(a, b) <= 0) {
        begin = a;
        end = b;
    } else {
        begin = b;
        end = a;
    }

    diff->tv_nsec = end->tv_nsec - begin->tv_nsec;
    diff->tv_sec = end->tv_sec - begin->tv_sec;
    if (diff->tv_nsec < 0) {
        diff->tv_nsec += S;
        diff->tv_sec -= 1;
    }

    validate_timespec(diff);
}

// retval = | a - b | <= precision
static int timespec_equal(const struct timespec *a, const struct timespec *b,
                          const struct timespec *precision) {
    struct timespec diff;
    timespec_diff(a, b, &diff);
    return timespec_cmp(&diff, precision) <= 0;
}


// retval = a + b
static void timespec_add(const struct timespec *a, const struct timespec *b,
                         struct timespec *res) {
    validate_timespec(a);
    validate_timespec(b);

    res->tv_sec = a->tv_sec + b->tv_sec;
    res->tv_nsec = a->tv_nsec + b->tv_nsec;
    if (res->tv_nsec >= S) {
        res->tv_nsec -= S;
        res->tv_sec += 1;
    }

    validate_timespec(res);
}

// Return SUCCESS(1) if check passed, FAIL(-1) if check failed
static int check_nanosleep(const struct timespec *expected_sleep_period) {
    struct timespec begin_timestamp, end_timestamp;
    clock_gettime(CLOCK_MONOTONIC, &begin_timestamp);

    if (nanosleep(expected_sleep_period, NULL) != 0) {
        THROW_ERROR("nanosleep failed");
    }

    clock_gettime(CLOCK_MONOTONIC, &end_timestamp);
    struct timespec actual_sleep_period;
    timespec_diff(&begin_timestamp, &end_timestamp, &actual_sleep_period);

    return timespec_equal(expected_sleep_period, &actual_sleep_period,
                          &OS_TIME_PRECISION) ? SUCCESS : FAIL;
}

// Return SUCCESS(1) if check passed, FAIL(-1) if check failed
static int check_clock_nanosleep_interval_with_clockid(clockid_t clock_id,
        const struct timespec *expected_sleep_period) {
    struct timespec begin_timestamp, end_timestamp;
    clock_gettime(clock_id, &begin_timestamp);
    if (clock_nanosleep(clock_id, 0, expected_sleep_period, NULL) != 0) {
        THROW_ERROR("nanosleep failed");
    }
    clock_gettime(clock_id, &end_timestamp);
    struct timespec actual_sleep_period;
    timespec_diff(&begin_timestamp, &end_timestamp, &actual_sleep_period);

    return timespec_equal(expected_sleep_period, &actual_sleep_period,
                          &OS_TIME_PRECISION) ? SUCCESS : FAIL;
}

// Return SUCCESS(1) if check passed, FAIL(-1) if check failed
static int check_clock_nanosleep_for_abs_time_with_clockid(clockid_t clock_id) {
    struct timespec begin_timestamp, end_timestamp, actual_period, req_time,
               actual_sleep_period;

    // req_time = current + 0s, expect to return immediately
    actual_period.tv_sec = 0;
    actual_period.tv_nsec = 0;
    clock_gettime(clock_id, &begin_timestamp);
    timespec_add(&begin_timestamp, &actual_period, &req_time);
    if (clock_nanosleep(clock_id, TIMER_ABSTIME, &req_time, NULL) != 0) {
        THROW_ERROR("nanosleep failed");
    }

    clock_gettime(clock_id, &end_timestamp);
    timespec_diff(&begin_timestamp, &end_timestamp, &actual_sleep_period);
    if (timespec_equal(&actual_period, &actual_sleep_period, &OS_TIME_PRECISION) != SUCCESS) {
        return FAIL;
    }

    // req_time = current + 1s, expect to sleep for 1s
    actual_period.tv_sec = 1;
    actual_period.tv_nsec = 0;
    clock_gettime(clock_id, &begin_timestamp);
    timespec_add(&begin_timestamp, &actual_period, &req_time);
    if (clock_nanosleep(clock_id, TIMER_ABSTIME, &req_time, NULL) != 0) {
        THROW_ERROR("nanosleep failed");
    }
    clock_gettime(clock_id, &end_timestamp);
    timespec_diff(&begin_timestamp, &end_timestamp, &actual_sleep_period);
    if (timespec_equal(&actual_period, &actual_sleep_period, &OS_TIME_PRECISION) != SUCCESS) {
        return FAIL;
    }

    // req_time = current - 1s, expect to return immediately
    actual_period.tv_sec = 0;
    actual_period.tv_nsec = 0;
    clock_gettime(clock_id, &begin_timestamp);
    timespec_add(&begin_timestamp, &actual_period, &req_time);
    req_time.tv_sec -= 1;
    validate_timespec(&req_time);
    if (clock_nanosleep(clock_id, TIMER_ABSTIME, &req_time, NULL) != 0) {
        THROW_ERROR("nanosleep failed");
    }
    clock_gettime(clock_id, &end_timestamp);
    timespec_diff(&begin_timestamp, &end_timestamp, &actual_sleep_period);
    if (timespec_equal(&actual_period, &actual_sleep_period, &OS_TIME_PRECISION) != SUCCESS) {
        return FAIL;
    }

    return SUCCESS;
}

// ============================================================================
// Test cases
// Return SUCCESS(1) if check passed, FAIL(-1) if check failed
// ============================================================================

static int test_nanosleep_0_second() {
    struct timespec period_of_0s = { .tv_sec = 0, .tv_nsec = 0 };
    return check_nanosleep(&period_of_0s);
}

static int test_nanosleep_1_second() {
    struct timespec period_of_1s = { .tv_sec = 1, .tv_nsec = 0 };
    return check_nanosleep(&period_of_1s);
}

static int test_nanosleep_10ms() {
    struct timespec period_of_10ms = { .tv_sec = 0, .tv_nsec = 10 * MS };
    return check_nanosleep(&period_of_10ms);
}

static int test_clock_nanosleep_for_interval_time() {
    struct timespec period;

    // CLOCK_REALTIME with 0s
    period.tv_sec = 0;
    period.tv_nsec = 0;
    if (check_clock_nanosleep_interval_with_clockid(CLOCK_REALTIME, &period) != SUCCESS) {
        printf("check_clock_nanosleep_interval failed with peroid={ %lds , %ld ms }, clock_id=%d\n",
               period.tv_sec, period.tv_nsec, CLOCK_REALTIME);
        return FAIL;
    }

    // CLOCK_REALTIME with 1s
    period.tv_sec = 1;
    period.tv_nsec = 0;
    if (check_clock_nanosleep_interval_with_clockid(CLOCK_REALTIME, &period) != SUCCESS) {
        printf("check_clock_nanosleep_interval failed with peroid={ %ld s, %ld ms }, clock_id=%d\n",
               period.tv_sec, period.tv_nsec, CLOCK_REALTIME);
        return FAIL;
    }

    // CLOCK_REALTIME with 10ms
    period.tv_sec = 0;
    period.tv_nsec = 10 * MS;
    if (check_clock_nanosleep_interval_with_clockid(CLOCK_REALTIME, &period) != SUCCESS) {
        printf("check_clock_nanosleep_interval failed with peroid={ %ld s, %ld ms }, clock_id=%d\n",
               period.tv_sec, period.tv_nsec, CLOCK_REALTIME);
        return FAIL;
    }

    return SUCCESS;
}

static int test_clock_nanosleep_for_abs_time() {
    if (check_clock_nanosleep_for_abs_time_with_clockid(CLOCK_REALTIME) != SUCCESS ||
            check_clock_nanosleep_for_abs_time_with_clockid(CLOCK_MONOTONIC) != SUCCESS ||
            check_clock_nanosleep_for_abs_time_with_clockid(CLOCK_BOOTTIME) != SUCCESS) {
        return FAIL;
    }

    return SUCCESS;
}

// ============================================================================
// Test cases with invalid arguments
// Return SUCCESS(1) if check passed, -1 if check failed
// ============================================================================

static int test_nanosleep_with_null_req() {
    if (nanosleep(NULL, NULL) != -1 && errno != EINVAL) {
        THROW_ERROR("nanosleep should report error");
    }
    return SUCCESS;
}

static int test_nanosleep_with_negative_tv_sec() {
    // nanosleep returns EINVAL if the value in the tv_sec field is negative
    struct timespec invalid_period = { .tv_sec = -1, .tv_nsec = 0};
    if (nanosleep(&invalid_period, NULL) != -1 && errno != EINVAL) {
        THROW_ERROR("nanosleep should report EINVAL error");
    }
    return SUCCESS;
}

static int test_nanosleep_with_negative_tv_nsec() {
    // nanosleep returns EINVAL if the value in the tv_nsec field
    // was not in the range 0 to 999999999.
    struct timespec invalid_period = { .tv_sec = 0, .tv_nsec = -1};
    if (nanosleep(&invalid_period, NULL) != -1 && errno != EINVAL) {
        THROW_ERROR("nanosleep should report EINVAL error");
    }
    return SUCCESS;
}

static int test_nanosleep_with_too_large_tv_nsec() {
    // nanosleep returns EINVAL if the value in the tv_nsec field
    // was not in the range 0 to 999999999 (10^6 - 1).
    struct timespec invalid_period = { .tv_sec = 0, .tv_nsec = S};
    if (nanosleep(&invalid_period, NULL) != -1 && errno != EINVAL) {
        THROW_ERROR("nanosleep should report EINVAL error");
    }
    return SUCCESS;
}

static int test_clock_nanosleep_with_invalid_flag() {
    // clock_nanosleep returns EINVAL if clock_id was invalid
    struct timespec period = { .tv_sec = 1, .tv_nsec = 0 };
    if (clock_nanosleep(CLOCK_THREAD_CPUTIME_ID, 0, &period, NULL) != EINVAL && errno != 0) {
        THROW_ERROR("nanosleep should report EINVAL error");
    }
}

// ============================================================================
// Test suite main
// ============================================================================

// TODO: test interruption
static test_case_t test_cases[] = {
    // Test cases for nanosleep()
    TEST_CASE(test_nanosleep_0_second),
    TEST_CASE(test_nanosleep_1_second),
    TEST_CASE(test_nanosleep_10ms),
    TEST_CASE(test_nanosleep_with_null_req),
    TEST_CASE(test_nanosleep_with_negative_tv_sec),
    TEST_CASE(test_nanosleep_with_negative_tv_nsec),
    TEST_CASE(test_nanosleep_with_too_large_tv_nsec),

    // Test cases for clock_nanosleep()
    TEST_CASE(test_clock_nanosleep_for_interval_time),
    TEST_CASE(test_clock_nanosleep_for_abs_time),
    TEST_CASE(test_clock_nanosleep_with_invalid_flag),
};

int main() {
    return test_suite_run(test_cases, ARRAY_SIZE(test_cases));
}
