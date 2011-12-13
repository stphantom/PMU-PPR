/* !!! must have patched kernel with 0x333 syscall */

inline void set_ppr(unsigned long long x)
{

    __asm__ volatile ("li 0, 0x333\n\t" "ld 3, %0 \n\t" "sc \n\t"::
		      "m" (x):"3");

}

inline int get_ppr()
{
    unsigned long long ret;
    __asm__ volatile ("mfspr 9, 896\n\t" "std 9, %0\n\t":"=m" (ret)::"9");

    return (int) ((ret >> 50) & 0x7);
}
