# Concurrency Kit

Modern concurrency primitives and building blocks for high performance applications.

### Continuous Integration

[![Build Status](https://github.com/concurrencykit/ck/workflows/CI/badge.svg)](https://github.com/concurrencykit/ck/actions?query=workflow%3ACI+branch%3Amaster)

Compilers tested in the past include gcc, clang, cygwin, icc, mingw32, mingw64
and suncc across all supported architectures. All new architectures are
required to pass the integration test and under-go extensive code review.

Continuous integration is currently enabled for the following targets:
 * `darwin/clang/arm64`
 * `freebsd/clang/x86-64`
 * `linux/gcc/arm64`
 * `linux/gcc/x86-64`
 * `linux/clang/x86-64`

### Compile and Build

* Step 1.
   `./configure`
   For additional options try `./configure --help`

* Step 2.
   In order to compile regressions (requires POSIX threads) use
   `make regressions`. In order to compile libck use `make all` or `make`.

* Step 3.
	In order to install use `make install`
	To uninstall use `make uninstall`.

See http://concurrencykit.org/ for more information.

### Supported Architectures

Concurrency Kit supports any architecture using compiler built-ins as a
fallback. There is usually a performance degradation associated with this.

Concurrency Kit has specialized assembly for the following architectures:
 * `aarch64`
 * `arm`
 * `ppc`
 * `ppc64`
 * `riscv64`
 * `s390x`
 * `sparcv9+`
 * `x86`
 * `x86_64`

### Features

#### Concurrency Primitives

##### ck_pr

Concurrency primitives including architecture-specific ones. Provides wrappers
around CAS in case of missing native support. This also provides support for
RTM (transactional memory), pipeline control, read-for-ownership and more.

##### ck_backoff

A simple and efficient (minimal noise) backoff function.

##### ck_cc

Abstracted compiler builtins when writing efficient concurrent data structures.

#### Safe Memory Reclamation

##### ck_epoch

A scalable safe memory reclamation mechanism with support for idle threads and
various optimizations that make it better than or competitive with many
state-of-the-art solutions.

##### ck_hp

Implements support for hazard pointers, a simple and efficient lock-free safe
memory reclamation mechanism.

#### Data Structures

##### ck_array

A simple concurrently-readable pointer array structure.

##### ck_bitmap

An efficient multi-reader and multi-writer concurrent bitmap structure.

##### ck_ring

Efficient concurrent bounded FIFO data structures with various performance
trade-off. This includes specialization for single-reader, many-reader,
single-writer and many-writer.

##### ck_fifo

A reference implementation of the first published lock-free FIFO algorithm,
with specialization for single-enqueuer-single-dequeuer and
many-enqueuer-single-dequeuer and extensions to allow for node re-use.

##### ck_hp_fifo

A reference implementation of the above algorithm, implemented with safe memory
reclamation using hazard pointers.

##### ck_hp_stack

A reference implementation of a Treiber stack with support for hazard pointers.

##### ck_stack

A reference implementation of an efficient lock-free stack, with specialized
variants for a variety of memory management strategies and bounded concurrency.

##### ck_queue

A concurrently readable friendly derivative of the BSD-queue interface. Coupled
with a safe memory reclamation mechanism, implement scalable read-side queues
with a simple search and replace.

##### ck_hs

An extremely efficient single-writer-many-reader hash set, that satisfies
lock-freedom with bounded concurrency without any usage of atomic operations
and allows for recycling of unused or deleted slots. This data structure is
recommended for use as a general hash-set if it is possible to compute values
from keys.

##### ck_ht

A specialization of the `ck_hs` algorithm allowing for disjunct key-value pairs.

##### ck_rhs

A variant of `ck_hs` that utilizes robin-hood hashing to allow for improved
performance with higher load factors and high deletion rates.

#### Synchronization Primitives

##### ck_ec

An extremely efficient event counter implementation, a better alternative to
condition variables with specialization for fixed concurrency use-cases.

##### ck_barrier

A plethora of execution barriers including: centralized barriers, combining
barriers, dissemination barriers, MCS barriers, tournament barriers.

##### ck_brlock

A simple big-reader lock implementation, write-biased reader-writer lock with
scalable read-side locking.

##### ck_bytelock

An implementation of bytelocks, for research purposes, allowing for (in
theory), fast read-side acquisition without the use of atomic operations. In
reality, memory barriers are required on the fast path.

##### ck_cohort

A generic lock cohorting interface, allows you to turn any lock into a
NUMA-friendly scalable NUMA lock. There is a significant trade-off in fast path
acquisition cost. Specialization is included for all relevant lock
implementations in Concurrency Kit. Learn more by reading "Lock Cohorting: A
General Technique for Designing NUMA Locks".

##### ck_elide

A generic lock elision framework, allows you to turn any lock implementation
into an elision-aware implementation. This requires support for restricted
transactional memory by the underlying hardware.

##### ck_pflock

Phase-fair reader-writer mutex that provides strong fairness guarantees between
readers and writers. Learn more by reading "Spin-Based Reader-Writer
Synchronization for Multiprocessor Real-Time Systems".

##### ck_rwcohort

A generic read-write lock cohorting interface, allows you to turn any
read-write lock into a NUMA-friendly scalable NUMA lock. There is a significant
trade-off in fast path acquisition cost. Specialization is included for all
relevant lock implementations in Concurrency Kit. Learn more by reading "Lock
Cohorting: A General Technique for Designing NUMA Locks".

##### ck_rwlock

A simple centralized write-biased read-write lock.

##### ck_sequence

A sequence counter lock, popularized by the Linux kernel, allows for very fast
read and write synchronization for simple data structures where deep copy is
permitted.

##### ck_swlock

A single-writer specialized read-lock that is copy-safe, useful for data
structures that must remain small, be copied and contain in-band mutexes.

##### ck_tflock

Task-fair locks are fair read-write locks, derived from "Scalable reader-writer
synchronization for shared-memory multiprocessors".

##### ck_spinlock

A basic but very fast spinlock implementation.

##### ck_spinlock_anderson

Scalable and fast anderson spinlocks. This is here for reference, one of the
earliest scalable and fair lock implementations.

##### ck_spinlock_cas

A basic spinlock utilizing compare_and_swap.

##### ck_spinlock_dec

A basic spinlock, a C adaption of the older optimized Linux kernel spinlock for
x86. Primarily here for reference.

##### ck_spinlock_fas

A basic spinlock utilizing atomic exchange.

##### ck_spinlock_clh

An efficient implementation of the scalable CLH lock, providing many of the
same performance properties of MCS with a better fast-path.

##### ck_spinlock_hclh

A NUMA-friendly CLH lock.

##### ck_spinlock_mcs

An implementation of the seminal scalable and fair MCS lock.

##### ck_spinlock_ticket

An implementation of fair centralized locks.

