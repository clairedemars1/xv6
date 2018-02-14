#include "param.h"
#include "types.h"
#include "defs.h"
#include "x86.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "elf.h"

extern char data[];  // defined by kernel.ld
pde_t *kpgdir;  // for use in scheduler()

// Set up CPU's kernel segment descriptors.
// Run once on entry on each CPU.
void
seginit(void)
{
  struct cpu *c;

  // Map "logical" addresses to virtual addresses using identity map.
  // Cannot share a CODE descriptor for both kernel and user
  // because it would have to have DPL_USR, but the CPU forbids
  // an interrupt from CPL=0 to DPL=3.
  c = &cpus[cpuid()];
  c->gdt[SEG_KCODE] = SEG(STA_X|STA_R, 0, 0xffffffff, 0);
  c->gdt[SEG_KDATA] = SEG(STA_W, 0, 0xffffffff, 0);
  c->gdt[SEG_UCODE] = SEG(STA_X|STA_R, 0, 0xffffffff, DPL_USER);
  c->gdt[SEG_UDATA] = SEG(STA_W, 0, 0xffffffff, DPL_USER);
  lgdt(c->gdt, sizeof(c->gdt));
}

// Return the address of the PTE in page table pgdir
// that corresponds to virtual address va.  If alloc!=0,
// create any required page table pages.
// (claire: it does not allocate physical memory or set the pte address to point to physical memory)
// (claire is this (the address of) the entry in the page directory or of the entry in the underlying page table? )
static pte_t *
walkpgdir(pde_t *pgdir, const void *va, int alloc)
{
	// A virtual address 'la' has a three-part structure as follows:
	//
	// +--------10------+-------10-------+---------12----------+
	// | Page Directory |   Page Table   | Offset within Page  |
	// |      Index     |      Index     |                     |
	// +----------------+----------------+---------------------+
	//  \--- PDX(va) --/ \--- PTX(va) --/
				
  pde_t *pde; // pointer to page directory entry (*not* to page table entry)
				// note: it's sorta a double pointer, since the entry itself is a pointer with some extra status bits
  pte_t *pgtab; // pointer to page table entry (entry not whole table: so could be called pte)

  pde = &pgdir[PDX(va)]; // get pde: get the page dir index for that virtual address, look there in the page dir
  // this is a pointer to the right place in the page directory
  // i think the order is &( pgdir[PDX(va)] )
  // it's a pointer so you can check it's flags
  if(*pde & PTE_P){ // go to the right place in the page directory, if any pages are allocated in it
    pgtab = (pte_t*)P2V(PTE_ADDR(*pde)); 
    // get the (physical) address of the beginning of the page of pg *table* entries
    //  convert it to virtual address
  } else { // the page directory entry is not filled in, so we need to fill it in
    
    if(!alloc || ( pgtab = (pte_t*)kalloc() ) == 0)
      return 0;
    // Make sure all those PTE_P bits are zero.
    memset(pgtab, 0, PGSIZE);
    // The permissions here are overly generous, but they can
    // be further restricted by the permissions in the page table
    // entries, if necessary.
    *pde = V2P(pgtab) | PTE_P | PTE_W | PTE_U;
  }
  return &pgtab[PTX(va)]; // pgtab is a pointer to a page-worth of pte,
  // so get the page part of the virtual address, 
  // and return a pointer to the right entry within it 
  // which has the address of the physical memory (if actually set) and the presence bit, which we haven't actually checked 
}

// Create PTEs for virtual addresses starting at va that refer to
// physical addresses starting at pa and going to pg + size (page aligned).
// va and size might not be page-aligned.
// perm is permissions
static int
mappages(pde_t *pgdir, void *va, uint size, uint pa, int perm)
{
  char *a, *last;
  pte_t *pte;

  a = (char*)PGROUNDDOWN((uint)va);
  last = (char*)PGROUNDDOWN(((uint)va) + size - 1);
  for(;;){
    if((pte = walkpgdir(pgdir, a, 1)) == 0) // try to create the pte
      return -1;
    if(*pte & PTE_P)
      panic("remap"); // if the pte had already been mapped to physical memory, throw error
    *pte = pa | perm | PTE_P; // combine the physical address with the flags, using bitwise or, to make the pte
    if(a == last) // basically the while loop condition, so it allocates all necessary pages
      break; 
    a += PGSIZE;
    pa += PGSIZE;
  }
  return 0;
}

// There is one page table per process, plus one that's used when
// a CPU is not running any process (kpgdir). The kernel uses the
// current process's page table during system calls and interrupts;
// page protection bits prevent user code from using the kernel's
// mappings.
//
// setupkvm() and exec() set up every page table like this:
//
//   0..KERNBASE: user memory (text+data+stack+heap), mapped to
//                phys memory allocated by the kernel
//   KERNBASE..KERNBASE+EXTMEM: mapped to 0..EXTMEM (for I/O space)
//   KERNBASE+EXTMEM..data: mapped to EXTMEM..V2P(data)
//                for the kernel's instructions and r/o data
//   data..KERNBASE+PHYSTOP: mapped to V2P(data)..PHYSTOP,
//                                  rw data + free physical memory
//   0xfe000000..0: mapped direct (devices such as ioapic)
//
// The kernel allocates physical memory for its heap and for user memory
// between V2P(end) and the end of physical memory (PHYSTOP)
// (directly addressable from end..P2V(PHYSTOP)).

// This table defines the kernel's mappings, which are present in
// every process's page table.
static struct kmap {
  void *virt;
  uint phys_start;
  uint phys_end;
  int perm;
} kmap[] = {
 { (void*)KERNBASE, 0,             EXTMEM,    PTE_W}, // I/O space
 { (void*)KERNLINK, V2P(KERNLINK), V2P(data), 0},     // kern text+rodata
 { (void*)data,     V2P(data),     PHYSTOP,   PTE_W}, // kern data+memory
 { (void*)DEVSPACE, DEVSPACE,      0,         PTE_W}, // more devices
};

// Set up kernel part of a page table.
pde_t*
setupkvm(void)
{
  pde_t *pgdir;
  struct kmap *k;

  if((pgdir = (pde_t*)kalloc()) == 0)
    return 0;
  memset(pgdir, 0, PGSIZE);
  if (P2V(PHYSTOP) > (void*)DEVSPACE)
    panic("PHYSTOP too high");
  for(k = kmap; k < &kmap[NELEM(kmap)]; k++)
    if(mappages(pgdir, k->virt, k->phys_end - k->phys_start,
                (uint)k->phys_start, k->perm) < 0) {
      freevm(pgdir, myproc());
      return 0;
    }
  return pgdir;
}

// Allocate one page table for the machine for the kernel address
// space for scheduler processes.
void
kvmalloc(void)
{
  kpgdir = setupkvm();
  switchkvm();
}

// Switch h/w page table register to the kernel-only page table,
// for when no process is running.
void
switchkvm(void)
{
  lcr3(V2P(kpgdir));   // switch to the kernel page table
}

// Switch TSS and h/w page table to correspond to process p.
void
switchuvm(struct proc *p)
{
  if(p == 0)
    panic("switchuvm: no process");
  if(p->kstack == 0)
    panic("switchuvm: no kstack");
  if(p->pgdir == 0)
    panic("switchuvm: no pgdir");

  pushcli();
  mycpu()->gdt[SEG_TSS] = SEG16(STS_T32A, &mycpu()->ts,
                                sizeof(mycpu()->ts)-1, 0);
  mycpu()->gdt[SEG_TSS].s = 0;
  mycpu()->ts.ss0 = SEG_KDATA << 3;
  mycpu()->ts.esp0 = (uint)p->kstack + KSTACKSIZE;
  // setting IOPL=0 in eflags *and* iomb beyond the tss segment limit
  // forbids I/O instructions (e.g., inb and outb) from user space
  mycpu()->ts.iomb = (ushort) 0xFFFF;
  ltr(SEG_TSS << 3);
  lcr3(V2P(p->pgdir));  // switch to process's address space
  popcli();
}

// Load the initcode into address 0 of pgdir.
// sz must be less than a page.
void
inituvm(pde_t *pgdir, char *init, uint sz)
{
  char *mem;

  if(sz >= PGSIZE)
    panic("inituvm: more than a page");
  mem = kalloc();
  memset(mem, 0, PGSIZE);
  mappages(pgdir, 0, PGSIZE, V2P(mem), PTE_W|PTE_U);
  memmove(mem, init, sz);
}

// Load a program segment into pgdir.  addr must be page-aligned
// and the pages from addr to addr+sz must already be mapped.
int
loaduvm(pde_t *pgdir, char *addr, struct inode *ip, uint offset, uint sz)
{
  uint i, pa, n;
  pte_t *pte;

  if((uint) addr % PGSIZE != 0)
    panic("loaduvm: addr must be page aligned");
  for(i = 0; i < sz; i += PGSIZE){
    if((pte = walkpgdir(pgdir, addr+i, 0)) == 0)
      panic("loaduvm: address should exist");
    pa = PTE_ADDR(*pte);
    if(sz - i < PGSIZE)
      n = sz - i;
    else
      n = PGSIZE;
    if(readi(ip, P2V(pa), offset+i, n) != n)
      return -1;
  }
  return 0;
}

// Allocate page tables and physical memory to grow process from oldsz to
// newsz, which need not be page aligned.  Returns new size or 0 on error.
int
allocuvm(pde_t *pgdir, uint oldsz, uint newsz)
{
  char *mem;
  uint a; // virtual address

  if(newsz >= KERNBASE)
    return 0;
  if(newsz < oldsz)
    return oldsz;

  a = PGROUNDUP(oldsz);
  for(; a < newsz; a += PGSIZE){ // for page necessary to meet the requested amount of memory
    mem = kalloc(); // allocate a physical page, and get its *virtual* address
    if(mem == 0){
      cprintf("allocuvm out of memory\n");
      deallocuvm(pgdir, newsz, oldsz);
      return 0;
    }
    memset(mem, 0, PGSIZE); // initialize all memory to zero 
    // map the page to the physical address, giving it user and write permissions
    if(mappages(pgdir, (char*)a, PGSIZE, V2P(mem), PTE_W|PTE_U) < 0){ 
      cprintf("allocuvm out of memory (2)\n");
      deallocuvm(pgdir, newsz, oldsz);
      kfree(mem);
      return 0;
    }
  }
  return newsz;
}

// Deallocate user pages to bring the process size from oldsz to
// newsz.  oldsz and newsz need not be page-aligned, nor does newsz
// need to be less than oldsz.  oldsz can be larger than the actual
// process size.  Returns the new process size.
int
deallocuvm(pde_t *pgdir, uint oldsz, uint newsz)
{
  pte_t *pte;
  uint a, pa;

  if(newsz >= oldsz)
    return oldsz;

  a = PGROUNDUP(newsz);
  for(; a  < oldsz; a += PGSIZE){
    pte = walkpgdir(pgdir, (char*)a, 0);
    if(!pte)
      a = PGADDR(PDX(a) + 1, 0, 0) - PGSIZE;
    else if((*pte & PTE_P) != 0){
      pa = PTE_ADDR(*pte);
      if(pa == 0)
        panic("kfree");
      char *v = P2V(pa);
      kfree(v);
      *pte = 0;
    }
  }
  return newsz;
}

//  if user_va represents a shared page for the process, returns 1 
// and sets out_page_num to the page's identifier
// returns 0 and sets out_page_num to NSH
int is_shared_f(char* user_va, struct proc* process, int* out_page_num){
	*out_page_num = NSH;
	if (user_va == 0 ){ panic("0 virtual address"); }
	struct sh_pg* shared_pages = process->shared_pages; 
	int i;
	for(i=0; i<NSH; i++){
		if ( shared_pages[i].virtual_addr == user_va ){
			*out_page_num = i;
			return 1; // found shared page
		}
	}
	return 0; // did not find
}

// Free a page table and all the physical memory pages
// in the user part.
void
freevm(pde_t *pgdir, struct proc* process)
{
  //~ cprintf("\tfreevm called for pid %d and process %s\n", process->pid, process->name);
  uint i;

  if(pgdir == 0)
    panic("freevm: no pgdir");
    
  // all pages except top NSH-many
  deallocuvm(pgdir, KERNBASE-NSH*PGSIZE, 0); 
  
  // top NSH-many pages
  uint pa, va;
  pte_t* pte;
  int j;
  for(j=0; j<NSH; j++){ // for each page
    va = KERNBASE-(j+1)*PGSIZE;
    
    int shared_page_index;
	int is_shared = is_shared_f((char*)va, process, &shared_page_index); // shared_page_index is output param
	if (!is_shared ){ 
		// free iff is in use (since it's not shared, that can only happen with an almost full heap)
		pte = walkpgdir(pgdir, (char*)va, 0);
		if( pte && ((*pte & PTE_P) != 0 ) ){ // in use
		  pa = PTE_ADDR(*pte);
		  if(pa == 0)
			panic("kfree vm.c 347");
		  char *kernal_va = P2V(pa);
		  kfree(kernal_va);
		  //~ cprintf("setting pa to zero for pid %d (for unshared page)\n", process->pid);
		  *pte = 0; // avoid dangling pointer
		}						
	} else { // is shared
		
		// free shared pages with reference count == 0
		if ( (--(global_shared_pages[shared_page_index].reference_count)) == 0 ){ // decrement ref count for any shared page
			pa = (uint) global_shared_pages[shared_page_index].phys_addr; // we could also get this from the pte
			if (pa == 0)
				panic("shared page kfree");
			kfree( P2V(pa) );
			//~ cprintf("setting pa to zero for pid %d\n", process->pid);
			global_shared_pages[shared_page_index].phys_addr = 0; // avoid dangling pointer
		}
		
		// avoid dangling pointer
		pte = walkpgdir(pgdir, (char*)va, 0); 
		if (pte) *pte = 0;
	}
  } // end for
  
  // free page dir 
  for(i = 0; i < NPDENTRIES; i++){
    if(pgdir[i] & PTE_P){
      char * v = P2V(PTE_ADDR(pgdir[i]));
      kfree(v);
    }
  }
  kfree((char*)pgdir);
}

// Clear PTE_U on a page. Used to create an inaccessible
// page beneath the user stack.
void
clearpteu(pde_t *pgdir, char *uva)
{
  pte_t *pte;

  pte = walkpgdir(pgdir, uva, 0);
  if(pte == 0)
    panic("clearpteu");
  *pte &= ~PTE_U;
}

// Given a parent process's page table, create and return a copy
// of it for a child.
pde_t*
copyuvm(pde_t *parent_pgdir, uint sz, struct proc* child_proc)
{
  pde_t *d; // new directory 
  pte_t *pte;
  uint pa, i, flags;
  char *mem;

  if((d = setupkvm()) == 0)
    return 0;
    
  // copy regular stuff
  for(i = 0; i < sz; i += PGSIZE){
    if((pte = walkpgdir(parent_pgdir, (void *) i, 0)) == 0)
      panic("copyuvm: pte should exist");
    if(!(*pte & PTE_P))
      panic("copyuvm: page not present");
    pa = PTE_ADDR(*pte);
    flags = PTE_FLAGS(*pte);
    if((mem = kalloc()) == 0)
      goto bad;
    memmove(mem, (char*)P2V(pa), PGSIZE);
    if(mappages(d, (void*)i, PGSIZE, V2P(mem), flags) < 0)
      goto bad;
  }
  // copy shared pages
  struct sh_pg* shared_pages = myproc()->shared_pages; // old process's shared pages
  int j;
  for (j = 0; j < NSH; j++){
	void* va;
	if( (va = shared_pages[j].virtual_addr) != 0){ // found a shared page
		// copy it over to new process's pgdir
		
		// the page's physical address (in actual physical form)
		pa = (uint) global_shared_pages[j].phys_addr; 
		  
		// the page's flags
		if((pte = walkpgdir(parent_pgdir, va, 0)) == 0)
		  panic("copyuvm: pte should exist line 435\n");
		if(!(*pte & PTE_P))
		  panic("copyuvm: page not present line 437\n");
		//~ pa = PTE_ADDR(*pte); // already got it
		flags = PTE_FLAGS(*pte);
			  
		if(mappages(d, va, PGSIZE, pa, flags) < 0) { // no reason not to use the same va
			goto bad;
		}

		// update records
		child_proc->shared_pages[j].virtual_addr = va;
		global_shared_pages[j].reference_count++;
	  }
  }
  return d;

bad:
  freevm(d, child_proc);
  return 0;
}

//PAGEBREAK!
// Map user virtual address to kernel address.
char*
uva2ka(pde_t *pgdir, char *uva)
{
  pte_t *pte;

  pte = walkpgdir(pgdir, uva, 0);
  if((*pte & PTE_P) == 0)
    return 0;
  if((*pte & PTE_U) == 0)
    return 0;
  return (char*)P2V(PTE_ADDR(*pte));
}

// Copy len bytes from p to user address va in page table pgdir.
// Most useful when pgdir is not the current page table.
// uva2ka ensures this only works for PTE_U pages.
int
copyout(pde_t *pgdir, uint va, void *p, uint len)
{
  char *buf, *pa0;
  uint n, va0;

  buf = (char*)p;
  while(len > 0){
    va0 = (uint)PGROUNDDOWN(va);
    pa0 = uva2ka(pgdir, (char*)va0);
    if(pa0 == 0)
      return -1;
    n = PGSIZE - (va - va0);
    if(n > len)
      n = len;
    memmove(pa0 + (va - va0), buf, n);
    len -= n;
    buf += n;
    va = va0 + PGSIZE;
  }
  return 0;
}

void* va_of_shared_page_for_cur_process(int pg_num){
	// return 0 if shared page is absent
	
	struct sh_pg* shared_pages = myproc()->shared_pages;
	return shared_pages[pg_num].virtual_addr;
}

void* next_available_shared_memory_va_of_cur_process(){
	void* next_avail = 0;
	struct sh_pg* shared_pages = myproc()->shared_pages;
	char* page_ptr = (char*) KERNBASE; 
	int i, j;
	for(i=0; i<NSH && !next_avail; i++){ 
		// while have not found next_avail, look for it in the top 4 pages
		page_ptr-= PGSIZE; // potential virtual address of the page
		
		// check if page_ptr it's actually being used as a shared page 
		int is_being_used = 0;
		for (j=0; j<NSH; j++){ 
			if (page_ptr == shared_pages[j].virtual_addr){
				is_being_used = 1;
			}
		}
		// we didn't find it being used, so it's free
		if (!is_being_used){
			next_avail = page_ptr; 
		}
	} // end for
	return next_avail;
}


void* pa_of_shared_page_for_any_process(int pg_num){
	return global_shared_pages[pg_num].phys_addr;
}


void* shmem_access(int pg_num){
	
	// of the shared page
	char* user_va = 0; // user_va
	char* kernal_va = 0; // kernal_va
	
	pde_t* pgdir = myproc()->pgdir; 
	
	if (pg_num >= NSH || pg_num < 0 ){ return 0; } // bad request
	
	// if the process already has access to that shared page, get its va
	// if not
	// 		find out what va we need
	//  	if any process has the shared page, get the page's pa
	// 		if no process has the shared page, allocate a new physical page and get it's pa
	// 		map the va to the pa
	// update the global info 
	
	if ( (user_va = va_of_shared_page_for_cur_process(pg_num) ) == 0 ) {
		// find out what va we need
		user_va = next_available_shared_memory_va_of_cur_process(); 
		
		char* pa = 0; // pa
		// get or set page's pa
		if ( ( pa = pa_of_shared_page_for_any_process(pg_num)) == 0 ){
			// pa dne (ie page is not allocated for *any* process) so set it
			// allocate a new page 
			kernal_va = kalloc(); 
			if (kernal_va == 0 ){
				cprintf("shmem_access out of physical memory\n");
				return 0;
			}
			pa = (char*) V2P(kernal_va);
			memset(kernal_va, 0, PGSIZE); // initialize to zero
						
		} // end nested if
		
		// map the va to the pa: making it a real va 
		// 		(in that the page table knows that va has a physical space assigned to it
		//~ cprintf("trying to give it this va %p %p \n", user_va, (char*) shared_pg_va );
		if(mappages(pgdir, (char*)user_va, PGSIZE, (uint) pa, PTE_W|PTE_U) < 0){ 
		  cprintf("shmem access out of memory (2)\n");
		  kfree(P2V(pa));
		  return 0;
		}
		// update reference count if and only if this process didn't have access before
		global_shared_pages[pg_num].reference_count++;
		global_shared_pages[pg_num].phys_addr = pa;

	} // end if
	
	myproc()->shared_pages[pg_num].virtual_addr = user_va; 
	
	return user_va;
}

int shmem_count(int pg_num){
	return global_shared_pages[pg_num].reference_count;
}

//PAGEBREAK!
// Blank page.
//PAGEBREAK!
// Blank page.
//PAGEBREAK!
// Blank page.
