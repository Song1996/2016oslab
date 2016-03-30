/* See COPYRIGHT for copyright information. */

#include "./include/jos/x86.h"
#include "./include/jos/mmu.h"
#include "./include/jos/error.h"
#include "./include/jos/string.h"
#include "./include/jos/assert.h"

#include "./include/jos/pmap.h"
#include "./include/jos/kclock.h"
extern void printk(const char*,...);
// These variables are set by i386_detect_memory()
size_t npages=1<<15;			// Amount of physical memory (in pages)
static size_t npages_basemem;	// Amount of base memory (in pages)

// These variables are set in mem_init()
pde_t *kern_pgdir;		// Kernel's initial page directory
struct PageInfo *pages;		// Physical page state array
static struct PageInfo *page_free_list;	// Free list of physical pages

static void boot_map_region(pde_t *pgdir, uintptr_t va, size_t size, physaddr_t pa, int perm);

unsigned mc146818_read(unsigned reg){
	outb(0x070,reg);
	return inb(0x070+1);
}


static int
nvram_read(int r)
{
	return mc146818_read(r) | (mc146818_read(r + 1) << 8);
}

static void
i386_detect_memory(void)
{
	size_t npages_extmem;

	// Use CMOS calls to measure available base & extended memory.
	// (CMOS calls return results in kilobytes.)
	npages_basemem = (nvram_read(NVRAM_BASELO) * 1024) / PGSIZE;
	npages_extmem = (nvram_read(NVRAM_EXTLO) * 1024) / PGSIZE;

	// Calculate the number of physical pages available in both base
	// and extended memory.
	if (npages_extmem)
		npages = (EXTPHYSMEM / PGSIZE) + npages_extmem;
	else
		npages = npages_basemem;

	printk("Physical memory: %dK available, base = %dK, extended = %dK,pagenum=%d\n",
		npages * PGSIZE / 1024,
		npages_basemem * PGSIZE / 1024,
		npages_extmem * PGSIZE / 1024,
		npages);
}




static void *
boot_alloc(uint32_t n)
{
	static char *nextfree;	// virtual address of next byte of free memory
	char *result;
	if (!nextfree) {
		extern char end[];
		nextfree = ROUNDUP((char *) end, PGSIZE);
	}
	char* ans;
	ans = nextfree;
	nextfree+=ROUNDUP(n,PGSIZE);
	return ans;
}

void
mem_init(void)
{
	uint32_t cr0;
	size_t n;

	i386_detect_memory();

	kern_pgdir = (pde_t *) boot_alloc(PGSIZE);
	
	memset(kern_pgdir, 0, PGSIZE);
	kern_pgdir[PDX(UVPT)] = PADDR(kern_pgdir) | PTE_U | PTE_P;


	pages=(struct PageInfo*)boot_alloc(npages*sizeof(struct PageInfo));
	
	printk("pages	%x\n",pages);
	
	page_init();

	struct PageInfo *pp1,*pp2,*pp;
	struct PageInfo **tp[2]={&pp1,&pp2};
	for(pp=page_free_list;pp;pp=pp->pp_link){
		int pagetype = (PDX(page2pa(pp)) >= 1024);
		*tp[pagetype] = pp;
		tp[pagetype] = &pp->pp_link;
	}
	*tp[1]=0;
	*tp[0]=pp2;
	page_free_list=pp1;
	printk("Hi%x\n",page2pa(pp1));

	page_alloc(ALLOC_ZERO);
	page_alloc(ALLOC_ZERO);
	page_alloc(ALLOC_ZERO);
	page_alloc(ALLOC_ZERO);
	page_alloc(ALLOC_ZERO);
	page_alloc(ALLOC_ZERO);
	page_alloc(ALLOC_ZERO);

	while(1);
	extern pde_t* entry_pgdir;
	
	boot_map_region(entry_pgdir,0xd0000000,0x1000,0x400000,PTE_P|PTE_W|PTE_U);
	unsigned i;
	for(i=0xd0000000;i<(0xd000000-0x1000);i+=4){
		*(int*)i=0x9c9c9c9c;
		printk("%x ",*(int*)i);
	}
	
	
/*
	check_page_free_list(1);
	check_page_alloc();
	check_page();
*/
	//////////////////////////////////////////////////////////////////////
	// Now we set up virtual memory

	//////////////////////////////////////////////////////////////////////
	// Map 'pages' read-only by the user at linear address UPAGES
	// Permissions:
	//    - the new image at UPAGES -- kernel R, user R
	//      (ie. perm = PTE_U | PTE_P)
	//    - pages itself -- kernel RW, user NONE
	// Your code goes here:


	boot_map_region(kern_pgdir,
				UPAGES,
				ROUNDUP((sizeof(struct PageInfo)*npages),PGSIZE),
				PADDR(pages),PTE_U);
	//page_alloc(ALLOC_ZERO);
//	while(1);
	//for(int i=0;i<ROUNDUP(npages*sizeof(struct PageInfo),PGSIZE);i+=PGSIZE)
	 // page_insert(kern_pgdir,pa2page(PADDR(pages)+i),(void*)(UPAGES+i),PTE_U);

	//////////////////////////////////////////////////////////////////////
	// Use the physical memory that 'bootstack' refers to as the kernel
	// stack.  The kernel stack grows down from virtual address KSTACKTOP.
	// We consider the entire range from [KSTACKTOP-PTSIZE, KSTACKTOP)
	// to be the kernel stack, but break this into two pieces:
	//     * [KSTACKTOP-KSTKSIZE, KSTACKTOP) -- backed by physical memory
	//     * [KSTACKTOP-PTSIZE, KSTACKTOP-KSTKSIZE) -- not backed; so if
	//       the kernel overflows its stack, it will fault rather than
	//       overwrite memory.  Known as a "guard page".
	//     Permissions: kernel RW, user NONE
	// Your code goes here:


	boot_map_region(kern_pgdir,0xc008000,0x8000,0x8000,PTE_W);
	//for(int i=0;i<KSTKSIZE;i+=PGSIZE)
	 // page_insert(kern_pgdir,pa2page(PADDR(bootstack)+i),(void*)(KSTACKTOP-KSTKSIZE+i),PTE_W);

	//////////////////////////////////////////////////////////////////////
	// Map all of physical memory at KERNBASE.
	// Ie.  the VA range [KERNBASE, 2^32) should map to
	//      the PA range [0, 2^32 - KERNBASE)
	// We might not have 2^32 - KERNBASE bytes of physical memory, but
	// we just set up the mapping anyway.
	// Permissions: kernel RW, user NONE
	// Your code goes here:

	boot_map_region(kern_pgdir,
				0xc00a0000,
				0x100000-0xa0000,
				0xa0000,PTE_W);
	/*
	for(int i=0;i<0xffffffff-KERNBASE;i+=PGSIZE){
		if(i<npages*PGSIZE){
			page_insert(kern_pgdir,pa2page(i),(void*)(KERNBASE+i),PTE_W);
			pa2page(i)->pp_ref--;
		} else {
			page_insert(kern_pgdir,pa2page(0),(void*)(KERNBASE+i),PTE_W);
			pa2page(0)->pp_ref--;
		}
	}
*/
	// Check that the initial page directory has been set up correctly.
/*	check_kern_pgdir();*/
	// Switch from the minimal entry page directory to the full kern_pgdir
	// page table we just created.	Our instruction pointer should be
	// somewhere between KERNBASE and KERNBASE+4MB right now, which is
	// mapped the same way by both page tables.
	//
	// If the machine reboots at this point, you've probably set up your
	// kern_pgdir wrong.

	lcr3(PADDR(kern_pgdir));
	while(1);
	/*
	check_page_free_list(0);
*/
	// entry.S set the really important flags in cr0 (including enabling
	// paging).  Here we configure the rest of the flags that we care about.
	
	cr0 = rcr0();
	cr0 |= CR0_PE|CR0_PG|CR0_AM|CR0_WP|CR0_NE|CR0_MP;
	cr0 &= ~(CR0_TS|CR0_EM);
	lcr0(cr0);

	// Some more checks, only possible after kern_pgdir is installed.
/*	check_page_installed_pgdir();*/
}

// --------------------------------------------------------------
// Tracking of physical pages.
// The 'pages' array has one 'struct PageInfo' entry per physical page.
// Pages are reference counted, and free pages are kept on a linked list.
// --------------------------------------------------------------

//
// Initialize page structure and memory free list.
// After this is done, NEVER use boot_alloc again.  ONLY use the page
// allocator functions below to allocate and deallocate physical
// memory via the page_free_list.
//

void
page_init(void)
{
	// The example code here marks all physical pages as free.
	// However this is not truly the case.  What memory is free?
	//  1) Mark physical page 0 as in use.
	//     This way we preserve the real-mode IDT and BIOS structures
	//     in case we ever need them.  (Currently we don't, but...)
	//  2) The rest of base memory, [PGSIZE, npages_basemem * PGSIZE)
	//     is free.
	//  3) Then comes the IO hole [IOPHYSMEM, EXTPHYSMEM), which must
	//     never be allocated.
	//  4) Then extended memory [EXTPHYSMEM, ...).
	//     Some of it is in use, some is free. Where is the kernel
	//     in physical memory?  Which pages are already in use for
	//     page tables and other data structures?
	//
	// Change the code to reflect this.
	// NB: DO NOT actually touch the physical memory corresponding to
	// free pages!
/*
	size_t i;
	physaddr_t pa;
	for (i = 0; i < npages; i++) {
		if (i==0 ||	i == PGNUM(0x10000)) {
			pages[i].pp_ref = 1;
			pages[i].pp_link = NULL;
		} else if (i<npages_basemem) {
			pages[i].pp_ref = 0;
			pages[i].pp_link = page_free_list;
			page_free_list = &pages[i];
		} else if ((i<=(EXTPHYSMEM / PGSIZE))
					|| (i<(((uint32_t)boot_alloc(0) - KERNBASE)>>PGSHIFT))){
			pages[i].pp_ref++;
			pages[i].pp_link = NULL;
		} else {
			pages[i].pp_ref = 0;
			pages[i].pp_link = page_free_list;
			page_free_list = &pages[i];
		}

		pa = page2pa(&pages[i]);

		if ((pa == 0 || pa == IOPHYSMEM)&& (pages[i].pp_ref==0))
			printk("page error: i %d\n", i);
	}
*/
	
	size_t i;
	for (i = npages-1; i >= 1; i--) {	
		pages[i].pp_ref = 0;
		pages[i].pp_link = page_free_list;
		page_free_list = &pages[i];
	}
	extern char end[];
	pages[1].pp_link=0;	
	struct PageInfo* pgstart=pa2page((physaddr_t)IOPHYSMEM);
	struct PageInfo* pgend=pa2page((physaddr_t)(end-KERNBASE+PGSIZE+npages*sizeof(struct PageInfo)));
	pgend=pgend+1;
	pgstart=pgstart-1;	
	printk("%x	\n",pgend);
	pgstart->pp_link=pgend;

}

//
// Allocates a physical page.  If (alloc_flags & ALLOC_ZERO), fills the entire
// returned physical page with '\0' bytes.  Does NOT increment the reference
// count of the page - the caller must do these if necessary (either explicitly
// or via page_insert).
//
// Be sure to set the pp_link field of the allocated page to NULL so
// page_free can check for double-free bugs.
//
// Returns NULL if out of free memory.
//
// Hint: use page2kva and memset
struct PageInfo *
page_alloc(int alloc_flags)
{
	// Fill this function in
	printk("pagealloc!%x\n",alloc_flags);
	if(page_free_list==NULL)
	  return NULL;
	struct PageInfo* ans=page_free_list;
	page_free_list=ans;
	printk("pagepa %x\n",page2kva(ans));
	if(alloc_flags&ALLOC_ZERO)
	  memset(page2kva(ans),0,PGSIZE);
	printk("pagealloc ans %x\n",ans);	
	return ans;
}

//
// Return a page to the free list.
// (This function should only be called when pp->pp_ref reaches 0.)
//
void
page_free(struct PageInfo *pp)
{
	// Fill this function in
	// Hint: You may want to panic if pp->pp_ref is nonzero or
	// pp->pp_link is not NULL.
	pp->pp_link=page_free_list;
	page_free_list=pp;
}

//
// Decrement the reference count on a page,
// freeing it if there are no more refs.
//
void
page_decref(struct PageInfo* pp)
{
	if (--pp->pp_ref == 0)
		page_free(pp);
}

// Given 'pgdir', a pointer to a page directory, pgdir_walk returns
// a pointer to the page table entry (PTE) for linear address 'va'.
// This requires walking the two-level page table structure.
//
// The relevant page table page might not exist yet.
// If this is true, and create == false, then pgdir_walk returns NULL.
// Otherwise, pgdir_walk allocates a new page table page with page_alloc.
//    - If the allocation fails, pgdir_walk returns NULL.
//    - Otherwise, the new page's reference count is incremented,
//	the page is cleared,
//	and pgdir_walk returns a pointer into the new page table page.
//
// Hint 1: you can turn a Page * into the physical address of the
// page it refers to with page2pa() from kern/pmap.h.
//
// Hint 2: the x86 MMU checks permission bits in both the page directory
// and the page table, so it's safe to leave permissions in the page
// directory more permissive than strictly necessary.
//
// Hint 3: look at inc/mmu.h for useful macros that mainipulate page
// table and page directory entries.
//
pte_t *
pgdir_walk(pde_t *pgdir, const void *va, int create)
{
	// Fill this function in
	 pde_t * pde; //va(virtual address) point to pa(physical address)
	  pte_t * pgtable; //same as pde
	  struct PageInfo *pp;

	  pde = &pgdir[PDX(va)]; // va->pgdir
	  if(*pde & PTE_P) { 
	  	pgtable = (KADDR(PTE_ADDR(*pde)));
	  } else {
		//page table page not exist
		if(!create || 
		   !(pp = page_alloc(ALLOC_ZERO)) ||
		   !(pgtable = (pte_t*)page2kva(pp))) 
			return NULL;
		    
		pp->pp_ref++;
		*pde = PADDR(pgtable) | PTE_P | PTE_W | PTE_U;
	}

	return &pgtable[PTX(va)];
}

void*
pageinsert(pde_t *pgdir,const void *va,int create){
	printk("pageinsert!\n");
	struct PageInfo* ans=NULL;
	pte_t* pgtable = pgdir_walk(pgdir,va,create);
	if(pgtable==NULL)
	  return NULL;
	if((void*)pgtable[PTX(va)]==NULL){
		struct PageInfo* page = page_alloc(ALLOC_ZERO);
		page->pp_ref++;
		pgtable[PTX(va)]=page2pa(page)||PTE_P|PTE_W|PTE_U;
		ans = page2kva(page);
	}else{
		ans=page2kva(pa2page(pgtable[PTX(va)]));
	}
	printk("pageinsert va %x return %x\n",va,ans);
	return ans;
}




//
// Map [va, va+size) of virtual address space to physical [pa, pa+size)
// in the page table rooted at pgdir.  Size is a multiple of PGSIZE, and
// va and pa are both page-aligned.
// Use permission bits perm|PTE_P for the entries.
//
// This function is only intended to set up the ``static'' mappings
// above UTOP. As such, it should *not* change the pp_ref field on the
// mapped pages.
//
// Hint: the TA solution uses pgdir_walk
static void
boot_map_region(pde_t *pgdir, uintptr_t va, size_t size, physaddr_t pa, int perm)
{
	// Fill this function in
	uintptr_t va_next = va;
	physaddr_t pa_next = pa;
	pte_t* pte;
	physaddr_t pa_check;
	uint32_t np = size/PGSIZE;
	uint32_t i = 0;
	do {
		pte = pgdir_walk(pgdir,(void*)va_next,1);
		if(!pte)
		  return;
		*pte = (PTE_ADDR(pa_next)|perm|PTE_P);
		va_next += PGSIZE;
		pa_next += PGSIZE;
	} while(++i<np);
}

//
// Map the physical page 'pp' at virtual address 'va'.
// The permissions (the low 12 bits) of the page table entry
// should be set to 'perm|PTE_P'.
//
// Requirements
//   - If there is already a page mapped at 'va', it should be page_remove()d.
//   - If necessary, on demand, a page table should be allocated and inserted
//     into 'pgdir'.
//   - pp->pp_ref should be incremented if the insertion succeeds.
//   - The TLB must be invalidated if a page was formerly present at 'va'.
//
// Corner-case hint: Make sure to consider what happens when the same
// pp is re-inserted at the same virtual address in the same pgdir.
// However, try not to distinguish this case in your code, as this
// frequently leads to subtle bugs; there's an elegant way to handle
// everything in one code path.
//
// RETURNS:
//   0 on success
//   -E_NO_MEM, if page table couldn't be allocated
//
// Hint: The TA solution is implemented using pgdir_walk, page_remove,
// and page2pa.
//
int
page_insert(pde_t *pgdir, struct PageInfo *pp, void *va, int perm)
{
	// Fill this function in
	printk("page_insert\n");
	pte_t* pte;
	struct PageInfo* pg=page_lookup(pgdir,va,NULL);
	if(pg==pp){
		pte=pgdir_walk(pgdir,va,1);
		pte[0]=page2pa(pp)|perm|PTE_P;
		return 0;
	} else if(pg!=NULL)
		page_remove(pgdir,va);
	pte=pgdir_walk(pgdir,va,1);
	if(pte==NULL)
	  return -E_NO_MEM;
	pte[0]=page2pa(pp)|perm|PTE_P;
	pp->pp_ref++;
	return 0;
}

//
// Return the page mapped at virtual address 'va'.
// If pte_store is not zero, then we store in it the address
// of the pte for this page.  This is used by page_remove and
// can be used to verify page permissions for syscall arguments,
// but should not be used by most callers.
//
// Return NULL if there is no page mapped at va.
//
// Hint: the TA solution uses pgdir_walk and pa2page.
//
struct PageInfo *
page_lookup(pde_t *pgdir, void *va, pte_t **pte_store)
{
	// Fill this function in
	printk("page lookup\n");
	pte_t* pte=pgdir_walk(pgdir,va,0);
	if(pte==NULL)
	  return NULL;
	if(pte_store!=0)
	  *pte_store=pte;
	if(pte[0]!=(pte_t)NULL)
	  return pa2page(PTE_ADDR(pte[0]));
	else
		return NULL;
}

//
// Unmaps the physical page at virtual address 'va'.
// If there is no physical page at that address, silently does nothing.
//
// Details:
//   - The ref count on the physical page should decrement.
//   - The physical page should be freed if the refcount reaches 0.
//   - The pg table entry corresponding to 'va' should be set to 0.
//     (if such a PTE exists)
//   - The TLB must be invalidated if you remove an entry from
//     the page table.
//
// Hint: The TA solution is implemented using page_lookup,
// 	tlb_invalidate, and page_decref.
//
void
page_remove(pde_t *pgdir, void *va)
{
	// Fill this function in
	printk("page_remove\n");
	pte_t*	pte=0;
	struct PageInfo* page=page_lookup(pgdir,va,&pte);
	if(page!=NULL)
	  page_decref(page);
	pte[0]=0;
	tlb_invalidate(pgdir,va);
}

//
// Invalidate a TLB entry, but only if the page tables being
// edited are the ones currently in use by the processor.
//
void
tlb_invalidate(pde_t *pgdir, void *va)
{
	// Flush the entry only if we're modifying the current address space.
	// For now, there is only one address space, so always invalidate.
	
	invlpg(va);
}


