#ifndef _MMU_H_
#define _MMU_H_

/* The memory management unit can return one of four things: success,
 * page-fault, protection-fault, or not-implemented.  
 * 1) If the resolution was successful, the variable pa holds the physical
 * address.  
 * 2)If there was a page fault, vpn holds the virtual page number.  
 * 3)If there was a protection fault, pte is a copy of the page table entry.
 */
typedef
struct result_st{
  enum {SUCCESS, 
        PAGEFAULT, 
        PROTFAULT,
        NOTIMPLEMENTED} status; 
  union {
    void* pa;
    unsigned vpn;
    unsigned pte;
  } value;
} result_t;

/* 32-bit addressing mode checks permissions.  This type enumerates the three
 * possible ways a program could access memory.  NOTE: Okay, we know that these
 * are, in fact, integers; use the symbols.
 */
typedef
     /* 0     1       2   */
enum {READ, WRITE, EXECUTE} access_t;

/* 16-bit legacy mode.
 */
result_t mmu_legacy(unsigned short va);

/* 32-bit mode with protection.
 */
result_t mmu_resolve(unsigned va, access_t use);


#endif /*_MMU_H_*/
