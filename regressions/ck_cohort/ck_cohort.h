#define LOCK_NAME "ck_cohort"
#define LOCK_DEFINE\
    typedef ck_spinlock_fas_t ck_spinlock_fas;\
    static ck_spinlock_fas global_fas_lock = CK_SPINLOCK_FAS_INITIALIZER;\
    static ck_spinlock_fas local_fas_lock = CK_SPINLOCK_FAS_INITIALIZER;\
	CK_COHORT_PROTOTYPE(fas_fas, ck_spinlock_fas, ck_spinlock_fas)\
    static struct ck_cohort_fas_fas CK_CC_CACHELINE cohort = CK_COHORT_INITIALIZER
#define LOCK_INIT ck_cohort_fas_fas_init(&cohort, &global_fas_lock, &local_fas_lock)
#define LOCK ck_cohort_fas_fas_lock(&cohort)
#define UNLOCK ck_cohort_fas_fas_unlock(&cohort)
