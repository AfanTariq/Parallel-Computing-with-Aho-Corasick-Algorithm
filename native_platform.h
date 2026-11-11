#ifndef AC_NATIVE_PLATFORM_H
#define AC_NATIVE_PLATFORM_H
/* Windows x64 platform layer. No POSIX emulation or WSL at runtime. */
#ifdef _WIN32
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#include <sys/types.h>
#define ac_fseek _fseeki64
#define ac_ftell _ftelli64
static void ac_platform_fail(const char *s){fprintf(stderr,"platform error: %s (Windows error %lu)\n",s,(unsigned long)GetLastError());exit(2);}
static inline double ac_now(void){LARGE_INTEGER q,f;if(!QueryPerformanceFrequency(&f)||!QueryPerformanceCounter(&q))ac_platform_fail("performance counter");return (double)q.QuadPart/(double)f.QuadPart;}
static int sched_getcpu(void){return (int)GetCurrentProcessorNumber();}
static void ac_pin(int worker){
 const char *p=getenv("AC_CPU_LIST");if(!p||!*p)return;
 int index=0;while(index<worker){p=strchr(p,',');if(!p)ac_platform_fail("CPU list too short");p++;index++;}
 char *end=NULL;unsigned long cpu=strtoul(p,&end,10);
 if(end==p||(*end&&*end!=',')||cpu>=64)ac_platform_fail("invalid CPU list");
 DWORD_PTR mask=((DWORD_PTR)1)<<cpu;
 if(!SetThreadAffinityMask(GetCurrentThread(),mask))ac_platform_fail("thread affinity");
 if((unsigned long)GetCurrentProcessorNumber()!=cpu)ac_platform_fail("CPU sample differs after binding");
}
static ssize_t ac_getline(char **line,size_t *capacity,FILE *f){
 size_t n=0;int c;
 if(!*line||!*capacity){*capacity=128;*line=malloc(*capacity);if(!*line)ac_platform_fail("line allocation");}
 while((c=fgetc(f))!=EOF){
  if(n+1>=*capacity){if(*capacity>SIZE_MAX/2)ac_platform_fail("line too long");size_t cap=*capacity*2;char *q=realloc(*line,cap);if(!q)ac_platform_fail("line allocation");*line=q;*capacity=cap;}
  (*line)[n++]=(char)c;if(c=='\n')break;
 }
 if(!n&&c==EOF)return -1;
 (*line)[n]=0;return (ssize_t)n;
}
#define getline ac_getline
static inline uint64_t ac_working_set_kib(void){PROCESS_MEMORY_COUNTERS_EX m;memset(&m,0,sizeof(m));m.cb=sizeof(m);if(!GetProcessMemoryInfo(GetCurrentProcess(),(PROCESS_MEMORY_COUNTERS*)&m,sizeof(m)))ac_platform_fail("memory counters");return (uint64_t)m.WorkingSetSize/1024;}
static inline uint64_t ac_private_commit_kib(void){PROCESS_MEMORY_COUNTERS_EX m;memset(&m,0,sizeof(m));m.cb=sizeof(m);if(!GetProcessMemoryInfo(GetCurrentProcess(),(PROCESS_MEMORY_COUNTERS*)&m,sizeof(m)))ac_platform_fail("memory counters");return (uint64_t)m.PrivateUsage/1024;}
#else
#include <sched.h>
#include <unistd.h>
#define ac_fseek fseeko
#define ac_ftell ftello
static inline double ac_now(void){struct timespec t;if(clock_gettime(CLOCK_MONOTONIC,&t))exit(2);return t.tv_sec+t.tv_nsec*1e-9;}
static void ac_pin(int worker){(void)worker;}
static inline uint64_t ac_private_commit_kib(void){return 0;}
#endif
#endif
