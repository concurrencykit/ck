#define LOCK_NAME "ck_ticket_pb"
#define LOCK_DEFINE static ck_spinlock_ticket_t CK_CC_CACHELINE lock = CK_SPINLOCK_TICKET_INITIALIZER
#define LOCK ck_spinlock_ticket_lock_pb(&lock)
#define UNLOCK ck_spinlock_ticket_unlock(&lock)

