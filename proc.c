#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
#include "date.h"

#define NULL 0;
struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

// Must be called with interrupts disabled
int
cpuid() {
  return mycpu()-cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu*
mycpu(void)
{
  int apicid, i;
  
  if(readeflags()&FL_IF)
    panic("mycpu called with interrupts enabled\n");
  
  apicid = lapicid();
  // APIC IDs are not guaranteed to be contiguous. Maybe we should have
  // a reverse map, or reserve a register to store &cpus[i].
  for (i = 0; i < ncpu; ++i) {
    if (cpus[i].apicid == apicid)
      return &cpus[i];
  }
  panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc*
myproc(void) {
  struct cpu *c;
  struct proc *p;
  pushcli();
  c = mycpu();
  p = c->proc;
  //p->startTime[0] = 0; p->startTime[1] = 0;
  //p->endTime[0] = 0; p->endTime[1] = 0;
  popcli();
  return p;
}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;
  int runnableNum = 3;
  acquire(&ptable.lock);

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == RUNNABLE || p->state == RUNNING)
	runnableNum++;
  }
  //cprintf("\n");
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;

  release(&ptable.lock);
  return 0;


// www foo default
found:
  p->state = EMBRYO;
  p->pid = nextpid++;           //Default priority
  p->inTime = 0;
  p->overDeadline = 0;
  p->deadline[0] = 24;
  p->deadline[1] = 60;
  p->startTime[0] = 24;
  p->startTime[1] = 60;
  p->endTime[0] = 24;
  p->endTime[1] = 60;
  if(runnableNum > 20)
	p->priority = 20;
  else if(runnableNum < 3)
	p->priority = 3;
  else
	p->priority = runnableNum;
  
  //cprintf("%d-%d\n", p->pid, p->priority);
  release(&ptable.lock);
 
	
  
  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

  return p;
}

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();
  
  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);

  p->state = RUNNABLE;

  release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  struct proc *curproc = myproc();

  sz = curproc->sz;
  if(n > 0){
    if((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  curproc->sz = sz;
  switchuvm(curproc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *curproc = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy process state from proc.
  if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = curproc->sz;
  np->parent = curproc;
  *np->tf = *curproc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;

  acquire(&ptable.lock);

  np->state = RUNNABLE;

  release(&ptable.lock);

  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  if(curproc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == curproc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  curproc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  struct proc *p;
  int havekids, pid;
  struct proc *curproc = myproc();
  
  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    // process finished
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
	cprintf("---------------\n");
	cprintf("pid: %d\nCPU ticks: %d\nMemory: %d\n",p->pid, p->curalarmticks, p->sz);
	cprintf("---------------\n");
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}

//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.

/* www new schedular*/
void
scheduler(void)
{
  struct proc *p;

  //get time
  struct rtcdate r;
  cmostime(&r);
  int hour = r.hour + 8, min = r.minute, totalMin;
  int temp;
  //static int check = 0;
  struct cpu *c = mycpu();
  c->proc = 0;

  for(;;){
   // Enable interrupts on this processor.
    sti();
     //
   // struct proc *highP=NULL;
    // Loop over process table looking for process to run.
    acquire(&ptable.lock);  //pic
    
    for(p=ptable.proc;p<&ptable.proc[NPROC];p++){
	if(p->name[0] == 's' && p->name[1] == 'h')
	  { p->priority = 1;}
	if(p->name[0] == 'p' && p->name[1] == 't')
	  {p->priority = 1;}
    }

	//int closest = 1440;
	//int closestPid = 0;

    /* check priority per minute */
    cmostime(&r);//pic
    if((min != r.minute))
    {
	//cprintf("Check time per minutes\n");	
	min = r.minute;
	hour = r.hour + 8;
        for(p=ptable.proc;p<&ptable.proc[NPROC];p++)
	{	
	    /* check if prority is in time or not*/
	    if(p->startTime[0] == hour && p->startTime[1] == min && p->inTime == 0){
	        //cprintf("now1: %d-%d\n", p->priority, p->timePriority);
	    	temp = p->priority;
	        p->priority = p->timePriority;
 	        p->timePriority = temp; 
		p->inTime = 1;
                //cprintf("changed1: %d-%d\n", p->priority, p->timePriority);
            }
		//cprintf("%d\n", c->cpuNum);
		//cprintf("--%d--\n", nowCPU);
	    if(p->endTime[0] == hour && p->endTime[1] == min && p->inTime == 1){	
		//cprintf("now2: %d-%d\n", p->priority, p->timePriority);	        
		temp = p->timePriority;
	        p->timePriority = p->priority;
 	        p->priority = temp;
		p->inTime = 0;
	        //cprintf("changed2: %d-%d\n", p->priority, p->timePriority);
            }
	    /* check deadline if process is over deadline or not*/
	    
	    totalMin = (p->deadline[0] - hour) * 60 + (p->deadline[1] - min);
	    if(p->inTime == 0 && totalMin <= 0)
		{p->priority = 2; p->overDeadline = 1;}
	    else{
		p->overDeadline = 0;
		//if(closest > totalMin)
		    //{closest = totalMin; closestPid = p->pid;}
		}
        }
    }
/*
	//if(check == 0){
	    for(p=ptable.proc;p<&ptable.proc[NPROC];p++){
	        if(p->state == RUNNABLE && p->pid == closestPid && p->priority != 3)
	   	    p->priority = 3;
	        else{
		    if(p->state == RUNNABLE || p->state == RUNNING)
		    p->priority++;
	       	    }		
	        }
            }
	    //check = 1;
        //}
	//else
	    //check = 0;

*/



	/* find process with hightest priority */
	int highestPid = 0;
	int highestPri = 20;
	int deadline = 1440;
	for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
	    if(p->state == RUNNABLE && (p->priority < highestPri)){
		highestPid = p->pid;
		highestPri = p->priority;
		deadline = (p->deadline[0] - hour) * 60 + (p->deadline[1] - min);
	    }
	    /* check which deadline is closer */
	    else if(p->state == RUNNABLE && (p->priority == highestPri)){
		int pDeadline = (p->deadline[0] - hour) * 60 + (p->deadline[1] - min);
		if(pDeadline < deadline){		
		    highestPid = p->pid;
		    highestPri = p->priority;
		    deadline = pDeadline;
		}
	    }
	}

      for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
       if(p->state != RUNNABLE)
        continue;
       if(p->pid != highestPid)
	continue;

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      c->proc = p;
      switchuvm(p);//pic
      p->state = RUNNING;
      p->cpuNum = c->cpuNum;

      swtch(&(c->scheduler), p->context);//pic
      switchkvm();//pic

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
    }
    release(&ptable.lock);//pic

  }
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(mycpu()->ncli != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = mycpu()->intena;
  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  myproc()->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  
  if(p == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    release(lk);
  }
  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING)
        p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  int i;
  struct proc *p;
  char *state;
  uint pc[10];

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}
int
cps()
{
  struct proc *p;
  
  // Enable interrupts on this processor.
  sti();

    // Loop over process table looking for process with pid.
  acquire(&ptable.lock);
  cprintf("------------------------------------------------------------------------------------------------------------------\n");
  cprintf("name \t pid \t state \t \t priority \t startTime \t endTime \t deadline \t CPU ticks \t memory\n");
  for(p=ptable.proc;p<&ptable.proc[NPROC];p++){
      if(p->state == SLEEPING)
        cprintf("%s\t %d  \t SLEEPING \t %d \t\t\t\t\t\t\t\t\t\t %d\n"
	,p->name,p->pid,p->priority, p->sz);
      else if(p->state == RUNNING)
	cprintf("%s\t %d  \t RUNNING \t %d \t\t %d:%d \t\t %d:%d \t\t %d:%d \t\t %d \t\t %d\n"
	,p->name,p->pid,p->priority, p->startTime[0], p->startTime[1], p->endTime[0], p->endTime[1], p->deadline[0], p->deadline[1], p->curalarmticks, p->sz);
      else if(p->state == RUNNABLE)
	cprintf("%s\t %d  \t RUNNABLE \t %d \t\t %d:%d \t\t %d:%d \t\t %d:%d \t\t %d \t\t %d\n"
	,p->name,p->pid,p->priority, p->startTime[0], p->startTime[1], p->endTime[0], p->endTime[1], p->deadline[0], p->deadline[1], p->curalarmticks, p->sz);
  }
  cprintf("------------------------------------------------------------------------------------------------------------------\n");  
  

  release(&ptable.lock);

  return 22;

}

//change priority
int
chpr(int pid,int priority)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p=ptable.proc;p<&ptable.proc[NPROC];p++){
    if(p->pid == pid){
	if(p->inTime == 1)
	    cprintf("Error, process is in setTime.\n");
	else
  	    p->priority = priority;
        break;
    }
  }
  release(&ptable.lock);

  return pid;
}
/********need add define in defs.h**********/
//set priority time
int setTime(int pid, int priority, int startHour, int startMin, int endHour, int endMin, int deadlineHour, int deadlineMin)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p=ptable.proc;p<&ptable.proc[NPROC];p++){
    if(p->pid == pid){
        p->startTime[0] = startHour;
	p->startTime[1] = startMin;
	p->endTime[0] = endHour;
	p->endTime[1] = endMin;
	p->deadline[0] = deadlineHour;
	p->deadline[1] = deadlineMin;
	p->timePriority = priority;
        break;
    }
  }
  release(&ptable.lock);
  return pid;

}

//check time
int checkTime(int hour, int min)
{
  struct proc *p;
  int temp;
  /*??*/
  acquire(&ptable.lock);
  for(p=ptable.proc;p<&ptable.proc[NPROC];p++){
    cprintf("%d--%d\n", p->pid, p->priority);
    if(p->startTime[0] == hour && p->startTime[1] == min){
	
	    temp = p->priority;
	    p->priority = p->timePriority;
 	    p->timePriority = temp; 
    }
    if(p->endTime[0] == hour && p->endTime[1] == min){
	
	    temp = p->timePriority;
	    p->timePriority = p->priority;
 	    p->priority = temp; 
    }
  }
    cprintf("%d\n");
  release(&ptable.lock);
  return 0;
}

int checkPr(void){

struct proc *p;
  
  // Enable interrupts on this processor.
  sti();

    // Loop over process table looking for process with pid.
  acquire(&ptable.lock);

  for(p=ptable.proc;p<&ptable.proc[NPROC];p++){
    if((p->state == RUNNABLE || p->state == RUNNING) && p->inTime == 0 && p->overDeadline == 0 && p->priority > 3)
	p->priority--;
  }
  release(&ptable.lock);

  return 0;


}

