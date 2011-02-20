static inline void
spin_lock(volatile unsigned int *lock)
{
#if defined(__x86__) || defined(__x86_64__)
            asm volatile(
                    "\n1:\t"
		    "pause\n"
                    "lock decl %0\n\t"
                    "jns 2f\n"
                    "jmp 1b\n"
                    "2:\t" : "=m" (*lock) : : "memory");
#else
	*lock = 0;
#endif

          return;
}

static inline void
spin_unlock(volatile unsigned int *lock)
{
#if defined(__x86__) || defined(__x86_64__)
        asm volatile("movl $1,%0" :"=m" (*lock) :: "memory");
#else
	*lock = 1;
#endif
        return;
}

#define LOCK_NAME "test_and_set"
#define LOCK_DEFINE volatile unsigned int lock = 1
#define LOCK spin_lock(&lock)
#define UNLOCK spin_unlock(&lock)

