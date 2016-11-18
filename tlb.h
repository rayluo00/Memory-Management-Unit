#ifndef _TLB_H_
#define _TLB_H_

/* Flush
 * This function clears the TLB's internal data structures.  Typically, this is
 * called at start-up time and whenever there is a task swap (e.g., CR3 is
 * changed).  
 */
void tlb_flush();

/* Search
 * If this function returns true, then the page table entry was found in the TLB
 * and the pte argument is updated with that information.  Otherwise, the entry
 * is not in the TLB and this function returns false.
 */
int tlb_search(unsigned char index, unsigned short tag, unsigned int* pte);

/* Add
 * This adds a new entry to the TLB.  This could remove an exiting entry in the
 * same index.  The only error is if the page table is already in the TLB.
 * Adding the same tag twice to an index breaks content-addressable-memory.  
 */
int tlb_add(unsigned char index, unsigned short tag, unsigned int pte);

#endif /* _TLB_H_ */
