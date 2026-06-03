#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pflayer/pf.h"
#include "pflayer/pftypes.h"
#include "amlayer/am.h"
#include "amlayer/testam.h"

int AM_BulkLoad(char *indexName, char *dataFile);

void do_approach_a() {
    PF_ResetStats();
    
    // Approach A: Data file has all records, index is built by scanning data file
    AM_DestroyIndex("index_a", 0);
    AM_CreateIndex("index_a", 0, 'i', 4);
    
    int fd = PF_OpenFile("index_a.0", PF_LRU);
    if (fd < 0) { PF_PrintError("PF_OpenFile A"); exit(1); }
    
    FILE *f = fopen("../../data/student.txt", "r");
    char line[1024];
    int recId = 1;
    while (fgets(line, sizeof(line), f)) {
        int rollno = 0;
        sscanf(line, "%*d;%d;", &rollno);
        if (rollno == 0) continue;
        
        AM_InsertEntry(fd, 'i', 4, (char*)&rollno, recId);
        recId++;
    }
    fclose(f);
    PF_CloseFile(fd);
    
    printf("Approach A (Existing file -> Index):\n");
    printf("  Logical Reads: %d\n", PF_logical_reads);
    printf("  Logical Writes: %d\n", PF_logical_writes);
    printf("  Physical Reads: %d\n", PF_physical_reads);
    printf("  Physical Writes: %d\n\n", PF_physical_writes);
}

void do_approach_b() {
    PF_ResetStats();
    
    // Approach B: Incremental building - we read student.txt, pretend we are inserting into data file (which costs I/O), 
    // and then we insert into index.
    // For fair comparison with A, we can just measure the index insertion part since A only measures index insertion.
    // However, A scan might be very cheap. B's incremental insert means the tree is constantly re-traversed.
    // Wait, A and B are basically the same if we just look at index insertions! The only difference is in the real world,
    // B interleaves data insertions. Let's just run it the same as A for the index part to show it's identical unless we interleave data I/O.
    // Let's write dummy data pages to simulate incremental data file building.
    
    AM_DestroyIndex("index_b", 0);
    AM_CreateIndex("index_b", 0, 'i', 4);
    
    int fd = PF_OpenFile("index_b.0", PF_LRU);
    if (fd < 0) { PF_PrintError("PF_OpenFile B"); exit(1); }
    
    PF_CreateFile("dummy_data.db");
    int data_fd = PF_OpenFile("dummy_data.db", PF_LRU);
    
    FILE *f = fopen("../../data/student.txt", "r");
    char line[1024];
    int recId = 1;
    
    int dataPageNum = -1;
    char *dataPageBuf = NULL;
    int recs_in_page = 0;
    
    while (fgets(line, sizeof(line), f)) {
        int rollno = 0;
        sscanf(line, "%*d;%d;", &rollno);
        if (rollno == 0) continue;
        
        if (recs_in_page == 0) {
            if (dataPageNum != -1) PF_UnfixPage(data_fd, dataPageNum, TRUE);
            PF_AllocPage(data_fd, &dataPageNum, &dataPageBuf);
        }
        
        // Simulating data insertion
        recs_in_page++;
        if (recs_in_page >= 40) recs_in_page = 0; // 40 recs per page
        
        AM_InsertEntry(fd, 'i', 4, (char*)&rollno, recId);
        recId++;
    }
    
    if (dataPageNum != -1) PF_UnfixPage(data_fd, dataPageNum, TRUE);
    
    fclose(f);
    PF_CloseFile(data_fd);
    PF_CloseFile(fd);
    
    printf("Approach B (Incremental Data + Index):\n");
    printf("  Logical Reads: %d\n", PF_logical_reads);
    printf("  Logical Writes: %d\n", PF_logical_writes);
    printf("  Physical Reads: %d\n", PF_physical_reads);
    printf("  Physical Writes: %d\n\n", PF_physical_writes);
}

void do_approach_c() {
    PF_ResetStats();
    
    AM_DestroyIndex("index_c", 0);
    AM_BulkLoad("index_c", "../../data/student.txt");
    
    printf("Approach C (Bulk Load Sorted File):\n");
    printf("  Logical Reads: %d\n", PF_logical_reads);
    printf("  Logical Writes: %d\n", PF_logical_writes);
    printf("  Physical Reads: %d\n", PF_physical_reads);
    printf("  Physical Writes: %d\n\n", PF_physical_writes);
}

int main() {
    PF_Init();
    
    printf("--- Index Construction Performance Comparison ---\n\n");
    do_approach_a();
    do_approach_b();
    do_approach_c();
    
    return 0;
}
