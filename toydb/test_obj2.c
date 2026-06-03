#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pflayer/pf.h"
#include "pflayer/pftypes.h"
#include "slotted_page.h"

int main() {
    PF_Init();
    
    char *fname = "slotted_test.db";
    PF_DestroyFile(fname); // Ignore error if it doesn't exist
    
    if (SP_InitFile(fname) != PFE_OK) {
        PF_PrintError("SP_InitFile");
        return 1;
    }
    
    int fd = PF_OpenFile(fname, PF_LRU);
    if (fd < 0) {
        PF_PrintError("PF_OpenFile");
        return 1;
    }
    
    FILE *f = fopen("../../data/student.txt", "r");
    if (!f) {
        perror("fopen student.txt");
        return 1;
    }
    
    char line[1024];
    int num_records = 0;
    int total_data_bytes = 0;
    
    while (fgets(line, sizeof(line), f)) {
        int len = strlen(line);
        if (len > 0 && line[len-1] == '\n') {
            line[len-1] = '\0';
            len--;
        }
        
        int pageNum, slotNum;
        if (SP_InsertRecord(fd, line, len, &pageNum, &slotNum) != PFE_OK) {
            PF_PrintError("SP_InsertRecord");
            return 1;
        }
        num_records++;
        total_data_bytes += len;
    }
    fclose(f);
    
    // Get total number of pages used
    extern struct PFftab_ele PFftab[];
    int total_pages = PFftab[fd].hdr.numpages;
    
    PF_CloseFile(fd);
    
    printf("\n--- Performance Metrics ---\n");
    printf("Total Records Inserted: %d\n", num_records);
    printf("Total Data Bytes: %d\n", total_data_bytes);
    
    double slotted_util = (double)total_data_bytes / (total_pages * PF_PAGE_SIZE);
    
    printf("\nSpace Utilization Comparison:\n");
    printf("----------------------------------------------------\n");
    printf("| Method               | Total Pages | Utilization |\n");
    printf("----------------------------------------------------\n");
    printf("| Slotted Page         | %11d | %9.2f%% |\n", total_pages, slotted_util * 100);
    
    int max_lengths[] = {50, 100, 150};
    for (int i = 0; i < 3; i++) {
        int max_len = max_lengths[i];
        int recs_per_page = PF_PAGE_SIZE / max_len;
        int static_pages = (num_records + recs_per_page - 1) / recs_per_page;
        double static_util = (double)total_data_bytes / ((double)static_pages * PF_PAGE_SIZE);
        
        printf("| Static (Max = %3d)   | %11d | %9.2f%% |\n", max_len, static_pages, static_util * 100);
    }
    printf("----------------------------------------------------\n");
    
    return 0;
}
