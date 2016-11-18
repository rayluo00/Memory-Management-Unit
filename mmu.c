#include "mmu.h"
#include <stdio.h>

/* I'm defining the CR3 "register" as a void*, but you are free to redefine it
 * as necessary.  Keep in mind that whatever type you use, it MUST be a
 * pointer.  However, the linker will not verify this.  If you redefine this
 * global as some non-pointer type, your program will probably crash.
 */
extern void* CR3;

/* The supervisor mode indicates that the processor is running in the operating
 * system.  If the permission bits are used, e.g., not in legacy mode, then
 * accessing a privileged page is only valid when SUPER is true.  Otherwise, it
 * is a protection fault.
 */
extern int  SUPER;

/* The page table is the same for both 16-bit and 32-bit addressing modes:
 *
 *  31    30..28 27..............................................4 3 2 1 0
 * +-----+------+-------------------------------------------------+-+-+-+-+
 * |Valid|unused| 24-bit Physical Page Number                     |P|R|W|X|
 * +-----+------+-------------------------------------------------+-+-+-+-+
 *
 * Unlike Intel, which uses a 4k (12-bit) page, this system uses a much smaller,
 * 256-byte (8-bit) page.
 */

int tlb_search(unsigned char index, unsigned short tag, unsigned int** pte);
int tlb_add (unsigned char index, unsigned short tag, unsigned int pte);

/*
  Retrieve the virtual page number from a 16-bit adress.
 */
static
unsigned int vpn(unsigned int virtAdress){
  return (virtAdress >> 8) & 0xFFFFFF;
}

/*
  Check if the page table entry is valid or invalid for either a SUCCESS or Page Fault.
 */
static
int check_pte_valid(unsigned int pageTabEnt){ 
  return ((pageTabEnt >> 31) & 1);
}

/*
  Make a address for the page table entry and virtual address, return the address of page.
 */
static
void* makeAddress(unsigned int pageTabEnt, unsigned int virtAddress){
  unsigned int phyPageNum = ((pageTabEnt >> 4) & 0xFFFFFF);
  unsigned int pageOff = virtAddress & 0xFF;
  return (void*)((phyPageNum << 8) | pageOff);
}

/* 16-bit legacy mode.
 * In legacy mode, CR3 points to an array of PTE.  Since there are only 256
 * pages, this array is rather small (1k).  Legacy mode doesn't enforce
 * permissions; every page is assumed to be read-write-executed regardless of
 * the permission bits.
 */
result_t mmu_legacy(unsigned short va)
{
  result_t result = {NOTIMPLEMENTED};
  unsigned int *pageTable = (unsigned int*)(CR3);
  unsigned int pageEntry = pageTable[vpn(va)];
  if(check_pte_valid(pageEntry)){
    result.status = SUCCESS;
    result.value.pa = makeAddress(pageEntry, va);
  }
  else{
    result.status = PAGEFAULT;
    result.value.vpn = vpn(va);
  }
  return result;
}

/* In 32-bit mode, CR3 points to an array of 256 directory pointers.  The format
 * of a directory pointer is the following:
 *
 *  31......................................................4 3....1     0
 * +---------------------------------------------------------+------+-----+
 * | Address of page table directory                         |Unused|Valid|
 * +---------------------------------------------------------+------+-----+
 *
 * Note that only 24-bits are needed for the pointer.  Each directory starts on
 * a page boundary, which means the four lest significant bits are always zero.
 * This is where we hide the valid bit.
 *
 * Each directory is an array of pointers with the same format above.  These
 * pointers, however, point at page tables.  Decoding a 32-bit address looks
 * like this:
 *
 *  31...........24 23..............16 15....................8 7.........0
 * +---------------+------------------+-----------------------+-----------+
 * |Directory Index| Page Table Index |Page Table Entry Index |Page Offset|
 * +---------------+------------------+-----------------------+-----------+
 *       |                  |                 |                     |
 * CR3   |   Root Dir.      |   Directory     |    Page Table       ++
 *  |    |   +------+       |   +------+      |     +------+         |
 *  |    |   |      |       |   |      |      |     |      |         v
 *  |    |   |      |       |   |      |      |     |      |
 *  |    +-> |======|--+    +-> |======|--+   +---> |======|---> PPN|PO = PA
 *  |        |      |  |        |      |  |         |      |     
 *  |        |      |  |        |      |  |         |      |     
 *  |        |      |  |        |      |  |         |      |
 *  |        |      |  |        |      |  |         |      |
 *  +------> +------+  +------> +------+  +-------> +------+
 *
 * Of course, you must check the valid bit at each level.  To be clear, the
 * diagrams are "upside-down" (i.e., little address are at the bottom).  
 */

/*Check for directory validity.*/
static
unsigned int valid_dir(unsigned int dirAddress){
  return dirAddress & 1;
}

/*Obtain the directory index from root directory pointer.*/
static
unsigned int get_dirI(unsigned int virtalAddress){
  return (virtalAddress >> 24) & 0xFF;
}

/*Get the page index from the directory pointer.*/
static
unsigned int get_ptIndex(unsigned int virtalAddress){
  return (virtalAddress >> 16) & 0xFF;
}

/*Get the page index of the page table entry.*/
static
unsigned int get_pteIndex(unsigned int virtalAddress){
  return (virtalAddress >> 8) & 0xFF;
}

/*Get the address from the pointer directories.*/
static
unsigned int get_addr(unsigned int virtalAddress){
  return (virtalAddress >> 4) << 4;
}

/*Check if the page has the execution bit set to 1.*/
static
int is_exec(unsigned int page){
    return (page & 1);
}

/*Check if the page is read only and not writable.*/
static
int read_only(unsigned int page){
  if((page & 0x6) == 0x4) {
    return 1;
  }
  return 0;
}

/*Check if the page has the permission bit set to 1.*/
static
int has_perm(unsigned int page){
  return ((page >> 3) & 1);
}

/*Get the TLB index from the virtual page number.*/
static
unsigned int tlbIndex(unsigned int vpn){
  return ((vpn) & 0xFF);
}

/*Get the TLB tag from the virtual page number*/
static
unsigned int tlbTag(unsigned int vpn){
  return ((vpn >> 8) & 0xFFFF);
}


/* Call functions for the specific faults or success.*/
static
result_t callProtFault(result_t result, unsigned int page){
  result.status = PROTFAULT;
  result.value.pte = page;
  return result;
}

/*If the page is invalid, call a page fault.*/
static
result_t callPageFault(result_t result, unsigned va){
  result.status = PAGEFAULT;
  result.value.vpn = vpn(va);
  return result;
}

/*Page was found and valid, return the result as successful.*/
static
result_t callSuccess(result_t result, unsigned int va, unsigned int page){
  result.status = SUCCESS;
  result.value.pa = makeAddress(page, va);
  return result;
}

/*Check the TLB page for protection faults*/
static
int call_faults(access_t use, unsigned int page){
  if(!SUPER && has_perm(page)){
    return 1;	  
  }
  else if(!is_exec(page) && use == 2){
    return 1;
  }
  else if(read_only(page) && use == 1){
    return 1;
  }
  return 0;
}


/* 32-bit mode with protection.
 * 
 * Start with the TLB search for the page in the cache, if search fails, find page in memory. Retrieve
 * valid addresses from directory to obtain the page from the Page Table. Also check for corresponding 
 * faults if the check fails.
 */

result_t mmu_resolve(unsigned va, access_t use)
{
    /*TLB Check*/
  result_t result = {NOTIMPLEMENTED};
  unsigned int vpnVal = vpn(va);
  unsigned int tIndex = tlbIndex(vpnVal);
  unsigned int tTag = tlbTag(vpnVal);
  unsigned int *tlb_pte;
  
  if (tlb_search(tIndex, tTag, &tlb_pte)){
    unsigned int pteCheck = (unsigned int)tlb_pte;
    
    if(call_faults(use, pteCheck)){
      result = callProtFault(result, pteCheck);
    }
    else{
      tlb_add(tIndex, tTag, (unsigned int)tlb_pte);
      result = callSuccess(result, va, pteCheck);
    }
  }
  
  else{
    unsigned int *directoryRoot = (unsigned int*)(CR3);
    unsigned int dir_index = directoryRoot[get_dirI(va)];

    unsigned int *directory = (unsigned int*) get_addr(dir_index);
    unsigned int page_index = directory[get_ptIndex(va)];

    unsigned int *pte = (unsigned int*) get_addr(page_index);
    unsigned int page = pte[get_pteIndex(va)];
    
    /*Check validity of addresses*/
    if(valid_dir(dir_index)){

      if(valid_dir(page_index)){
      
	/*32-bit Address Fault & Success Check*/
	if(check_pte_valid(page)){
	  if(!SUPER && has_perm(page)){
	    result = callProtFault(result, page);
	  }
	  else if(!is_exec(page) && use == 2){
	    result = callProtFault(result, page);
	  }
	  else if(read_only(page) && use == 1){
	    result = callProtFault(result, page);
	  }
	  else{
	    tlb_add(tIndex, tTag, page);
	    result = callSuccess(result, va, page);
	  }
	}
	else{
	  result = callPageFault(result, va);
	}
      }
    }
  }
  
  return result;
}
