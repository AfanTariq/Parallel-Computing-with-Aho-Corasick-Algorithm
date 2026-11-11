#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0601
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
int main(void){
 DWORD bytes=0;GetLogicalProcessorInformationEx(RelationProcessorCore,NULL,&bytes);
 void *buf=malloc(bytes);if(!buf||!GetLogicalProcessorInformationEx(RelationProcessorCore,buf,&bytes))return 2;
 DWORD_PTR process_mask=0,system_mask=0;
 if(GetActiveProcessorGroupCount()!=1||!GetProcessAffinityMask(GetCurrentProcess(),&process_mask,&system_mask)){fprintf(stderr,"Only a single Windows processor group is supported.\n");free(buf);return 3;}
 MEMORYSTATUSEX m={0};m.dwLength=sizeof(m);if(!GlobalMemoryStatusEx(&m))return 4;
 SYSTEM_POWER_STATUS power;if(!GetSystemPowerStatus(&power))return 5;
 printf("{\"processor_groups\":1,\"available_ram_bytes\":%llu,\"total_ram_bytes\":%llu,\"ac_line_status\":%u,\"cores\":[",(unsigned long long)m.ullAvailPhys,(unsigned long long)m.ullTotalPhys,(unsigned)power.ACLineStatus);
 int comma=0,index=0;for(DWORD offset=0;offset<bytes;){
  PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX q=(void*)((char*)buf+offset);
  if(!q->Size||offset+q->Size>bytes)return 6;
  if(q->Relationship==RelationProcessorCore){
   for(WORD g=0;g<q->Processor.GroupCount;g++){
    if(q->Processor.GroupMask[g].Group!=0)return 7;
    ULONG_PTR mask=q->Processor.GroupMask[g].Mask&process_mask;if(!mask)continue;
    printf("%s{\"core_index\":%d,\"efficiency_class\":%u,\"logical_cpus\":[",comma?",":"",index++,(unsigned)q->Processor.EfficiencyClass);comma=1;
    int sep=0;for(unsigned b=0;b<64;b++)if(mask&(((ULONG_PTR)1)<<b)){printf("%s%u",sep?",":"",b);sep=1;}printf("]}");
   }
  }offset+=q->Size;
 }
 printf("]}\n");free(buf);return 0;
}
