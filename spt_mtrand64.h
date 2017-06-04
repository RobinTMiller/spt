#if !defined(spt_mtrand64_h)
#define spt_mtrand64_h 1

extern void init_genrand64(unsigned long long seed);
extern void init_by_array64(unsigned long long init_key[], unsigned long long key_length);
extern unsigned long long genrand64_int64(void);
extern long long genrand64_int63(void);
extern double genrand64_real1(void);
extern double genrand64_real2(void);
extern double genrand64_real3(void);

#endif /* !defined(spt_mtrand64_h) */
