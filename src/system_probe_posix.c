#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(void) {
    long cpus = sysconf(_SC_NPROCESSORS_ONLN);
    long pages = sysconf(_SC_PHYS_PAGES);
    long page_size = sysconf(_SC_PAGESIZE);
    unsigned long long ram = (pages > 0 && page_size > 0)
        ? (unsigned long long)pages * (unsigned long long)page_size : 0;
    printf("{\"platform\":\"posix\",\"logical_cpus\":%ld,\"total_ram_bytes\":%llu,\"ac_line_status\":-1}\n",
           cpus > 0 ? cpus : 0, ram);
    return 0;
}
