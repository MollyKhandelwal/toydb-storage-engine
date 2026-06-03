#ifndef SLOTTED_PAGE_H
#define SLOTTED_PAGE_H

#include "pflayer/pf.h"
#include "pflayer/pftypes.h"

// We store the header at the very end of the page (offset 4096 - sizeof(SP_Header))
typedef struct {
    short freeSpaceOffset; // Where free space starts (grows upwards from 0)
    short numSlots;        // Number of slots allocated
} SP_Header;

typedef struct {
    short recordOffset;    // Offset of the record from the beginning of the page
    short recordLength;    // Length of the record, -1 if empty/deleted
} SP_Slot;

#define SP_DELETED -1

int SP_InitFile(char *fname);
int SP_OpenFile(char *fname);
int SP_InsertRecord(int fd, char *record, short length, int *pageNum, int *slotNum);
int SP_DeleteRecord(int fd, int pageNum, int slotNum);
int SP_GetRecord(int fd, int pageNum, int slotNum, char *out_record, short *length);

#endif
