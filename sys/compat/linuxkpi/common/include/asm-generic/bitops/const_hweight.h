#ifndef _ASM_GENERIC_BITOPS_CONST_HWEIGHT_H_
#define _ASM_GENERIC_BITOPS_CONST_HWEIGHT_H_



static __inline u_long
__arch_hweight8(u_long mask)
{
        u_long result;

        __asm __volatile("popcntq %1,%0" : "=r" (result) : "rm" (mask));
        return (result);
}


#define __const_hweight8(w)		\
	((unsigned int)			\
	 ((!!((w) & (1ULL << 0))) +	\
	  (!!((w) & (1ULL << 1))) +	\
	  (!!((w) & (1ULL << 2))) +	\
	  (!!((w) & (1ULL << 3))) +	\
	  (!!((w) & (1ULL << 4))) +	\
	  (!!((w) & (1ULL << 5))) +	\
	  (!!((w) & (1ULL << 6))) +	\
	  (!!((w) & (1ULL << 7)))))

#define bitcount8(w) (__builtin_constant_p(w) ? __const_hweight8(w)  : __arch_hweight8(w))
#define hweight8(x) bitcount8(x) 
#define hweight16(x) bitcount16(x)
#define hweight32(x) bitcount32(x)
#define hweight64(x) (bitcount32((uint32_t)(x>>32)) + bitcount32((uint32_t)x))


#endif /* _ASM_GENERIC_BITOPS_CONST_HWEIGHT_H_ */
