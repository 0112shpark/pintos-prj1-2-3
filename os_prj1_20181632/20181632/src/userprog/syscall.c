#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "devices/shutdown.h"
#include "threads/vaddr.h"

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}
/* esp의 값이 user address인지 판별*/
void is_useradd(const void *vaddr)
{
  if(!is_user_vaddr(vaddr))
    exit(-1);
}
static void
syscall_handler (struct intr_frame *f ) 
{
  //printf("syscall number:%d", *(uint32_t*)f->esp);
  //printf("\n%d %d %d %d is in (f->esp)\n", *(uint32_t*)(f->esp),*(uint32_t*)(f->esp+4),*(uint32_t*)(f->esp+8),*(uint32_t*)(f->esp+12));
  //printf ("system call!\n");
  //hex_dump(f->esp, f->esp, 100,1);

  switch (*(uint32_t*)(f->esp)){
    case SYS_HALT:
      //is_useradd(f->esp+4);
      halt();
      break;
    case SYS_EXIT:
      is_useradd(f->esp+4);
      exit(*(uint32_t*)(f->esp+4));
      break;
    case SYS_EXEC:
      //printf("\ncmd: %d\n", (const char*)(f->esp+20));
      is_useradd(f->esp+4);
      f->eax = exec((const char*)*(uint32_t*)(f->esp+4));
      break;
    case SYS_WAIT:
      is_useradd(f->esp+4);
      f->eax = wait((pid_t)*(uint32_t*)(f->esp+4));
      break;
    case SYS_READ:
      is_useradd(f->esp+4);
      is_useradd(f->esp+8);
      is_useradd(f->esp+12);
      f->eax = read((int)*(uint32_t*)(f->esp+4), (const void*)*(uint32_t*)(f->esp+8),(unsigned)*(uint32_t*)(f->esp+12));
      break;
    case SYS_WRITE:
    //printf("Write!\n");
      is_useradd(f->esp+4);
      is_useradd(f->esp+8);
      is_useradd(f->esp+12);
      f->eax = write((int)*(uint32_t*)(f->esp+4), (const void*)*(uint32_t*)(f->esp+8),(unsigned)*(uint32_t*)(f->esp+12));
      break;
    case SYS_FIBO:
      is_useradd(f->esp+4);
      f->eax = (uint32_t)fibonacci((int)*(uint32_t*)(f->esp+4));
      break;
    case SYS_MAX_FOUR:
      is_useradd(f->esp+4);
      is_useradd(f->esp+8);
      is_useradd(f->esp+12);
      is_useradd(f->esp+16);
      f->eax = max_of_four_int((int)*(uint32_t*)(f->esp+4),(int)*(uint32_t*)(f->esp+8),(int)*(uint32_t*)(f->esp+12),(int)*(uint32_t*)(f->esp+16));
      break;
  }
}
void halt(void){
  shutdown_power_off();
}
void exit (int status)
{
  
  printf("%s: exit(%d)\n", thread_name(), status);
  thread_current() -> exit_stat = status;
  thread_exit();
}
pid_t exec (const char *cmd_line){
  //printf("executing.. %s\n", thread_name());
  pid_t pid = (pid_t)process_execute(cmd_line);
  return pid;
}
int wait (pid_t pid)
{
  //printf("waiting...\n");
  return process_wait((tid_t)pid);
}
int read (int fd, void *buffer, unsigned size){
  int bytes_read = -1; // -1 if error
  if(!fd){
    bytes_read=0;
    while(input_getc()!='\0'){
      bytes_read++;
    }
  }
  return bytes_read;
}
int write(int fd, const void *buffer, unsigned size)
{
  if(fd){
    putbuf(buffer, size);
    return size;
  }
  return -1;
}
int fibonacci(int n)
{
  int prev = 0;
  int cur = 1;
  int result=0;
  
  if(n < 0)
  {
     return -1; 
  }
  else if(n == 0)
  {
    return 0;
  }
  else if(n == 1 || n == 2)
  {
    return cur;
  }
  else{
    for(int i = 3; i <= n+1; i++)
    {
      result = prev + cur;
      prev = cur;
      cur = result;
    }
    
    return result;
  }
}
int max_of_four_int(int a, int b, int c, int d)
{
  int max = a;
  if (max < b)
  {
    max=b;
  }
  if(max < c)
  {
    max = c;
  }
  if(max <d)
  {
    max = d;
  }
  return max;
}
