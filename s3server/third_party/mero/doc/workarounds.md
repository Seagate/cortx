List of workarounds for third-party libraries and external dependencies
=======================================================================

* `sem_timedwait(3)` from _glibc_ on _Centos_ >= 7.2

  **Problem**: `sem_timedwait(3)` returns `-ETIMEDOUT` immediately if `tv_sec` is
  greater than `gettimeofday(2) + INT_MAX`, that makes `m0_semaphore_timeddown(M0_TIME_NEVER)`
  to exit immediately instead of sleeping "forever".

  **Solution**: truncate `abs_timeout` inside `m0_semaphore_timeddown()` to
  `INT_MAX - 1`.

  **Impact**: limits sleep duration for `m0_semaphore_timeddown(M0_TIME_NEVER)`
  calls to fixed point in time which is `INT_MAX` seconds starting from the
  beginning of Unix epoch, which is approximately year 2038.

  **Source**: `lib/user_space/semaphore.c: m0_semaphore_timeddown()`

  **References**:
    - [CASTOR-1990: Different sem_timedwait() behaviour on real cluster node and EC2 node](https://jts.seagate.com/browse/CASTOR-1990)
    - [Bug 1412082 - futex_abstimed_wait() always converts abstime to relative time](https://bugzilla.redhat.com/show_bug.cgi?id=1412082)

* `sched_getcpu(3)` on KVM guest

  **Problem**: `sched_getcpu(3)` can return 0 on a KVM guest system regardless of cpu number.

  **Solution**: add run-time check and fall back to syscall `getcpu(2)` on problem system.

  **Impact**: syscall leads to context switches.

  **Source**: `lib/user_space/processor.c processor_getcpu_init()`

  **References**:
    - [MERO-2500: Mero panic: (locality == m0_locality_here()) at m0_locality_chores_run()](https://jts.seagate.com/browse/MERO-2500)
