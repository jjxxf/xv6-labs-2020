// Saved registers for kernel context switches.
// 用于保存内核上下文切换时的寄存器状态
struct context {
  uint64 ra; // 常用于存储函数返回地址
  uint64 sp; // 指向当前的栈顶位置

  // callee-saved  由被调用者（callee）保存和恢复的寄存器
  uint64 s0;
  uint64 s1;
  uint64 s2;
  uint64 s3;
  uint64 s4;
  uint64 s5;
  uint64 s6;
  uint64 s7;
  uint64 s8;
  uint64 s9;
  uint64 s10;
  uint64 s11;
};

// Per-CPU state.
struct cpu {
  struct proc *proc;          // The process running on this cpu, or null. 正在运行在这个cpu上的进程，或者为空
  struct context context;     // swtch() here to enter scheduler(). 用于进入 scheduler() 的 swtch()。用于保存 CPU 的上下文信息。在进行上下文切换时，swtch() 函数会使用这个字段来进入调度器（scheduler）
  int noff;                   // Depth of push_off() nesting. push_off() 嵌套的深度。用于关闭中断，这个字段记录了关闭中断的嵌套层次
  int intena;                 // Were interrupts enabled before push_off()? push_off() 之前中断是否被打开？
}; 

extern struct cpu cpus[NCPU];

// per-process data for the trap handling code in trampoline.S.
// sits in a page by itself just under the trampoline page in the
// user page table. not specially mapped in the kernel page table.
// the sscratch register points here.
// uservec in trampoline.S saves user registers in the trapframe,
// then initializes registers from the trapframe's
// kernel_sp, kernel_hartid, kernel_satp, and jumps to kernel_trap.
// usertrapret() and userret in trampoline.S set up
// the trapframe's kernel_*, restore user registers from the
// trapframe, switch to the user page table, and enter user space.
// the trapframe includes callee-saved user registers like s0-s11 because the
// return-to-user path via usertrapret() doesn't return through
// the entire kernel call stack.
// 用于保存各种重要的寄存器状态，以便在中断处理过程中恢复现场
struct trapframe {
  /*   0 */ uint64 kernel_satp;   // kernel page table
  /*   8 */ uint64 kernel_sp;     // top of process's kernel stack
  /*  16 */ uint64 kernel_trap;   // usertrap()
  /*  24 */ uint64 epc;           // saved user program counter
  /*  32 */ uint64 kernel_hartid; // saved kernel tp
  /*  40 */ uint64 ra;
  /*  48 */ uint64 sp;
  /*  56 */ uint64 gp;
  /*  64 */ uint64 tp;
  /*  72 */ uint64 t0;
  /*  80 */ uint64 t1;
  /*  88 */ uint64 t2;
  /*  96 */ uint64 s0;
  /* 104 */ uint64 s1;
  /* 112 */ uint64 a0;
  /* 120 */ uint64 a1;
  /* 128 */ uint64 a2;
  /* 136 */ uint64 a3;
  /* 144 */ uint64 a4;
  /* 152 */ uint64 a5;
  /* 160 */ uint64 a6;
  /* 168 */ uint64 a7;
  /* 176 */ uint64 s2;
  /* 184 */ uint64 s3;
  /* 192 */ uint64 s4;
  /* 200 */ uint64 s5;
  /* 208 */ uint64 s6;
  /* 216 */ uint64 s7;
  /* 224 */ uint64 s8;
  /* 232 */ uint64 s9;
  /* 240 */ uint64 s10;
  /* 248 */ uint64 s11;
  /* 256 */ uint64 t3;
  /* 264 */ uint64 t4;
  /* 272 */ uint64 t5;
  /* 280 */ uint64 t6;
};

enum procstate { UNUSED, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };

// Per-process state
struct proc {
  struct spinlock lock;

  // p->lock must be held when using these:
  enum procstate state;        // Process state 进程状态
  struct proc *parent;         // Parent process 父进程
  void *chan;                  // If non-zero, sleeping on chan 如果非0.在chan等待
  int killed;                  // If non-zero, have been killed 如果非0.已经被杀死
  int xstate;                  // Exit status to be returned to parent's wait 进程退出状态，将返回给父进程的 wait
  int pid;    
  // 作业1                 // Process ID
  int trace_mask;              // Trace mask for this process 用于跟踪进程的掩码

  // these are private to the process, so p->lock need not be held.
  uint64 kstack;               // Virtual address of kernel stack 内核栈的虚拟地址
  uint64 sz;                   // Size of process memory (bytes) 进程内存的大小（字节）
  pagetable_t pagetable;       // User page table 用户页表
  struct trapframe *trapframe; // data page for trampoline.S 用于 trampoline.S 的数据页
  struct context context;      // swtch() here to run process 切换到这里以运行进程 
  struct file *ofile[NOFILE];  // Open files 打开的文件
  struct inode *cwd;           // Current directory 当前目录
  char name[16];               // Process name (debugging) 进程名（调试）
};
