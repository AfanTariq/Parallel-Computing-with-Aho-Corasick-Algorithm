#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>
#include <time.h>
#include <errno.h>
#include <omp.h>
#include "native_platform.h"
#ifdef WITH_MPI
#include <mpi.h>
#endif
/* Completed byte DFA; duplicate patterns count as distinct signatures. */
typedef struct { uint32_t next[256]; uint64_t out; } State;
typedef struct { State *s; size_t n,cap,maxlen,patterns; } AC;
static void fail(const char *s) { fprintf(stderr,"error: %s\n",s);
#ifdef WITH_MPI
 int initialized=0; MPI_Initialized(&initialized); if(initialized) MPI_Abort(MPI_COMM_WORLD,1);
#endif
 exit(1); }
static void *alloc(size_t n) { void *p=malloc(n?n:1); if(!p) fail("out of memory"); return p; }
static size_t number(const char *s) { char *end;errno=0;unsigned long long x=strtoull(s,&end,10);if(errno||!*s||*s=='-'||*end||x>SIZE_MAX)fail("invalid numeric argument");return (size_t)x; }
#ifndef WITH_MPI
static double now(void) { return ac_now(); }
#endif
static uint32_t node(AC *a) {
 if(a->n>=UINT32_MAX)fail("too many states");
 if(a->n==a->cap){size_t c=a->cap?a->cap*2:64;if(c<a->cap||c>SIZE_MAX/sizeof(State))fail("automaton too large");void *p=realloc(a->s,c*sizeof(State));if(!p)fail("out of memory");a->s=p;a->cap=c;}
 memset(&a->s[a->n],0,sizeof(State));return (uint32_t)a->n++;
}
static int hexval(int c){if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;if(c>='A'&&c<='F')return c-'A'+10;return -1;}
static AC build(const char *path) {
 AC a={0};node(&a);FILE *f=fopen(path,"r");if(!f)fail("cannot open patterns file");char *line=NULL;size_t cap=0;ssize_t k;
 while((k=getline(&line,&cap,f))>=0){while(k&&(line[k-1]=='\n'||line[k-1]=='\r'))k--;if(!k)fail("empty pattern line");if(k%2)fail("pattern must be even-length hexadecimal");uint32_t q=0;
  for(ssize_t i=0;i<k;i+=2){int hi=hexval(line[i]),lo=hexval(line[i+1]);if(hi<0||lo<0)fail("invalid hexadecimal pattern");unsigned b=(unsigned)(hi*16+lo);if(!a.s[q].next[b]){uint32_t v=node(&a);a.s[q].next[b]=v;}q=a.s[q].next[b];}
  if(a.s[q].out==UINT64_MAX) { fail("output overflow"); }
  a.s[q].out++;a.patterns++;if((size_t)k/2>a.maxlen)a.maxlen=(size_t)k/2;
 }
 if(ferror(f)) { fail("pattern read failure"); }
 free(line);fclose(f);if(!a.patterns)fail("empty pattern set");
 uint32_t *queue=alloc(a.n*sizeof(uint32_t)),*failure=calloc(a.n,sizeof(uint32_t));if(!failure)fail("out of memory");size_t head=0,tail=0;
 for(unsigned b=0;b<256;b++)if(a.s[0].next[b])queue[tail++]=a.s[0].next[b];
 while(head<tail){uint32_t v=queue[head++];for(unsigned b=0;b<256;b++){uint32_t u=a.s[v].next[b];if(u){failure[u]=a.s[failure[v]].next[b];if(UINT64_MAX-a.s[u].out<a.s[failure[u]].out)fail("output overflow");a.s[u].out+=a.s[failure[u]].out;queue[tail++]=u;}else a.s[v].next[b]=a.s[failure[v]].next[b];}}
 free(queue);free(failure);return a;
}
static unsigned char *readall(const char *path,size_t *n){FILE *f=fopen(path,"rb");if(!f)fail("cannot open input file");if(ac_fseek(f,0,SEEK_END))fail("input must be seekable");int64_t z=ac_ftell(f);if(z<0)fail("input size failure");rewind(f);*n=(size_t)z;unsigned char *t=alloc(*n);if(fread(t,1,*n,f)!=*n)fail("input read failure");fclose(f);return t;}
static uint64_t scan(const AC *a,const unsigned char *t,size_t f,size_t end){size_t overlap=a->maxlen-1,start=f>overlap?f-overlap:0;uint32_t q=0;uint64_t count=0;for(size_t i=start;i<end;i++){q=a->s[q].next[t[i]];if(i>=f)count+=a->s[q].out;}return count;}
static size_t boundary(size_t n,size_t k,size_t p){return n/p*k+(n%p<k?n%p:k);}
#ifndef WITH_MPI
static uint64_t search(const AC *a, const AC *copies, int priv, int serial,
 const unsigned char *text, size_t n, size_t chunk, const char *schedule,
 int *actual, int *cpus) {
 uint64_t matches=0;
 if(serial){*actual=1;ac_pin(0);cpus[0]=sched_getcpu();return scan(a,text,0,n);}
 if(!strcmp(schedule,"block")) {
#pragma omp parallel reduction(+:matches)
  {int id=omp_get_thread_num(),p=omp_get_num_threads();ac_pin(id);cpus[id]=sched_getcpu();
#pragma omp single
   *actual=p;
   const AC *use=priv?&copies[id]:a;
   matches+=scan(use,text,boundary(n,(size_t)id,(size_t)p),boundary(n,(size_t)id+1,(size_t)p));}
 } else {size_t chunks=n/chunk+(n%chunk!=0);
#pragma omp parallel reduction(+:matches)
  {int id=omp_get_thread_num();ac_pin(id);cpus[id]=sched_getcpu();
#pragma omp single
   *actual=omp_get_num_threads();
   const AC *use=priv?&copies[id]:a;
#pragma omp for schedule(runtime)
   for(size_t j=0;j<chunks;j++){size_t f=j*chunk,e=n-f<chunk?n:f+chunk;matches+=scan(use,text,f,e);}}
 }
 return matches;
}
#endif
int main(int argc,char **argv){
 int rank=0;
#ifdef WITH_MPI
 int ranks=1;
#endif
#ifdef WITH_MPI
 MPI_Init(&argc,&argv);MPI_Comm_rank(MPI_COMM_WORLD,&rank);MPI_Comm_size(MPI_COMM_WORLD,&ranks);
#endif
 if(argc!=7)fail("usage: ac_bench MODE WORKERS CHUNK_BYTES SCHEDULE INPUT PATTERNS_HEX");
 const char *mode=argv[1],*schedule=argv[4];size_t w=number(argv[2]),chunk=number(argv[3]);if(!w||w>INT_MAX)fail("invalid worker count");
 int serial=!strcmp(mode,"serial"),shared=!strcmp(mode,"shared"),priv=!strcmp(mode,"private"),mpi=!strcmp(mode,"mpi");if(!serial&&!shared&&!priv&&!mpi)fail("unknown mode");if(serial&&w!=1)fail("serial requires one worker");
 if(strcmp(schedule,"block")&&strcmp(schedule,"static")&&strcmp(schedule,"dynamic")&&strcmp(schedule,"guided"))fail("unknown schedule");
 if(strcmp(schedule,"block")&&!chunk)fail("chunk bytes must be positive");
#ifndef WITH_MPI
 if(mpi)fail("MPI binary required; run make mpi");
#else
 if(!mpi)fail("MPI binary supports mpi mode only");
 if((size_t)ranks!=w)fail("MPI ranks must equal workers");
 if(strcmp(schedule,"block"))fail("MPI baseline supports block schedule only");
#endif
 ac_pin(rank);AC a=build(argv[6]);size_t n=0;unsigned char *text=readall(argv[5],&n);
 /* Conservative guard against accumulation overflow in all reductions. */
 if(n && a.patterns>UINT64_MAX/n)fail("possible match-count overflow");
 AC *copies=NULL;uint64_t matches=0,warm_matches=0;int actual=1;double elapsed=0;
 const char *warmenv=getenv("AC_WARMUP_SCANS");size_t warmups=warmenv?number(warmenv):1;
 if(warmups>100)fail("warm-up scan count must be at most 100");
 int *cpus=alloc(w*sizeof(int));for(size_t i=0;i<w;i++)cpus[i]=-1;
 omp_set_dynamic(0);omp_set_num_threads((int)w);
 /* Both layouts prepare their measured storage in an untimed parallel region.
    Private needs p copies, shared needs one; copying costs remain excluded.
    This symmetry and same-process scans reduce, but do not eliminate, memory effects. */
 if(shared||priv){size_t count=priv?w:1;copies=calloc(count,sizeof(AC));if(!copies)fail("out of memory");
#pragma omp parallel
  {
#pragma omp single
   actual=omp_get_num_threads();
   int id=omp_get_thread_num();ac_pin(id);if(priv||id==0){size_t idx=priv?(size_t)id:0;
    copies[idx]=a;copies[idx].s=alloc(a.n*sizeof(State));memcpy(copies[idx].s,a.s,a.n*sizeof(State));}
  }
  if((size_t)actual!=w)fail("preparation worker count mismatch");
  free(a.s);a.s=NULL;
  if(shared){a=copies[0];free(copies);copies=NULL;}
 }
#ifdef WITH_MPI
 size_t f=boundary(n,(size_t)rank,w),e=boundary(n,(size_t)rank+1,w);
 for(size_t j=0;j<warmups;j++){uint64_t local=scan(&a,text,f,e),total=0;
  MPI_Allreduce(&local,&total,1,MPI_UINT64_T,MPI_SUM,MPI_COMM_WORLD);
  if(j&&total!=warm_matches)fail("warm-up count mismatch");
  warm_matches=total;
 }
 MPI_Barrier(MPI_COMM_WORLD);double begin=MPI_Wtime();
 uint64_t local=scan(&a,text,f,e);MPI_Allreduce(&local,&matches,1,MPI_UINT64_T,MPI_SUM,MPI_COMM_WORLD);
 double local_time=MPI_Wtime()-begin;
 MPI_Reduce(&local_time,&elapsed,1,MPI_DOUBLE,MPI_MAX,0,MPI_COMM_WORLD);actual=ranks;
 cpus[0]=sched_getcpu();
#else
 if(!strcmp(schedule,"static"))omp_set_schedule(omp_sched_static,1);
 else if(!strcmp(schedule,"dynamic"))omp_set_schedule(omp_sched_dynamic,1);
 else if(!strcmp(schedule,"guided"))omp_set_schedule(omp_sched_guided,1);
 for(size_t j=0;j<warmups;j++){
  uint64_t count=search(&a,copies,priv,serial,text,n,chunk,schedule,&actual,cpus);
  if((size_t)actual!=w)fail("warm-up worker count mismatch");
  if(j&&count!=warm_matches)fail("warm-up count mismatch");
  warm_matches=count;
 }
 double begin=now();
 matches=search(&a,copies,priv,serial,text,n,chunk,schedule,&actual,cpus);
 elapsed=now()-begin;
 if((size_t)actual!=w)fail("measured worker count mismatch");
#endif
 if(warmups&&matches!=warm_matches)fail("measured count differs from warm-up");
 if(!rank){printf("{\"mode\":\"%s\",\"workers_requested\":%zu,\"workers_actual\":%d,\"schedule\":\"%s\",\"chunk_bytes\":%zu,\"text_bytes\":%zu,\"patterns\":%zu,\"states\":%zu,\"max_pattern_bytes\":%zu,\"transition_bytes_per_copy\":%zu,\"state_bytes_per_copy\":%zu,\"matches\":%llu,\"seconds\":%.9f,\"protocol\":\"native-windows-v1\",\"warmup_scans\":%zu,\"warmup_matches\":%llu,\"cpu_sample\":[",mode,w,actual,schedule,chunk,n,a.patterns,a.n,a.maxlen,a.n*256*sizeof(uint32_t),a.n*sizeof(State),(unsigned long long)matches,elapsed,warmups,(unsigned long long)warm_matches);
 for(int i=0;i<(mpi?1:actual);i++){printf("%s%d",i?",":"",cpus[i]);}
 printf("]}\n");}
 if(copies){for(size_t i=0;i<w;i++)free(copies[i].s);free(copies);}free(text);free(a.s);free(cpus);
#ifdef WITH_MPI
 MPI_Finalize();
#endif
 return 0;
}
