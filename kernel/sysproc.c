#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "sysinfo.h"

uint64
sys_exit(void)
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  if(argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

uint64
sys_trace(void)
{
  int mask;
  struct proc *p = myproc(); //获取当前函数的进程结构体
  if(argint(0, &mask) < 0)
    return -1;

  // mask默认为0，所以不会进入syscall的打印系统调用信息的部分，只有在这里传进去参数才会打印
  p->trace_mask = mask; //trace_mask字段赋值为mask   
  return 0;
}

// 作业2
uint64
sys_sysinfo(void)
{
  struct sysinfo info;
  uint64 st;

  if(argaddr(0, &st) < 0)
    return -1;

  info.freemem = freebytes();
  info.nproc = procnum();

  if(copyout(myproc()->pagetable, st, (char *)&info, sizeof(info)) < 0) // 将info传递给赋给st，传递至用户空间
    return -1;

  return 0;  
}


