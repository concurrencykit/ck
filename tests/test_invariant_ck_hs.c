#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ck_hs.h>
#include <ck_malloc.h>

static void *hs_malloc(size_t r) { return malloc(r); }
static void hs_free(void *p, size_t b, bool r) { (void)b; (void)r; free(p); }

static struct ck_malloc my_allocator = {
    .malloc = hs_malloc,
    .free = hs_free
};

/* Hash function that always returns the same value to simulate collision attack */
static unsigned long collision_hash(const void *key, unsigned long seed)
{
    (void)key; (void)seed;
    return 42; /* All keys collide */
}

START_TEST(test_hs_collision_dos_bounded_operations)
{
    /* Invariant: Even under full hash collisions, insert and lookup must
       complete correctly (no infinite loops, no corruption). The hash set
       must remain functionally correct regardless of probe sequence length. */
    ck_hs_t hs;
    bool ret;
    unsigned long h;
    const int N = 1024; /* Adversarial: many keys all colliding */

    ret = ck_hs_init(&hs, CK_HS_MODE_OBJECT | CK_HS_MODE_SPMC,
                     collision_hash, NULL, &my_allocator, 16, 0);
    ck_assert(ret == true);

    /* Insert N keys that all hash to the same bucket */
    uintptr_t keys[1024];
    for (int i = 0; i < N; i++) {
        keys[i] = (uintptr_t)(malloc(1)); /* unique pointers as keys */
        ck_assert(keys[i] != 0);
        h = collision_hash((const void *)keys[i], 0);
        ret = ck_hs_put(&hs, h, (const void *)keys[i]);
        ck_assert_msg(ret == true, "Insert failed at i=%d", i);
    }

    /* Verify all keys are retrievable (correctness under collision) */
    for (int i = 0; i < N; i++) {
        h = collision_hash((const void *)keys[i], 0);
        void *found = ck_hs_get(&hs, h, (const void *)keys[i]);
        ck_assert_msg(found == (void *)keys[i],
                      "Lookup failed for key %d", i);
    }

    /* Cleanup */
    for (int i = 0; i < N; i++)
        free((void *)keys[i]);
    ck_hs_destroy(&hs);
}
END_TEST

Suite *security_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Security");
    tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, 5); /* Must complete within 5 seconds */

    tcase_add_test(tc_core, test_hs_collision_dos_bounded_operations);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = security_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}