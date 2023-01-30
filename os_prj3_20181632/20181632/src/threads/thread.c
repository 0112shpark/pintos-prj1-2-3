#include "threads/thread.h"
#include <debug.h>
#include <stddef.h>
#include <random.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/intr-stubs.h"
#include "threads/palloc.h"
#include "threads/switch.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "devices/timer.h"
#ifdef USERPROG
#include "userprog/process.h"
#endif
#define f (1 << 14)
/* Random value for struct thread's `magic' member.
   Used to detect stack overflow.  See the big comment at the top
   of thread.h for details. */
#define THREAD_MAGIC 0xcd6abf4b

/*timer_tick*/
static int64_t ticks = 0;

/* List of processes in THREAD_READY state, that is, processes
   that are ready to run but not actually running. */
static struct list ready_list;

/* List of all processes.  Processes are added to this list
   when they are first scheduled and removed when they exit. */
static struct list all_list;

/* Block 된 threads*/
static struct list blocked_list;
/* Idle thread. */
static struct thread *idle_thread;

/* Initial thread, the thread running init.c:main(). */
static struct thread *initial_thread;

/* Lock used by allocate_tid(). */
static struct lock tid_lock;

/* Stack frame for kernel_thread(). */
struct kernel_thread_frame 
  {
    void *eip;                  /* Return address. */
    thread_func *function;      /* Function to call. */
    void *aux;                  /* Auxiliary data for function. */
  };

/* Statistics. */
static long long idle_ticks;    /* # of timer ticks spent idle. */
static long long kernel_ticks;  /* # of timer ticks in kernel threads. */
static long long user_ticks;    /* # of timer ticks in user programs. */

/* Scheduling. */
#define TIME_SLICE 4            /* # of timer ticks to give each thread. */
static unsigned thread_ticks;   /* # of timer ticks since last yield. */

#ifndef USERPROG
/* project #3. */
bool thread_prior_aging;
static int load_avg;
#endif
/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
bool thread_mlfqs;

static void kernel_thread (thread_func *, void *aux);

static void idle (void *aux UNUSED);
static struct thread *running_thread (void);
static struct thread *next_thread_to_run (void);
static void init_thread (struct thread *, const char *name, int priority);
static bool is_thread (struct thread *) UNUSED;
static void *alloc_frame (struct thread *, size_t size);
static void schedule (void);
void thread_schedule_tail (struct thread *prev);
static tid_t allocate_tid (void);

/* Initializes the threading system by transforming the code
   that's currently running into a thread.  This can't work in
   general and it is possible in this case only because loader.S
   was careful to put the bottom of the stack at a page boundary.

   Also initializes the run queue and the tid lock.

   After calling this function, be sure to initialize the page
   allocator before trying to create any threads with
   thread_create().

   It is not safe to call thread_current() until this function
   finishes. */
void
thread_init (void) 
{
  ASSERT (intr_get_level () == INTR_OFF);

  lock_init (&tid_lock);
  list_init (&ready_list);
  list_init (&all_list);
  list_init (&blocked_list);
  load_avg = 0;

  /* Set up a thread structure for the running thread. */
  initial_thread = running_thread ();
  
  init_thread (initial_thread, "main", PRI_DEFAULT);
  initial_thread->status = THREAD_RUNNING;
  initial_thread->tid = allocate_tid ();
  initial_thread->nice = 0;
  initial_thread->recent_cpu = 0;
}

/* Starts preemptive thread scheduling by enabling interrupts.
   Also creates the idle thread. */
void
thread_start (void) 
{
  /* Create the idle thread. */
  struct semaphore idle_started;
  sema_init (&idle_started, 0);
  thread_create ("idle", PRI_MIN, idle, &idle_started);

  /* Start preemptive thread scheduling. */
  intr_enable ();

  /* Wait for the idle thread to initialize idle_thread. */
  sema_down (&idle_started);
}

/*list.c의 list_insert_ordered함수 사용을 위해 
priority 비교하는 함수 정의*/
bool compare_priority(const struct list_elem *a, const struct list_elem *b, void *aux){

  // priority 크기 비교->a가 b보다 크면 true
  return list_entry(a, struct thread, elem)->priority > list_entry(b, struct thread, elem)->priority;

}

/* Called by the timer interrupt handler at each timer tick.
   Thus, this function runs in an external interrupt context. */
void
thread_tick (void) 
{
  struct thread *t = thread_current ();
  ticks++;
  /* Update statistics. */
  if (t == idle_thread){
    idle_ticks++;
    
  }
#ifdef USERPROG
  else if (t->pagedir != NULL)
    user_ticks++;
#endif
  else{
    kernel_ticks++;
    
  }
  
  /* Enforce preemption. */
  if (++thread_ticks >= TIME_SLICE)
    intr_yield_on_return ();
#ifndef USERPROG
  /* Project #3. */
    //thread_wake_up (ticks);
  //printf("\n%d :aging\n",thread_mlfqs);
  /* Project #3. */
  if(thread_prior_aging == true || thread_mlfqs == true){
    //printf("ticks: %d\n", ticks);
    thread_aging();
  }
#endif
}
void thread_aging(void){
 
  // timer_interrupt 발생, recent_cpu + 1
  if(thread_current()!=idle_thread)
    thread_current()->recent_cpu = fix_int_add(thread_current()->recent_cpu,1);
  
  
  
  //printf("ok1\n");
  //1 초마다 load_avg, recent_cpu값 갱신
  if(ticks % TIMER_FREQ == 0){
    
    struct list_elem *list = list_begin(&all_list);
    struct thread *cur;
    // load_avg계산
    cal_load_avg();
    
    // 모든 thread의 recent_cpu 다시 계산
    while(list != list_end(&all_list)){
      cur = list_entry(list, struct thread, allelem);
      //printf("ok2\n");
      cal_recent_cpu(cur);
      
      list = list_next(list);
    }
    
    
  }
  // 4 tick 마다 priority 계산
  if(ticks % 4 == 0){
    struct list_elem *list1 = list_begin(&all_list);
    struct thread *cur1;
    
    // 모든 thread의 priority 다시 계산
    while(list1 != list_end(&all_list)){
      cur1 = list_entry(list1, struct thread, allelem);
      //printf("ok3\n");
      cal_priority(cur1);
      //printf("ok4\n");
      list1 = list_next(list1);
    }
    
  }
}

/* Prints thread statistics. */
void
thread_print_stats (void) 
{
  printf ("Thread: %lld idle ticks, %lld kernel ticks, %lld user ticks\n",
          idle_ticks, kernel_ticks, user_ticks);
}

/* Creates a new kernel thread named NAME with the given initial
   PRIORITY, which executes FUNCTION passing AUX as the argument,
   and adds it to the ready queue.  Returns the thread identifier
   for the new thread, or TID_ERROR if creation fails.

   If thread_start() has been called, then the new thread may be
   scheduled before thread_create() returns.  It could even exit
   before thread_create() returns.  Contrariwise, the original
   thread may run for any amount of time before the new thread is
   scheduled.  Use a semaphore or some other form of
   synchronization if you need to ensure ordering.

   The code provided sets the new thread's `priority' member to
   PRIORITY, but no actual priority scheduling is implemented.
   Priority scheduling is the goal of Problem 1-3. */
tid_t
thread_create (const char *name, int priority,
               thread_func *function, void *aux) 
{
  struct thread *t;
  struct kernel_thread_frame *kf;
  struct switch_entry_frame *ef;
  struct switch_threads_frame *sf;
  enum intr_level old_level;
  tid_t tid;

  ASSERT (function != NULL);

  /* Allocate thread. */
  t = palloc_get_page (PAL_ZERO);
  if (t == NULL)
    return TID_ERROR;

  /* Initialize thread. */
  init_thread (t, name, priority);
  tid = t->tid = allocate_tid ();
  old_level = intr_disable();

  /* Stack frame for kernel_thread(). */
  kf = alloc_frame (t, sizeof *kf);
  kf->eip = NULL;
  kf->function = function;
  kf->aux = aux;

  /* Stack frame for switch_entry(). */
  ef = alloc_frame (t, sizeof *ef);
  ef->eip = (void (*) (void)) kernel_thread;

  /* Stack frame for switch_threads(). */
  sf = alloc_frame (t, sizeof *sf);
  sf->eip = switch_entry;
  sf->ebp = 0;

  /*add child element*/
  //list_push_back(&(thread_current()->child), &(t->child_elem));
  
  intr_set_level(old_level);
  
  /* Add to run queue. */
  thread_unblock (t);

  /* 새로운 thread 생성 후, 현재 실행중인 thread의 priority와 ready queue의 젤 앞 비교*/
  // thread_current()대신 thread_get_priority 함수를 사용하면 출력이 밀림 --> ???
  
  /* (priority-change) Thread 2 now lowering priority.
     (priority-change) Thread 2 should have just lowered its priority.
      이 두 문장이 순서가 바뀌어서 나옴*/

  if(!list_empty(&ready_list) && list_entry(list_front(&ready_list), struct thread, elem)->priority > thread_current()->priority){
    // priority의 변화가 있고  더 큰 priority가 존재하면 CPU yield
    thread_yield();
  }
  return tid;
}

/* Puts the current thread to sleep.  It will not be scheduled
   again until awoken by thread_unblock().

   This function must be called with interrupts turned off.  It
   is usually a better idea to use one of the synchronization
   primitives in synch.h. */
void
thread_block (void) 
{
  ASSERT (!intr_context ());
  ASSERT (intr_get_level () == INTR_OFF);

  thread_current ()->status = THREAD_BLOCKED;
  schedule ();
}

/* Transitions a blocked thread T to the ready-to-run state.
   This is an error if T is not blocked.  (Use thread_yield() to
   make the running thread ready.)

   This function does not preempt the running thread.  This can
   be important: if the caller had disabled interrupts itself,
   it may expect that it can atomically unblock a thread and
   update other data. */
void
thread_unblock (struct thread *t) 
{
  enum intr_level old_level;

  ASSERT (is_thread (t));

  old_level = intr_disable ();
  ASSERT (t->status == THREAD_BLOCKED);
  // priority 정렬해서 재 삽입
  //list_push_back (&ready_list, &t->elem);
  list_insert_ordered(&ready_list, &t->elem, compare_priority, NULL);
  t->status = THREAD_READY;
  intr_set_level (old_level);
}

/*thread를 block_list에 넣는 함수*/
void thread_fall_sleep(int64_t ticks){
  
  
  enum intr_level old_level;

  //interrupt off
  old_level = intr_disable();

  struct thread *cur = thread_current();
  /*idle_thread는 실행주체가 아님*/
  ASSERT(cur != idle_thread);

  //일어날 시간
  cur->up_time = ticks;
  //block_list에 추가
  list_push_back(&blocked_list, &cur->elem);
  //block상태로 만듬
  thread_block();

  //interrupt on
  intr_set_level(old_level);

}

/*thread를 blocked_list에서 찾아 깨우는 함수*/
void thread_wake_up(int64_t ticks){
  
  struct list_elem *list = list_begin(&blocked_list);
  struct thread *cur;

  //깨울시간 확인 후 unblock
  while(list != list_end(&blocked_list)){
    cur = list_entry(list, struct thread, elem);
    if(cur->up_time <= ticks){ // 현 시간보다 thread의 일어날 시간이 적으면
      list = list_remove(list); // blocked_list에서 제거
      thread_unblock(cur); // unblock
    }
    else{
      list = list_next(list);
    }
  }
}


/* Returns the name of the running thread. */
const char *
thread_name (void) 
{
  return thread_current ()->name;
}

/* Returns the running thread.
   This is running_thread() plus a couple of sanity checks.
   See the big comment at the top of thread.h for details. */
struct thread *
thread_current (void) 
{
  struct thread *t = running_thread ();
  
  /* Make sure T is really a thread.
     If either of these assertions fire, then your thread may
     have overflowed its stack.  Each thread has less than 4 kB
     of stack, so a few big automatic arrays or moderate
     recursion can cause stack overflow. */
  ASSERT (is_thread (t));
  ASSERT (t->status == THREAD_RUNNING);

  return t;
}

/* Returns the running thread's tid. */
tid_t
thread_tid (void) 
{
  return thread_current ()->tid;
}

/* Deschedules the current thread and destroys it.  Never
   returns to the caller. */
void
thread_exit (void) 
{
  ASSERT (!intr_context ());

#ifdef USERPROG
  process_exit ();
#endif

  /* Remove thread from all threads list, set our status to dying,
     and schedule another process.  That process will destroy us
     when it calls thread_schedule_tail(). */
  intr_disable ();
  list_remove (&thread_current()->allelem);
  running_thread()->status = THREAD_DYING;
  schedule ();
  NOT_REACHED ();
}



/* Yields the CPU.  The current thread is not put to sleep and
   may be scheduled again immediately at the scheduler's whim. */
void
thread_yield (void) 
{
  struct thread *cur = thread_current ();
  enum intr_level old_level;
  
  ASSERT (!intr_context ());

  old_level = intr_disable ();
  if (cur != idle_thread){
    //list_push_back (&ready_list, &cur->elem); //list_insert_ordered로 교체
    // priority로 정렬하여 ready queue에 삽입
    list_insert_ordered(&ready_list, &cur->elem, compare_priority, NULL);  
  }
  cur->status = THREAD_READY;
  schedule ();
  intr_set_level (old_level);
}

/* Invoke function 'func' on all threads, passing along 'aux'.
   This function must be called with interrupts off. */
void
thread_foreach (thread_action_func *func, void *aux)
{
  struct list_elem *e;

  ASSERT (intr_get_level () == INTR_OFF);

  for (e = list_begin (&all_list); e != list_end (&all_list);
       e = list_next (e))
    {
      struct thread *t = list_entry (e, struct thread, allelem);
      func (t, aux);
    }
}
/*priority, recent_cpu, load_avg 계산*/
void cal_priority(struct thread *t){
  //printf("%d recent cpu: %d, nice : %d\n", PRI_MAX,t->recent_cpu, t->nice);
  if(t == idle_thread)
    return;
  
  t->priority = fix_to_int(fix_sub(fix_sub(int_to_fix(PRI_MAX),fix_int_div(t->recent_cpu,4)),fix_int_mul(int_to_fix(t->nice),2)));
  
  if(t->priority > PRI_MAX)
    t->priority = PRI_MAX;
  if(t->priority < PRI_MIN)
    t->priority = PRI_MIN;
}
void cal_recent_cpu(struct thread *t){
  
  if(t == idle_thread)
    return;

  t->recent_cpu = fix_add(fix_mul(fix_div(fix_int_mul(load_avg,2),fix_int_add(fix_int_mul(load_avg,2),1)),t->recent_cpu),int_to_fix(t->nice));

}
void cal_load_avg(void){

  int ready_threads_cnt;
  //ready_lists 안의 thread 갯수
  ready_threads_cnt = list_size(&ready_list);

  // 만약 idle_thread가 running이 아니라면 
  // 현재 running 중인 thread 갯수 더하기
  if(thread_current() != idle_thread){
    ready_threads_cnt++;
  }

  load_avg = fix_add(fix_mul(fix_div(int_to_fix(59),int_to_fix(60)),load_avg),fix_mul(fix_div(int_to_fix(1),int_to_fix(60)),int_to_fix(ready_threads_cnt)));
  //printf("%d\n", load_avg);
}
/* Sets the current thread's priority to NEW_PRIORITY. */
void
thread_set_priority (int new_priority) 
{
  // priority 변경 X
  if(thread_mlfqs){
    return;
  }

  // 실행중인 thread의 priority가 바뀔 경우
  int temp_priority = thread_get_priority();

  
  thread_current ()->priority = new_priority;

  // 새로 바뀐 priority가 더 작을 경우 reschedule
  if(temp_priority > new_priority){
    thread_yield();
  }
}

/* Returns the current thread's priority. */
int
thread_get_priority (void) 
{
  return thread_current ()->priority;
}

/* Sets the current thread's nice value to NICE. */
void
thread_set_nice (int nice ) 
{
  enum intr_level old_level;
  //printf("Before: %d %d\n", thread_current()->nice, nice);
  //interrupt 비활성화
  old_level = intr_disable();
  thread_current()->nice = nice;
  
  // nice 값이 변했으므로 priority값도 변함
  //printf("before: %d \n",thread_current()->priority);
  cal_priority(thread_current());
  //printf("after: %d \n",thread_current()->priority);
  //priority가 변했으므로 우선순위 recalculate
  if(!list_empty(&ready_list) && list_entry(list_front(&ready_list), struct thread, elem)->priority > thread_current()->priority){
    thread_yield();
  }
  intr_set_level(old_level);
  //printf("%d %d\n", thread_current()->up_time, nice);
}

/* Returns the current thread's nice value. */
int
thread_get_nice (void) 
{
  enum intr_level old_level;
  
  //interrupt 비활성화
  old_level = intr_disable();
  int return_val = thread_current()->nice;
  intr_set_level(old_level);

  return return_val;
}

/* Returns 100 times the system load average. */
int
thread_get_load_avg (void) 
{
  enum intr_level old_level;
  
  //interrupt 비활성화
  old_level = intr_disable();
  //printf("%d\n", load_avg);
  int return_val = fix_to_round(fix_int_mul(load_avg,100));
  intr_set_level(old_level);

  return return_val;
}

/* Returns 100 times the current thread's recent_cpu value. */
int
thread_get_recent_cpu (void) 
{
  
  enum intr_level old_level;
  
  //interrupt 비활성화
  old_level = intr_disable();
  int return_val = fix_to_round(fix_int_mul(thread_current()->recent_cpu, 100));
  intr_set_level(old_level);

  return return_val;
}

/* Idle thread.  Executes when no other thread is ready to run.

   The idle thread is initially put on the ready list by
   thread_start().  It will be scheduled once initially, at which
   point it initializes idle_thread, "up"s the semaphore passed
   to it to enable thread_start() to continue, and immediately
   blocks.  After that, the idle thread never appears in the
   ready list.  It is returned by next_thread_to_run() as a
   special case when the ready list is empty. */
static void
idle (void *idle_started_ UNUSED) 
{
  struct semaphore *idle_started = idle_started_;
  idle_thread = thread_current ();
  sema_up (idle_started);

  for (;;) 
    {
      /* Let someone else run. */
      intr_disable ();
      thread_block ();

      /* Re-enable interrupts and wait for the next one.

         The `sti' instruction disables interrupts until the
         completion of the next instruction, so these two
         instructions are executed atomically.  This atomicity is
         important; otherwise, an interrupt could be handled
         between re-enabling interrupts and waiting for the next
         one to occur, wasting as much as one clock tick worth of
         time.

         See [IA32-v2a] "HLT", [IA32-v2b] "STI", and [IA32-v3a]
         7.11.1 "HLT Instruction". */
      asm volatile ("sti; hlt" : : : "memory");
    }
}

/* Function used as the basis for a kernel thread. */
static void
kernel_thread (thread_func *function, void *aux) 
{
  ASSERT (function != NULL);

  intr_enable ();       /* The scheduler runs with interrupts off. */
  function (aux);       /* Execute the thread function. */
  thread_exit ();       /* If function() returns, kill the thread. */
}

/* Returns the running thread. */
struct thread *
running_thread (void) 
{
  uint32_t *esp;

  /* Copy the CPU's stack pointer into `esp', and then round that
     down to the start of a page.  Because `struct thread' is
     always at the beginning of a page and the stack pointer is
     somewhere in the middle, this locates the curent thread. */
  asm ("mov %%esp, %0" : "=g" (esp));
  return pg_round_down (esp);
}

/* Returns true if T appears to point to a valid thread. */
static bool
is_thread (struct thread *t)
{
  return t != NULL && t->magic == THREAD_MAGIC;
}

/* Does basic initialization of T as a blocked thread named
   NAME. */
static void
init_thread (struct thread *t, const char *name, int priority)
{
  enum intr_level old_level;

  ASSERT (t != NULL);
  ASSERT (PRI_MIN <= priority && priority <= PRI_MAX);
  ASSERT (name != NULL);

  memset (t, 0, sizeof *t);
  t->status = THREAD_BLOCKED;
  strlcpy (t->name, name, sizeof t->name);
  t->stack = (uint8_t *) t + PGSIZE;
  t->priority = priority;
  t->magic = THREAD_MAGIC;
  t->parents = running_thread();
  // 부모의 것을 상속받는다
  t->recent_cpu = running_thread()->recent_cpu;
  t->nice = running_thread()->nice;
  
  //old_level = intr_disable ();
  list_push_back (&all_list, &t->allelem);
  list_init(&(t->child));
  sema_init(&(t->child_lock), 0);
  sema_init(&(t->free_lock), 0);
  sema_init(&(t->early_lock), 0);

  for(int i=0; i<128;i++)
  {
    t->fd[i]=NULL;
  }
  list_push_back(&(running_thread()->child), &(t->child_elem));
  //intr_set_level (old_level);
}

/* Allocates a SIZE-byte frame at the top of thread T's stack and
   returns a pointer to the frame's base. */
static void *
alloc_frame (struct thread *t, size_t size) 
{
  /* Stack data is always allocated in word-size units. */
  ASSERT (is_thread (t));
  ASSERT (size % sizeof (uint32_t) == 0);

  t->stack -= size;
  return t->stack;
}

/* Chooses and returns the next thread to be scheduled.  Should
   return a thread from the run queue, unless the run queue is
   empty.  (If the running thread can continue running, then it
   will be in the run queue.)  If the run queue is empty, return
   idle_thread. */
static struct thread *
next_thread_to_run (void) 
{
  if (list_empty (&ready_list))
    return idle_thread;
  else
    return list_entry (list_pop_front (&ready_list), struct thread, elem);
}

/* Completes a thread switch by activating the new thread's page
   tables, and, if the previous thread is dying, destroying it.

   At this function's invocation, we just switched from thread
   PREV, the new thread is already running, and interrupts are
   still disabled.  This function is normally invoked by
   thread_schedule() as its final action before returning, but
   the first time a thread is scheduled it is called by
   switch_entry() (see switch.S).

   It's not safe to call printf() until the thread switch is
   complete.  In practice that means that printf()s should be
   added at the end of the function.

   After this function and its caller returns, the thread switch
   is complete. */
void
thread_schedule_tail (struct thread *prev)
{
  struct thread *cur = running_thread ();
  
  ASSERT (intr_get_level () == INTR_OFF);

  /* Mark us as running. */
  cur->status = THREAD_RUNNING;

  /* Start new time slice. */
  thread_ticks = 0;

#ifdef USERPROG
  /* Activate the new address space. */
  process_activate ();
#endif

  /* If the thread we switched from is dying, destroy its struct
     thread.  This must happen late so that thread_exit() doesn't
     pull out the rug under itself.  (We don't free
     initial_thread because its memory was not obtained via
     palloc().) */
  if (prev != NULL && prev->status == THREAD_DYING && prev != initial_thread) 
    {
      ASSERT (prev != cur);
      palloc_free_page (prev);
    }
}

/* Schedules a new process.  At entry, interrupts must be off and
   the running process's state must have been changed from
   running to some other state.  This function finds another
   thread to run and switches to it.

   It's not safe to call printf() until thread_schedule_tail()
   has completed. */
static void
schedule (void) 
{
  struct thread *cur = running_thread ();
  struct thread *next = next_thread_to_run ();
  struct thread *prev = NULL;

  ASSERT (intr_get_level () == INTR_OFF);
  ASSERT (cur->status != THREAD_RUNNING);
  ASSERT (is_thread (next));

  if (cur != next)
    prev = switch_threads (cur, next);
  thread_schedule_tail (prev);
}

/* Returns a tid to use for a new thread. */
static tid_t
allocate_tid (void) 
{
  static tid_t next_tid = 1;
  tid_t tid;

  lock_acquire (&tid_lock);
  tid = next_tid++;
  lock_release (&tid_lock);

  return tid;
}
/*fixed points calculations*/
int int_to_fix(int n){
  return n*f;
}
int fix_to_int(int x){
  return x/f;
}
int fix_to_round(int x){
  if(x>=0)
    return (x+f/2)/f;
  else 
    return (x-f/2)/f;
}
int fix_add(int x, int y){
  return x+y;
}
int fix_sub(int x, int y){
  return x-y;
}
int fix_int_add(int x, int n){
  return x+n*f;
}
int fix_int_sub(int x, int n){
  return x-n*f;
}
int fix_mul(int x, int y){
  return ((int64_t)x)*y/f;
}
int fix_int_mul(int x, int n){
  return x*n;
}
int fix_div(int x, int y){
  return ((int64_t)x)*f/y;
}
int fix_int_div(int x, int n){
  return x/n;
}



/* Offset of `stack' member within `struct thread'.
   Used by switch.S, which can't figure it out on its own. */
uint32_t thread_stack_ofs = offsetof (struct thread, stack);
