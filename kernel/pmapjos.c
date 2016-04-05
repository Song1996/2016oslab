/* See COPYRIGHT for copyright information. */

#include "./include/jos/x86.h"
#include "./include/jos/mmu.h"
#include "./include/jos/error.h"
#include "./include/jos/string.h"
#include "./include/jos/env.h"
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
	npages_basemem = (nvram_read(NVRAM_BASELO) * 1024) / PGSIZE;
	npages_extmem = (nvram_read(NVRAM_EXTLO) * 1024) / PGSIZE;
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
	char* result;
	if (!nextfree) {
		extern char end[];
		nextfree = ROUNDUP((char *) end, PGSIZE);
	}
	result = nextfree;
	nextfree+=((ROUNDUP(n,PGSIZE))>>PGSHIFT)*PGSIZE;
	return result;
}

void
mem_init(void)
{
	uint32_t cr0;	
	i386_detect_memory();
	kern_pgdir = (pde_t *) boot_alloc(PGSIZE);
	memset(kern_pgdir, 0, PGSIZE);
	pages=(struct PageInfo*)boot_alloc(npages*sizeof(struct PageInfo));
	extern struct Env* envs;
	envs = (struct Env*) boot_alloc(NENV * sizeof(struct Env));
	printk("envs start %x\n",envs);
	printk("envs end %x\n",&envs[NENV-1]);
	memset(envs,0,NENV * sizeof(struct Env));
	printk("pages	%x\n",pages);
	page_init();
	boot_map_region(kern_pgdir,
				0xc0000000,
				0xc0400000-0xc0000000,
				0x0,PTE_W|PTE_U);
	boot_map_region(kern_pgdir,
				0x00000,
				0x400000,
				0x00000,PTE_W|PTE_U);
	printk("ready to cr3?\n");
	lcr3(PADDR(kern_pgdir));
	printk("hellowold\n");
	//while(1);
	cr0 = rcr0();
	cr0 |= CR0_PE|CR0_PG|CR0_AM|CR0_WP|CR0_NE|CR0_MP;
	cr0 &= ~(CR0_TS|CR0_EM);
	lcr0(cr0);
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
	extern char end[];

	printk("pageinit %x\n",page_free_list);
	printk(" the start page %x\n",pa2page((physaddr_t)(end-0xc0000000+PGSIZE+npages*sizeof(struct PageInfo)+ROUNDUP(sizeof(struct Env)*1024,PGSIZE))) );
	printk(" the end page %x\n",&pages[npages-1]);
	size_t i = npages-1;	
	while( page_free_list!=pa2page((physaddr_t)(end-KERNBASE+PGSIZE+npages*sizeof(struct PageInfo)+ROUNDUP(sizeof(struct Env)*NENV,PGSIZE))) ){	
		pages[i].pp_ref = 0;
		pages[i].pp_link = page_free_list;
		page_free_list = &pages[i];
		i--;
	}
	page_free_list = page_free_list -> pp_link;
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
	if(page_free_list==NULL)
	  return NULL;
	struct PageInfo* ans=page_free_list;
	page_free_list=page_free_list->pp_link;
	if(alloc_flags&ALLOC_ZERO)
	  memset(page2kva(ans),0,PGSIZE);
	//printk("pagealloc%x\n",ans);
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
	if (--(pp->pp_ref) == 0)
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
	pde_t * pde;
	pte_t * pte;
	struct PageInfo *pp;
	/*if((unsigned)va==0x8048000)
	  printk("pgdir_walk1\n");*/

	pde = &pgdir[PDX(va)]; // va->pgdir
/*	if((unsigned)va==0x8048000)
	  while(1);*/
	if(*pde & PTE_P) { 
		pte = (KADDR(PTE_ADDR(*pde)));
	} else {
		if(!create || !(pp = page_alloc(1)) || !(pte = (pte_t*)page2kva(pp))) 
			return NULL;
		pp->pp_ref++;
		*pde = PADDR(pte) | PTE_P | PTE_W | PTE_U;
	}

	return &pte[PTX(va)];
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
	//printk("boot_map_region\n");
	pte_t* pte;
	uint32_t np = size/PGSIZE;
	for(int i=0;i<np;i++){
		pte = pgdir_walk(pgdir,(void*)va,1);
		if(pte==NULL)printk("bootmapfail!\n");
		*pte = (PTE_ADDR(pa)|perm|PTE_P);
		va+=PGSIZE;
		pa+=PGSIZE;
	}
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
	//printk("page_insert\n");
	
	pte_t *pte = pgdir_walk(pgdir,va,0);
	//printk("insert	pte %x\n",pte);
	if(pte!=NULL){
		if(*pte & PTE_P)
			page_remove(pgdir,va);
		if(page_free_list==pp)
		  page_free_list=page_free_list->pp_link;
	} else {
		//printk("b\n");
		pte = pgdir_walk(pgdir,va,1);
		if(!pte)
		  return -E_NO_MEM;
	}
	//while(1);
	*pte = page2pa(pp)|perm|PTE_P;
	pp->pp_ref++;
	tlb_invalidate(pgdir,va);
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
	//printk("page lookup\n");
	pte_t* pte=pgdir_walk(pgdir,va,0);
	if(pte==NULL)
	  return NULL;
	if(pte_store!=NULL)
	  *pte_store=pte;
	if(*pte!=(pte_t)NULL)
	  return pa2page(PTE_ADDR(*pte));
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
	*pte=0;
	tlb_invalidate(pgdir,va);
}

void
tlb_invalidate(pde_t *pgdir, void *va){	
	invlpg(va);
}

pte_t* ucr3;

void region_alloc(pde_t* pgdir, void *va,size_t len){
	//pgdir[PDX(va)]=page_alloc(0);
	void* begin = (void*) ROUNDDOWN(va,PGSIZE);
	void* end = (void*) ROUNDUP(va+len,PGSIZE);
	while(begin<end){
		struct PageInfo* page = page_alloc(0);	
		page_insert(pgdir,page,begin,PTE_U|PTE_W);	
		begin+=PGSIZE;
	}
}



