#ifndef _LINUX_LKPI_MUTEX_H_
#define _LINUX_LKPI_MUTEX_H_
#include <sys/lock.h>
#include <sys/mutex.h>


void	__lkpi_mtx_init(volatile uintptr_t *c, const char *name, const char *type,
	    int opts);
int	__lkpi_mtx_trylock_flags(volatile uintptr_t *c, int opts, const char *file,
	    int line);
void	__lkpi_mtx_lock_flags(volatile uintptr_t *c, int opts, const char *file,
	    int line);
void	__lkpi_mtx_unlock_flags(volatile uintptr_t *c, int opts, const char *file,
	    int line);
void	__lkpi_mtx_lock_spin_flags(volatile uintptr_t *c, int opts, const char *file,
	    int line);
void	__lkpi_mtx_unlock_spin_flags(volatile uintptr_t *c, int opts, const char *file,
	    int line);


#define lkpi_mtx_init(m, n, t, o) __lkpi_mtx_init(&(m)->mtx_lock, (n), (t), (o))
		      
#if LOCK_DEBUG > 0 || defined(MUTEX_NOINLINE) 
#define lkpi_mtx_lock_spin(m)	__lkpi_mtx_lock_spin_flags(&(m)->mtx_lock, 0, __FILE__, __LINE__)
#define lkpi_mtx_unlock_spin(m)	__lkpi_mtx_unlock_spin_flags(&(m)->mtx_lock, 0, __FILE__, __LINE__)
#define lkpi_mtx_lock(m)	__lkpi_mtx_lock_flags(&(m)->mtx_lock, 0, __FILE__, __LINE__)
#define lkpi_mtx_unlock(m)	__lkpi_mtx_unlock_flags(&(m)->mtx_lock, 0, __FILE__, __LINE__)
#define lkpi_mtx_trylock(m)	__lkpi_mtx_trylock_flags(&(m)->mtx_lock, 0, __FILE__, __LINE__)

#else

#ifdef SMP
#define __lkpi_mtx_lock_spin(mp, tid, opts, file, line) do {			\
	uintptr_t _tid = (uintptr_t)(tid);				\
									\
	spinlock_enter();						\
	if (((mp)->mtx_lock != MTX_UNOWNED || !_mtx_obtain_lock((mp), _tid))) {\
		if ((mp)->mtx_lock == _tid)				\
			(mp)->mtx_recurse++;				\
		else							\
			_mtx_lock_spin_cookie(&(mp)->mtx_lock, _tid, (opts), (file), (line)); \
	} else 								\
		LOCKSTAT_PROFILE_OBTAIN_LOCK_SUCCESS(spin__acquire,	\
		    mp, 0, 0, file, line);				\
} while (0)

#define __lkpi_mtx_unlock_spin(mp) do {					\
	if (mtx_recursed((mp)))						\
		(mp)->mtx_recurse--;					\
	else {								\
		LOCKSTAT_PROFILE_RELEASE_LOCK(spin__release, mp);	\
		atomic_store_rel_ptr(&(mp)->mtx_lock, MTX_UNOWNED)	\
	}								\
	spinlock_exit();						\
} while (0)

#else /* SMP */

#define __lkpi_mtx_lock_spin(mp, tid, opts, file, line) do {		\
	uintptr_t _tid = (uintptr_t)(tid);				\
									\
	spinlock_enter();						\
	if ((mp)->mtx_lock == _tid)					\
		(mp)->mtx_recurse++;					\
	else {								\
		KASSERT((mp)->mtx_lock == MTX_UNOWNED, ("corrupt spinlock")); \
		(mp)->mtx_lock = _tid;					\
	}								\
} while (0)

#define __lkpi_mtx_unlock_spin(mp) do {					\
	if (mtx_recursed((mp)))						\
		(mp)->mtx_recurse--;					\
	else {								\
		LOCKSTAT_PROFILE_RELEASE_LOCK(spin__release, mp);	\
		(mp)->mtx_lock = MTX_UNOWNED;				\
	}								\
	spinlock_exit();						\
} while (0)

#define __lkpi_mtx_lock(mp, tid, opts, file, line) do {			\
	uintptr_t _tid = (uintptr_t)(tid);				\
									\
	if (((mp)->mtx_lock != MTX_UNOWNED || !_mtx_obtain_lock((mp), _tid))) {	\
		struct thread *td = curthread;				\
									\
		if (td->td_critnest || td->td_intr_nesting_level)	\
			_mtx_lock_spin_cookie(&(mp)->mtx_lock, _tid, (opts), (file), (line)); \
		else							\
			__mtx_lock_sleep(&(mp)->mtx_lock, _tid, (opts), (file), (line)); \
	} else								\
		LOCKSTAT_PROFILE_OBTAIN_LOCK_SUCCESS(adaptive__acquire,	\
		    mp, 0, 0, file, line);				\
} while (0)

#define lkpi_mtx_lock_spin(mp)		__lkpi_mtx_lock_spin((mp), curthread, 0, __FILE__, __LINE__)
#define lkpi_mtx_unlock_spin(mp)	__lkpi_mtx_unlock_spin((mp))


#define lkpi_mtx_lock(mp)		__lkpi_mtx_lock((mp), curthread, 0, __FILE__, __LINE__)
#define lkpi_mtx_unlock(mp)		__lkpi_mtx_unlock((mp))


#endif

#endif /* SMP */

#endif /* _LINUX_LKPI_MUTEX_H_ */
