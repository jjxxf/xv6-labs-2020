#include "param.h"
#include "types.h"
#include "memlayout.h"
#include "elf.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "spinlock.h" 
#include "proc.h"


/*
 * the kernel's page table.
 */
pagetable_t kernel_pagetable;

extern char etext[];  // kernel.ld sets this to end of kernel code.

extern char trampoline[]; // trampoline.S

// 作业2
pagetable_t
proc_kvminit()
{
  pagetable_t proc_kernel_pagetable = (pagetable_t) kalloc();
  memset(proc_kernel_pagetable, 0, PGSIZE);
  // memset(proc_kernel_pagetable, 0, PGSIZE);

  // uart registers
  proc_kvmmap(proc_kernel_pagetable, UART0, UART0, PGSIZE, PTE_R | PTE_W);

  // virtio mmio disk interface
  proc_kvmmap(proc_kernel_pagetable, VIRTIO0, VIRTIO0, PGSIZE, PTE_R | PTE_W);

  // CLINT 
  //作业3  CLINT 仅在内核启动的时候需要使用到，而用户进程在内核态中的操作并不需要使用到该映射,所以可以注释掉，防止重映射
  // proc_kvmmap(proc_kernel_pagetable, CLINT, CLINT, 0x10000, PTE_R | PTE_W);

  // PLIC
  proc_kvmmap(proc_kernel_pagetable, PLIC, PLIC, 0x400000, PTE_R | PTE_W);

  // map kernel text executable and read-only.
  proc_kvmmap(proc_kernel_pagetable, KERNBASE, KERNBASE, (uint64)etext-KERNBASE, PTE_R | PTE_X);

  // map kernel data and the physical RAM we'll make use of.
  proc_kvmmap(proc_kernel_pagetable, (uint64)etext, (uint64)etext, PHYSTOP-(uint64)etext, PTE_R | PTE_W);

  // map the trampoline for trap entry/exit to
  // the highest virtual address in the kernel.
  proc_kvmmap(proc_kernel_pagetable, TRAMPOLINE, (uint64)trampoline, PGSIZE, PTE_R | PTE_X);

  return proc_kernel_pagetable;
}

/*
 * create a direct-map page table for the kernel.
 */
void
kvminit()
{
  kernel_pagetable = (pagetable_t) kalloc();
  memset(kernel_pagetable, 0, PGSIZE);

  // uart registers
  kvmmap(UART0, UART0, PGSIZE, PTE_R | PTE_W);

  // virtio mmio disk interface
  kvmmap(VIRTIO0, VIRTIO0, PGSIZE, PTE_R | PTE_W);

  // CLINT
  kvmmap(CLINT, CLINT, 0x10000, PTE_R | PTE_W);

  // PLIC
  kvmmap(PLIC, PLIC, 0x400000, PTE_R | PTE_W);

  // map kernel text executable and read-only.
  kvmmap(KERNBASE, KERNBASE, (uint64)etext-KERNBASE, PTE_R | PTE_X);

  // map kernel data and the physical RAM we'll make use of.
  kvmmap((uint64)etext, (uint64)etext, PHYSTOP-(uint64)etext, PTE_R | PTE_W);

  // map the trampoline for trap entry/exit to
  // the highest virtual address in the kernel.
  kvmmap(TRAMPOLINE, (uint64)trampoline, PGSIZE, PTE_R | PTE_X);
}

// Switch h/w page table register to the kernel's page table,
// and enable paging.
// 将硬件页表寄存器切换到内核的页表，并启用分页。
void
kvminithart()
{
  w_satp(MAKE_SATP(kernel_pagetable));
  sfence_vma();
}

// Return the address of the PTE in page table pagetable
// that corresponds to virtual address va.  If alloc!=0,
// create any required page-table pages.
//
// The risc-v Sv39 scheme has three levels of page-table
// pages. A page-table page contains 512 64-bit PTEs.
// A 64-bit virtual address is split into five fields:
//   39..63 -- must be zero.
//   30..38 -- 9 bits of level-2 index.
//   21..29 -- 9 bits of level-1 index.
//   12..20 -- 9 bits of level-0 index.
//    0..11 -- 12 bits of byte offset within the page.
/**
 * @brief 遍历页表以查找给定虚拟地址的页表项（PTE）。
 *
 * 该函数从根页表开始遍历页表，并跟随与给定虚拟地址对应的页表项。它返回给定虚拟地址的PTE的指针。
 *
 * @param pagetable 根页表。
 * @param va 要查找其PTE的虚拟地址。
 * @param alloc 标志，指示是否在不存在时分配新的页表。
 * @return 给定虚拟地址的PTE的指针，如果PTE不存在且不允许分配，则返回NULL。
 */
pte_t *
walk(pagetable_t pagetable, uint64 va, int alloc)
{
  if(va >= MAXVA)
    panic("walk");

  for(int level = 2; level > 0; level--) {
    pte_t *pte = &pagetable[PX(level, va)]; // 根据深度从虚拟地址提取出pte
    if(*pte & PTE_V) { // 如果pte有效
      pagetable = (pagetable_t)PTE2PA(*pte);
    } else {
      if(!alloc || (pagetable = (pde_t*)kalloc()) == 0) // 如果alloc不为0，会执行kalloc()
        return 0;
      memset(pagetable, 0, PGSIZE);
      *pte = PA2PTE(pagetable) | PTE_V;
    }
  }
  return &pagetable[PX(0, va)]; // 返回与虚拟地址 va 对应的最终页表项 因为0是对应当前pagetable的，pagetabele已经递归到最深
 }

// Look up a virtual address, return the physical address,
// or 0 if not mapped.
// Can only be used to look up user pages.
uint64
walkaddr(pagetable_t pagetable, uint64 va)
{
  pte_t *pte;
  uint64 pa;

  if(va >= MAXVA)
    return 0;

  pte = walk(pagetable, va, 0);
  if(pte == 0)
    return 0;
  if((*pte & PTE_V) == 0)
    return 0;
  if((*pte & PTE_U) == 0)
    return 0;
  pa = PTE2PA(*pte);
  return pa;
}

// add a mapping to the kernel page table. // 向内核页表添加映射
// only used when booting. // 仅在引导时使用
// does not flush TLB or enable paging. // 不刷新TLB或启用分页
void
kvmmap(uint64 va, uint64 pa, uint64 sz, int perm)
{
  if(mappages(kernel_pagetable, va, sz, pa, perm) != 0)
    panic("kvmmap");
}

// 作业2 为进程中的内核页表创建进程内核栈
void
proc_kvmmap(pagetable_t pagetable, uint64 va, uint64 pa, uint64 sz, int perm)
{
  if(mappages(pagetable, va, sz, pa, perm) != 0)
    panic("proc_kvmmap");
}


/**
 * 将内核虚拟地址转换为物理地址。
 * 该函数仅适用于栈上的地址。
 * 假设虚拟地址（va）是页对齐的。
 *
 * @param va 要转换的内核虚拟地址。
 * @return 对应的物理地址。
 */
// 将内核虚拟地址转换为物理地址
// a physical address. only needed for
// addresses on the stack.
// assumes va is page aligned.
uint64
kvmpa(uint64 va)
{
  uint64 off = va % PGSIZE;
  pte_t *pte;
  uint64 pa;

  pte = walk(myproc()->kernel_pagetable, va, 0); // 作业二。把全局的内核页表修改为当前进程的内核页表。 因为有其他进程会调用，所以要改
  if(pte == 0)
    panic("kvmpa");
  if((*pte & PTE_V) == 0)
    panic("kvmpa");
  pa = PTE2PA(*pte);
  return pa+off; // 物理地址
}

// 作业2 将进程中内核页表的进程内核栈 转换为 物理地址
uint64
proc_kvmpa(pagetable_t pagetable, uint64 va)
{
  uint64 off = va % PGSIZE;
  pte_t *pte;
  uint64 pa;
  
  pte = walk(pagetable, va, 0); // 找到va对应的物理地址的PTE
  if(pte == 0)
    panic("proc_kvmpa");
  if((*pte & PTE_V) == 0)
    panic("proc_kvmpa");
  pa = PTE2PA(*pte);
  return pa+off; // 物理地址
}

/**
 * 为从va开始的虚拟地址创建指向从pa开始的物理地址的PTE。
 *
 * @param pagetable 要创建PTE的页表。
 * @param va 起始虚拟地址。
 * @param size 虚拟地址范围的大小。
 * @param pa 起始物理地址。
 * @param perm PTE的权限。
 * @return 成功返回0，如果walk()无法分配所需的页表页，则返回-1。
 */
// 将范围虚拟地址到同等范围物理地址的映射装载到一个页表中
// physical addresses starting at pa. va and size might not
// be page-aligned. Returns 0 on success, -1 if walk() couldn't
// allocate a needed page-table page.
int
mappages(pagetable_t pagetable, uint64 va, uint64 size, uint64 pa, int perm)
{
  uint64 a, last;
  pte_t *pte;

  a = PGROUNDDOWN(va);
  last = PGROUNDDOWN(va + size - 1);
  for(;;){
    if((pte = walk(pagetable, a, 1)) == 0)
      return -1;
    if(*pte & PTE_V)
      panic("remap");
    *pte = PA2PTE(pa) | perm | PTE_V;
    if(a == last)
      break;
    a += PGSIZE;
    pa += PGSIZE;
  }
  return 0;
}

// 从给定的页表中移除从虚拟地址 va 开始的 npages 个页的映射。该函数还可以选择性地释放物理内存（dofree=1）
// Remove npages of mappings starting from va. va must be
// page-aligned. The mappings must exist.
// Optionally free the physical memory.
void
uvmunmap(pagetable_t pagetable, uint64 va, uint64 npages, int do_free)
{
  uint64 a;
  pte_t *pte;

  if((va % PGSIZE) != 0)
    panic("uvmunmap: not aligned");

  for(a = va; a < va + npages*PGSIZE; a += PGSIZE){
    if((pte = walk(pagetable, a, 0)) == 0)
      panic("uvmunmap: walk");
    if((*pte & PTE_V) == 0)
      panic("uvmunmap: not mapped");
    if(PTE_FLAGS(*pte) == PTE_V)
      panic("uvmunmap: not a leaf");
    if(do_free){
      uint64 pa = PTE2PA(*pte);
      kfree((void*)pa);
    }
    *pte = 0;
  }
}

// create an empty user page table.
// returns 0 if out of memory.
pagetable_t
uvmcreate()
{
  pagetable_t pagetable;
  pagetable = (pagetable_t) kalloc();
  if(pagetable == 0)
    return 0;
  memset(pagetable, 0, PGSIZE);
  return pagetable;
}

// Load the user initcode into address 0 of pagetable,
// for the very first process.
// sz must be less than a page.
void
uvminit(pagetable_t pagetable, uchar *src, uint sz)
{
  char *mem;

  if(sz >= PGSIZE)
    panic("inituvm: more than a page");
  mem = kalloc();
  memset(mem, 0, PGSIZE);
  mappages(pagetable, 0, PGSIZE, (uint64)mem, PTE_W|PTE_R|PTE_X|PTE_U);
  memmove(mem, src, sz);
}

// Allocate PTEs and physical memory to grow process from oldsz to
// newsz, which need not be page aligned.  Returns new size or 0 on error.
uint64
uvmalloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz)
{
  char *mem;
  uint64 a;

  if(newsz < oldsz)
    return oldsz;

  oldsz = PGROUNDUP(oldsz);
  for(a = oldsz; a < newsz; a += PGSIZE){
    mem = kalloc();
    if(mem == 0){
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
    memset(mem, 0, PGSIZE);
    if(mappages(pagetable, a, PGSIZE, (uint64)mem, PTE_W|PTE_X|PTE_R|PTE_U) != 0){
      kfree(mem);
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
  }
  return newsz;
}

// Deallocate user pages to bring the process size from oldsz to
// newsz.  oldsz and newsz need not be page-aligned, nor does newsz
// need to be less than oldsz.  oldsz can be larger than the actual
// process size.  Returns the new process size.
uint64
uvmdealloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz)
{
  if(newsz >= oldsz)
    return oldsz;

  if(PGROUNDUP(newsz) < PGROUNDUP(oldsz)){
    int npages = (PGROUNDUP(oldsz) - PGROUNDUP(newsz)) / PGSIZE;
    uvmunmap(pagetable, PGROUNDUP(newsz), npages, 1);
  }

  return newsz;
}

// Recursively free page table pages  [ 递归释放页表页面 ]
// All leaf mappings must already have been removed  [ 必须已删除所有叶映射 ]
void
freewalk(pagetable_t pagetable)
{
  // there are 2^9 = 512 PTEs in a page table.
  for(int i = 0; i < 512; i++){
    pte_t pte = pagetable[i];
    // 当一个 PTE 有效但没有读、写或执行权限时，通常表示该 PTE 指向的是一个较低级别的页表，
    // 而不是一个实际的物理页（即不是叶子节点）。这是因为叶子节点通常表示实际的物理页，
    // 并且至少会有一个权限位被设置（读、写或执行权限）。
    if((pte & PTE_V) && (pte & (PTE_R|PTE_W|PTE_X)) == 0){
      // this PTE points to a lower level page table  [此pte指向较低级别的页表 ]
      uint64 child = PTE2PA(pte);
      freewalk((pagetable_t)child);
      pagetable[i] = 0; // 将要释放的页表项清零
    } else if(pte & PTE_V){ // pte为叶子节点
      panic("freewalk: leaf");
    }
  }
  kfree((void*)pagetable); // 释放
}

// 作业2 释放进程的内核页表
void
free_proc_kernelpage(pagetable_t pagetable)
{
  // printf("pagetable: %p\n", pagetable);
  // there are 2^9 = 512 PTEs in a page table.
  for(int i = 0; i < 512; i++){
    pte_t pte = pagetable[i];
    // 当一个 PTE 有效但没有读、写或执行权限时，通常表示该 PTE 指向的是一个较低级别的页表，
    // 而不是一个实际的物理页（即不是叶子节点）。这是因为叶子节点通常表示实际的物理页，
    // 并且至少会有一个权限位被设置（读、写或执行权限）。
    if((pte & PTE_V) && (pte & (PTE_R|PTE_W|PTE_X)) == 0){
      // this PTE points to a lower level page table  [此pte指向较低级别的页表 ]
      uint64 child = PTE2PA(pte);
      // printf("%d: pte %p pa %p\n", i, pte, child);
      free_proc_kernelpage((pagetable_t)child);
      pagetable[i] = 0; // 将要释放的页表项清零
    } else if(pte & PTE_V){ // pte为叶子节点
      break;
    } 
  }
  kfree((void*)pagetable); // 释放
}

// Free user memory pages,
// then free page-table pages.
void
uvmfree(pagetable_t pagetable, uint64 sz)
{
  if(sz > 0)
    uvmunmap(pagetable, 0, PGROUNDUP(sz)/PGSIZE, 1);
  freewalk(pagetable);
}

// Given a parent process's page table, copy
// its memory into a child's page table.
// Copies both the page table and the
// physical memory.
// returns 0 on success, -1 on failure.
// frees any allocated pages on failure.
// 将父进程的页表及其物理内存复制到子进程的页表中
int
uvmcopy(pagetable_t old, pagetable_t new, uint64 sz)
{
  pte_t *pte;
  uint64 pa, i;
  uint flags;
  char *mem;

  for(i = 0; i < sz; i += PGSIZE){
    if((pte = walk(old, i, 0)) == 0)
      panic("uvmcopy: pte should exist");
    if((*pte & PTE_V) == 0)
      panic("uvmcopy: page not present");
    pa = PTE2PA(*pte);
    flags = PTE_FLAGS(*pte);
    if((mem = kalloc()) == 0)
      goto err;
    memmove(mem, (char*)pa, PGSIZE); // pa传给mem
    if(mappages(new, i, PGSIZE, (uint64)mem, flags) != 0){ // 将mem映射到new的i处
      kfree(mem);
      goto err;
    }
  }
  return 0;

 err:
  uvmunmap(new, 0, i / PGSIZE, 1);
  return -1;
}

// 作业3 将用户空间的映射复制到进程的内核页表中
int
uvmcopy_to_kernel(pagetable_t proc_pagetable, pagetable_t kernel_pagetable, uint64 begin, uint64 end)
{
  pte_t *pte;
  uint64 pa, i;
  uint flags;
  uint64 begin_page = PGROUNDUP(begin); // 按页(4kb)映射，向上取整，防止输入的begin不是页对齐的

  for(i = begin_page; i < end; i += PGSIZE){
    if((pte = walk(proc_pagetable, i, 0)) == 0) // 找到对应的pte，从而找到对应点物理地址
      panic("uvmcopy_to_kernel: pte should exist");
    if((*pte & PTE_V) == 0)
      panic("uvmcopy_to_kernel: page not present");
    pa = PTE2PA(*pte);
    flags = PTE_FLAGS(*pte) & (~PTE_U); // 在内核模式下，无法访问设置了PTE_U的页面,所以要去除PTE_U
    if(mappages(kernel_pagetable, i, PGSIZE, pa, flags) != 0){ // 将pa映射到proc_pagetable的i处
      goto err;
    }
  }
  return 0;

 err:
  uvmunmap(kernel_pagetable, begin_page, (i-begin_page) / PGSIZE, 0);
  return -1;
}


// mark a PTE invalid for user access.
// used by exec for the user stack guard page.
void
uvmclear(pagetable_t pagetable, uint64 va)
{
  pte_t *pte;
  
  pte = walk(pagetable, va, 0);
  if(pte == 0)
    panic("uvmclear");
  *pte &= ~PTE_U;
}

// Copy from kernel to user.
// Copy len bytes from src to virtual address dstva in a given page table.
// Return 0 on success, -1 on error.
int
copyout(pagetable_t pagetable, uint64 dstva, char *src, uint64 len)
{
  uint64 n, va0, pa0;

  while(len > 0){
    va0 = PGROUNDDOWN(dstva);
    pa0 = walkaddr(pagetable, va0);
    if(pa0 == 0)
      return -1;
    n = PGSIZE - (dstva - va0);
    if(n > len)
      n = len;
    memmove((void *)(pa0 + (dstva - va0)), src, n);

    len -= n;
    src += n;
    dstva = va0 + PGSIZE;
  }
  return 0;
}

// Copy from user to kernel.
// Copy len bytes to dst from virtual address srcva in a given page table.
// Return 0 on success, -1 on error.
int
copyin(pagetable_t pagetable, char *dst, uint64 srcva, uint64 len)
{
  // uint64 n, va0, pa0;

  // while(len > 0){
  //   va0 = PGROUNDDOWN(srcva);
  //   pa0 = walkaddr(pagetable, va0);
  //   if(pa0 == 0)
  //     return -1;
  //   n = PGSIZE - (srcva - va0);
  //   if(n > len)
  //     n = len;
  //   memmove(dst, (void *)(pa0 + (srcva - va0)), n);

  //   len -= n;
  //   dst += n;
  //   srcva = va0 + PGSIZE;
  // }
  // return 0;
  return copyin_new(pagetable, dst, srcva, len);
}

// Copy a null-terminated string from user to kernel.
// Copy bytes to dst from virtual address srcva in a given page table,
// until a '\0', or max.
// Return 0 on success, -1 on error.
int
copyinstr(pagetable_t pagetable, char *dst, uint64 srcva, uint64 max)
{
  // uint64 n, va0, pa0;
  // int got_null = 0;

  // while(got_null == 0 && max > 0){
  //   va0 = PGROUNDDOWN(srcva);
  //   pa0 = walkaddr(pagetable, va0);
  //   if(pa0 == 0)
  //     return -1;
  //   n = PGSIZE - (srcva - va0);
  //   if(n > max)
  //     n = max;

  //   char *p = (char *) (pa0 + (srcva - va0));
  //   while(n > 0){
  //     if(*p == '\0'){
  //       *dst = '\0';
  //       got_null = 1;
  //       break;
  //     } else {
  //       *dst = *p;
  //     }
  //     --n;
  //     --max;
  //     p++;
  //     dst++;
  //   }

  //   srcva = va0 + PGSIZE;
  // }
  // if(got_null){
  //   return 0;
  // } else {
  //   return -1;
  // }
  return copyinstr_new(pagetable, dst, srcva, max);
}


// 作业1
void
vmprint(pagetable_t pagetable)
{
  printf("page table %p\n", pagetable);
  int depth = 1;
  traversal(pagetable, depth);

}

void traversal(pagetable_t pagetable, int depth)
{
  // there are 2^9 = 512 PTEs in a page table.
  for(int i = 0; i < 512; i++){
    pte_t pte = pagetable[i];
    // 当一个 PTE 有效但没有读、写或执行权限时，通常表示该 PTE 指向的是一个较低级别的页表，
    // 而不是一个实际的物理页（即不是叶子节点）。这是因为叶子节点通常表示实际的物理页，
    // 并且至少会有一个权限位被设置（读、写或执行权限）。
    if(pte & PTE_V){
      // this PTE points to a lower level page table  [此pte指向较低级别的页表 ]
      uint64 child = PTE2PA(pte);
      for(int j = 1; j <= depth; j++){
        if(j == 1)
          printf("..");
        else  
          printf(" ..");
      }
      printf("%d: pte %p pa %p\n", i, pte, child);
      
      // pte为叶子节点, 只能Continue，不能return。
      //如果return，同一级的其他节点就不会被遍历打印
      if((pte & (PTE_R|PTE_W|PTE_X)) != 0){ 
        continue;
      }
      depth++;
      traversal((pagetable_t)child, depth);
      depth--;
    // }else if(pte & PTE_V){ // pte为叶子节点
    //   return;
    }
  }
}
