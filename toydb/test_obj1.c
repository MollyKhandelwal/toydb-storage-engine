#include <stdio.h>
#include <stdlib.h>
#include "pflayer/pf.h"
#include "pflayer/pftypes.h"

#define NUM_PAGES 100

void simulate_workload(int fd, int mix_read_percent) {
    int i, pagenum;
    char *pagebuf;
    int is_write;
    
    PF_ResetStats();
    
    // Pattern that shows difference between LRU and MRU:
    // Sequential scan of a working set larger than buffer size.
    // LRU will fault on every access, MRU will hit on the most recently faulted.
    // Actually, a simple loop with repeated accesses to a working set.
    for (i = 0; i < 1000; i++) {
        // Access pages 0 to NUM_PAGES-1 sequentially, looping around
        pagenum = i % NUM_PAGES;
        
        is_write = ((rand() % 100) < (100 - mix_read_percent));
        
        if (PF_GetThisPage(fd, pagenum, &pagebuf) != PFE_OK) {
            PF_PrintError("PF_GetThisPage failed");
            exit(1);
        }
        
        // simulate some work
        if (is_write) {
            pagebuf[0] = 'W';
        }
        
        if (PF_UnfixPage(fd, pagenum, is_write) != PFE_OK) {
            PF_PrintError("PF_UnfixPage failed");
            exit(1);
        }
    }
}

int main() {
    int fd, buf_size, mix, strategy;
    char *fname = "test_obj1.db";
    FILE *out;
    int pagenum;
    char *pagebuf;
    
    PF_Init();
    
    out = fopen("obj1_stats.csv", "w");
    if (!out) {
        perror("fopen");
        return 1;
    }
    
    fprintf(out, "Strategy,BufferSize,ReadMixPercent,LogicalReads,LogicalWrites,PhysicalReads,PhysicalWrites\n");
    
    // Create and populate the file
    if (PF_CreateFile(fname) != PFE_OK) {
        PF_PrintError("PF_CreateFile");
        return 1;
    }
    if ((fd = PF_OpenFile(fname, PF_LRU)) < 0) {
        PF_PrintError("PF_OpenFile");
        return 1;
    }
    for (int i = 0; i < NUM_PAGES; i++) {
        if (PF_AllocPage(fd, &pagenum, &pagebuf) != PFE_OK) {
            PF_PrintError("PF_AllocPage");
            return 1;
        }
        if (PF_UnfixPage(fd, pagenum, TRUE) != PFE_OK) {
            PF_PrintError("PF_UnfixPage alloc");
            return 1;
        }
    }
    PF_CloseFile(fd);
    
    int buf_sizes[] = {10, 20, 50};
    int mixes[] = {100, 75, 50, 25, 0}; // 100% read, 75% read, ...
    
    for (int s = 0; s < 2; s++) { // 0: LRU, 1: MRU
        for (int b = 0; b < 3; b++) {
            PF_MAX_BUFS = buf_sizes[b];
            for (int m = 0; m < 5; m++) {
                if ((fd = PF_OpenFile(fname, s)) < 0) {
                    PF_PrintError("PF_OpenFile");
                    return 1;
                }
                
                simulate_workload(fd, mixes[m]);
                
                fprintf(out, "%s,%d,%d,%d,%d,%d,%d\n",
                    s == PF_LRU ? "LRU" : "MRU",
                    buf_sizes[b],
                    mixes[m],
                    PF_logical_reads,
                    PF_logical_writes,
                    PF_physical_reads,
                    PF_physical_writes
                );
                
                PF_CloseFile(fd);
            }
        }
    }
    
    fclose(out);
    PF_DestroyFile(fname);
    printf("Stats generated in obj1_stats.csv\n");
    return 0;
}
