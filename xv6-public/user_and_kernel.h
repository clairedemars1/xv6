// definitions both kernal and user need

// moved from mmu.h
#define PGSIZE          4096    // bytes mapped by a page
#define PGROUNDUP(sz)  (((sz)+PGSIZE-1) & ~(PGSIZE-1))
#define PGROUNDDOWN(a) (((a)) & ~(PGSIZE-1))

#define NULL 0


