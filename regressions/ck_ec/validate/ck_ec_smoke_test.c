#include <assert.h>
#include <ck_ec.h>
#include <ck_limits.h>
#include <ck_stdbool.h>
#include <pthread.h>
#include <stdio.h>
#include <sys/time.h>
#include <unistd.h>

#define TIME_MAX ((time_t)((1ULL << ((sizeof(time_t) * CHAR_BIT) - 1)) - 1))

#ifndef __linux__
/* Zero-initialize to mark the ops as unavailable. */
static const struct ck_ec_ops test_ops;
#else
#include <linux/futex.h>
#include <sys/syscall.h>
#include <time.h>

static int gettime(const struct ck_ec_ops *, struct timespec *out);
static void wake32(const struct ck_ec_ops *, const uint32_t *);
static void wait32(const struct ck_ec_wait_state *, const uint32_t *,
		   uint32_t, const struct timespec *);
static void wake64(const struct ck_ec_ops *, const uint64_t *);
static void wait64(const struct ck_ec_wait_state *, const uint64_t *,
		   uint64_t, const struct timespec *);

static const struct ck_ec_ops test_ops = {
	.gettime = gettime,
	.wait32 = wait32,
	.wait64 = wait64,
	.wake32 = wake32,
	.wake64 = wake64
};

static int gettime(const struct ck_ec_ops *ops, struct timespec *out)
{
	assert(ops == &test_ops);
	return clock_gettime(CLOCK_MONOTONIC, out);
}

static void wait32(const struct ck_ec_wait_state *state,
		   const uint32_t *address, uint32_t expected,
		   const struct timespec *deadline)
{
	assert(state->ops == &test_ops);
	syscall(SYS_futex, address,
		FUTEX_WAIT_BITSET, expected, deadline,
		NULL, FUTEX_BITSET_MATCH_ANY, 0);
	return;
}

static void wait64(const struct ck_ec_wait_state *state,
		   const uint64_t *address, uint64_t expected,
		   const struct timespec *deadline)
{
	const void *low_half;

	assert(state->ops == &test_ops);

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
	low_half = address;
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
	low_half = (void *)((uintptr_t)address + sizeof(uint32_t));
#else
# error "__BYTE_ORDER__ must be defined."
#endif

	syscall(SYS_futex, low_half,
		FUTEX_WAIT_BITSET, (uint32_t)expected, deadline,
		NULL, FUTEX_BITSET_MATCH_ANY, 0);
	return;
}

static void wake32(const struct ck_ec_ops *ops, const uint32_t *address)
{
	assert(ops == &test_ops);
	syscall(SYS_futex, address,
		FUTEX_WAKE, INT_MAX,
		/* ignored arguments */NULL, NULL, 0);
	return;
}

static void wake64(const struct ck_ec_ops *ops, const uint64_t *address)
{
	const void *low_half;

	assert(ops == &test_ops);

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
	low_half = address;
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
	low_half = (void *)((uintptr_t)address + sizeof(uint32_t));
#else
# error "__BYTE_ORDER__ must be defined."
#endif

	syscall(SYS_futex, low_half,
		FUTEX_WAKE, INT_MAX,
		/* ignored arguments */NULL, NULL, 0);
	return;
}
#endif /* __linux__ */

static const struct ck_ec_mode sp = {
	.ops = &test_ops,
	.single_producer = true
};

static const struct ck_ec_mode mp = {
	.ops = &test_ops,
	.single_producer = false
};

static void test_update_counter_32(const struct ck_ec_mode *mode)
{
	struct ck_ec32 ec = CK_EC_INITIALIZER;

	assert(ck_ec_value(&ec) == 0);

	ck_ec_inc(&ec, mode);
	assert(ck_ec_value(&ec) == 1);

	uint32_t old = ck_ec_add(&ec, mode, 42);
	assert(old == 1);
	assert(ck_ec_value(&ec) == 43);
	return;
}

#ifdef CK_F_EC64
static void test_update_counter_64(const struct ck_ec_mode *mode)
{
	struct ck_ec64 ec = CK_EC_INITIALIZER;

	assert(ck_ec_value(&ec) == 0);

	ck_ec_inc(&ec, mode);
	assert(ck_ec_value(&ec) == 1);

	uint64_t old = ck_ec_add(&ec, mode, 42);
	assert(old == 1);
	assert(ck_ec_value(&ec) == 43);
	return;
}
#endif

static void test_deadline(void)
{
	struct timespec deadline;

	assert(ck_ec_deadline(&deadline, &sp, NULL) == 0);
	assert(deadline.tv_sec == TIME_MAX);

	{
		const struct timespec timeout = {
			.tv_sec = 1,
			.tv_nsec = 1000
		};
		const struct timespec no_timeout = {
			.tv_sec = 0
		};
		struct timespec now;

		assert(ck_ec_deadline(&deadline, &sp, &timeout) == 0);
		assert(ck_ec_deadline(&now, &sp, &no_timeout) == 0);

		double now_sec = now.tv_sec + 1e-9 * now.tv_nsec;
		double deadline_sec = deadline.tv_sec + 1e-9 * deadline.tv_nsec;
		assert(now_sec < deadline_sec);
		assert(deadline_sec <= now_sec + 1 + 1000e-9);
	}

	{
		const struct timespec timeout = {
			.tv_sec = TIME_MAX - 1,
			.tv_nsec = 1000
		};

		assert(ck_ec_deadline(&deadline, &sp, &timeout) == 0);
		assert(deadline.tv_sec == TIME_MAX);
	}

	return;
}

static void test_wait_32(void)
{
	struct timespec deadline = { .tv_sec = 0 };
	struct ck_ec32 ec;

	ck_ec_init(&ec, 1);
	assert(ck_ec_value(&ec) == 1);
	assert(ck_ec_wait(&ec, &sp, 2, NULL) == 0);
	assert(ck_ec_wait(&ec, &sp, 1, &deadline) == -1);

	{
		const struct timespec timeout = { .tv_nsec = 1 };

		assert(ck_ec_deadline(&deadline, &sp, &timeout) == 0);
		assert(ck_ec_wait(&ec, &sp, 1, &deadline) == -1);
		assert(ck_ec_has_waiters(&ec));
	}

	return;
}

#ifdef CK_F_EC64
static void test_wait_64(void)
{
	struct timespec deadline = { .tv_sec = 0 };
	struct ck_ec64 ec;

	ck_ec_init(&ec, 0);
	assert(ck_ec_value(&ec) == 0);
	assert(ck_ec_wait(&ec, &sp, 1, NULL) == 0);
	assert(ck_ec_wait(&ec, &sp, 0, &deadline) == -1);

	{
		const struct timespec timeout = { .tv_nsec = 1 };

		assert(ck_ec_deadline(&deadline, &sp, &timeout) == 0);
		assert(ck_ec_wait(&ec, &sp, 0, &deadline) == -1);
		assert(ck_ec_has_waiters(&ec));
	}

	return;
}
#endif

static int pred(const struct ck_ec_wait_state *state,
		struct timespec *deadline)
{
	double initial_ts = state->start.tv_sec +
	    1e-9 * state->start.tv_nsec;
	int *count = state->data;

	printf("pred wait: %f\n",
	       deadline->tv_sec + 1e-9 * deadline->tv_nsec - initial_ts);

	if ((*count)++ < 3) {
		return 0;
	}

	return (*count)++;
}

/*
 * Check that pred's return value is correctly bubbled up,
 * and that the event count is marked as having waiters.
 */
static void test_wait_pred_32(void)
{
	struct ck_ec32 ec = CK_EC_INITIALIZER;
	int count = 0;

	assert(!ck_ec_has_waiters(&ec));
	assert(ck_ec_wait_pred(&ec, &sp, 0, pred, &count, NULL) == 4);
	assert(ck_ec_has_waiters(&ec));
	assert(count == 5);
	return;
}

#ifdef CK_F_EC64
static int pred2(const struct ck_ec_wait_state *state,
		 struct timespec *deadline)
{
	double initial_ts = state->start.tv_sec +
	    1e-9 * state->start.tv_nsec;
	int *count = state->data;

	printf("pred2 wait: %f\n",
	       deadline->tv_sec + 1e-9 * deadline->tv_nsec - initial_ts);

	*deadline = state->now;
	deadline->tv_sec++;

	(*count)++;
	return 0;
}

/*
 * wait_pred_64 is nearly identical to _32. Now check that deadline
 * overriding works.
 */
static void test_wait_pred_64(void)
{
	const struct timespec timeout = { .tv_sec = 5 };
	struct timespec deadline;
	struct ck_ec64 ec = CK_EC_INITIALIZER;
	int count = 0;

	assert(!ck_ec_has_waiters(&ec));
	assert(ck_ec_deadline(&deadline, &sp, &timeout) == 0);
	assert(ck_ec_wait_pred(&ec, &sp, 0, pred2, &count, &deadline) == -1);
	assert(ck_ec_has_waiters(&ec));
	assert(count == 5);
	return;
}
#endif

static int woken = 0;

static void *test_threaded_32_waiter(void *data)
{
	struct ck_ec32 *ec = data;

	ck_ec_wait(ec, &sp, 0, NULL);
	ck_pr_store_int(&woken, 1);
	return NULL;
}

static void test_threaded_inc_32(const struct ck_ec_mode *mode)
{
	struct ck_ec32 ec = CK_EC_INITIALIZER;
	pthread_t waiter;

	ck_pr_store_int(&woken, 0);

	pthread_create(&waiter, NULL, test_threaded_32_waiter, &ec);
	usleep(10000);

	assert(ck_pr_load_int(&woken) == 0);
	ck_ec_inc(&ec, mode);

	pthread_join(waiter, NULL);
	assert(ck_pr_load_int(&woken) == 1);
	return;
}

static void test_threaded_add_32(const struct ck_ec_mode *mode)
{
	struct ck_ec32 ec = CK_EC_INITIALIZER;
	pthread_t waiter;

	ck_pr_store_int(&woken, 0);

	pthread_create(&waiter, NULL, test_threaded_32_waiter, &ec);
	usleep(10000);

	assert(ck_pr_load_int(&woken) == 0);
	ck_ec_add(&ec, mode, 4);

	pthread_join(waiter, NULL);
	assert(ck_pr_load_int(&woken) == 1);
	return;
}

#ifdef CK_F_EC64
static void *test_threaded_64_waiter(void *data)
{
	struct ck_ec64 *ec = data;

	ck_ec_wait(ec, &sp, 0, NULL);
	ck_pr_store_int(&woken, 1);
	return NULL;
}

static void test_threaded_inc_64(const struct ck_ec_mode *mode)
{
	struct ck_ec64 ec = CK_EC_INITIALIZER;
	pthread_t waiter;

	ck_pr_store_int(&woken, 0);

	pthread_create(&waiter, NULL, test_threaded_64_waiter, &ec);
	usleep(10000);

	assert(ck_pr_load_int(&woken) == 0);
	ck_ec_inc(&ec, mode);

	pthread_join(waiter, NULL);
	assert(ck_pr_load_int(&woken) == 1);
	return;
}

static void test_threaded_add_64(const struct ck_ec_mode *mode)
{
	struct ck_ec64 ec = CK_EC_INITIALIZER;
	pthread_t waiter;

	ck_pr_store_int(&woken, 0);

	pthread_create(&waiter, NULL, test_threaded_64_waiter, &ec);
	usleep(10000);

	assert(ck_pr_load_int(&woken) == 0);
	ck_ec_add(&ec, mode, 4);

	pthread_join(waiter, NULL);
	assert(ck_pr_load_int(&woken) == 1);
	return;
}
#endif

int main(int argc, char **argv)
{
	(void)argc;
	(void)argv;

	if (test_ops.gettime == NULL ||
	    test_ops.wake32 == NULL ||
	    test_ops.wait32 == NULL) {
		printf("No ck_ec ops for this platform. Trivial success.\n");
		return 0;
	}

	test_update_counter_32(&sp);
#ifdef CK_F_EC64
	test_update_counter_64(&sp);
#endif
	printf("test_update_counter SP passed.\n");

	test_update_counter_32(&mp);
#ifdef CK_F_EC64
	test_update_counter_64(&mp);
#endif
	printf("test_update_counter MP passed.\n");

	test_deadline();
	printf("test_deadline passed.\n");

	test_wait_32();
#ifdef CK_F_EC64
	test_wait_64();
#endif
	printf("test_wait passed.\n");

	test_wait_pred_32();
#ifdef CK_F_EC64
	test_wait_pred_64();
#endif
	printf("test_wait_pred passed.\n");

	test_threaded_inc_32(&sp);
	test_threaded_add_32(&sp);
#ifdef CK_F_EC64
	test_threaded_inc_64(&sp);
	test_threaded_add_64(&sp);
#endif
	printf("test_threaded SP passed.\n");

	test_threaded_inc_32(&mp);
	test_threaded_add_32(&mp);
#ifdef CK_F_EC64
	test_threaded_inc_64(&mp);
	test_threaded_add_64(&mp);
#endif
	printf("test_threaded MP passed.\n");
	return 0;
}
