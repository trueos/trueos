#ifndef __ASM_GENERIC_ATOMIC_H
#define __ASM_GENERIC_ATOMIC_H

#ifdef SMP
#define ATOMIC_OP(op, c_op)                                             \
static inline void atomic_##op(int i, atomic_t *v)                      \
{                                                                       \
        int c, old;                                                     \
                                                                        \
        c = v->counter;                                                 \
        while ((old = cmpxchg(&v->counter, c, c c_op i)) != c)          \
                c = old;                                                \
}
#else
#define ATOMIC_OP(op, c_op)                                             \
static inline void atomic_##op(int i, atomic_t *v)                      \
{                                                                       \
	critical_enter();						\
        v->counter = v->counter c_op i;                                 \
	critical_exit();						\
}
#endif

#ifndef atomic_or
ATOMIC_OP(or, |)
#endif

#define atomic_cmpxchg(v, old, new)	(cmpxchg(&((v)->counter), (old), (new)))

#endif
