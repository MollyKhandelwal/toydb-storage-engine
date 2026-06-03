#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "slotted_page.h"

#define SP_PAGE_HEADER(pagebuf) ((SP_Header *)((pagebuf) + PF_PAGE_SIZE - sizeof(SP_Header)))
#define SP_SLOT(pagebuf, slotNum) ((SP_Slot *)((pagebuf) + PF_PAGE_SIZE - sizeof(SP_Header) - ((slotNum) + 1) * sizeof(SP_Slot)))

int SP_InitFile(char *fname) {
    int error = PF_CreateFile(fname);
    if (error != PFE_OK) return error;
    
    // Allocate first page
    int fd = PF_OpenFile(fname, PF_LRU);
    if (fd < 0) return fd;
    
    int pageNum;
    char *pagebuf;
    if ((error = PF_AllocPage(fd, &pageNum, &pagebuf)) != PFE_OK) {
        PF_CloseFile(fd);
        return error;
    }
    
    SP_Header *hdr = SP_PAGE_HEADER(pagebuf);
    hdr->freeSpaceOffset = 0;
    hdr->numSlots = 0;
    
    PF_UnfixPage(fd, pageNum, TRUE);
    PF_CloseFile(fd);
    return PFE_OK;
}

int SP_OpenFile(char *fname) {
    return PF_OpenFile(fname, PF_LRU);
}

int SP_InsertRecord(int fd, char *record, short length, int *pageNum, int *slotNum) {
    int error;
    extern PFftab_ele PFftab[];
    int currPage = PFftab[fd].hdr.numpages - 1;
    if (currPage < 0) currPage = 0;
    char *pagebuf;
    SP_Header *hdr;
    
    while (1) {
        error = PF_GetThisPage(fd, currPage, &pagebuf);
        if (error == PFE_EOF || error == PFE_INVALIDPAGE) {
            if ((error = PF_AllocPage(fd, &currPage, &pagebuf)) != PFE_OK) {
                return error;
            }
            hdr = SP_PAGE_HEADER(pagebuf);
            hdr->freeSpaceOffset = 0;
            hdr->numSlots = 0;
        } else if (error != PFE_OK) {
            return error;
        } else {
            hdr = SP_PAGE_HEADER(pagebuf);
        }
        
        int emptySlot = -1;
        for (int i = 0; i < hdr->numSlots; i++) {
            SP_Slot *slot = SP_SLOT(pagebuf, i);
            if (slot->recordLength == SP_DELETED) {
                emptySlot = i;
                break;
            }
        }
        
        int requiredSpace = length;
        if (emptySlot == -1) {
            requiredSpace += sizeof(SP_Slot);
        }
        
        int availableSpace = PF_PAGE_SIZE - sizeof(SP_Header) - (hdr->numSlots * sizeof(SP_Slot)) - hdr->freeSpaceOffset;
        
        if (availableSpace >= requiredSpace) {
            int targetSlot = (emptySlot != -1) ? emptySlot : hdr->numSlots;
            SP_Slot *slot = SP_SLOT(pagebuf, targetSlot);
            
            slot->recordOffset = hdr->freeSpaceOffset;
            slot->recordLength = length;
            
            memcpy(pagebuf + hdr->freeSpaceOffset, record, length);
            hdr->freeSpaceOffset += length;
            
            if (emptySlot == -1) {
                hdr->numSlots++;
            }
            
            *pageNum = currPage;
            *slotNum = targetSlot;
            
            PF_UnfixPage(fd, currPage, TRUE);
            return PFE_OK;
        } else {
            PF_UnfixPage(fd, currPage, FALSE);
            currPage++;
        }
    }
}

int SP_DeleteRecord(int fd, int pageNum, int slotNum) {
    char *pagebuf;
    int error;
    if ((error = PF_GetThisPage(fd, pageNum, &pagebuf)) != PFE_OK) {
        return error;
    }
    
    SP_Header *hdr = SP_PAGE_HEADER(pagebuf);
    if (slotNum < 0 || slotNum >= hdr->numSlots) {
        PF_UnfixPage(fd, pageNum, FALSE);
        return -1; 
    }
    
    SP_Slot *slot = SP_SLOT(pagebuf, slotNum);
    slot->recordLength = SP_DELETED;
    
    PF_UnfixPage(fd, pageNum, TRUE);
    return PFE_OK;
}

int SP_GetRecord(int fd, int pageNum, int slotNum, char *out_record, short *length) {
    char *pagebuf;
    int error;
    if ((error = PF_GetThisPage(fd, pageNum, &pagebuf)) != PFE_OK) {
        return error;
    }
    
    SP_Header *hdr = SP_PAGE_HEADER(pagebuf);
    if (slotNum < 0 || slotNum >= hdr->numSlots) {
        PF_UnfixPage(fd, pageNum, FALSE);
        return -1;
    }
    
    SP_Slot *slot = SP_SLOT(pagebuf, slotNum);
    if (slot->recordLength == SP_DELETED) {
        PF_UnfixPage(fd, pageNum, FALSE);
        return -1;
    }
    
    memcpy(out_record, pagebuf + slot->recordOffset, slot->recordLength);
    *length = slot->recordLength;
    
    PF_UnfixPage(fd, pageNum, FALSE);
    return PFE_OK;
}
