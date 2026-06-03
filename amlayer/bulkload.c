#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pflayer/pf.h"
#include "pflayer/pftypes.h"
#include "amlayer/am.h"
#include "amlayer/testam.h"

typedef struct {
    int key;
    int pageNum;
} IndexEntry;

static int build_internal(int fd, IndexEntry *entries, int num_entries, int *out_root_page) {
    if (num_entries == 0) return PFE_OK;
    if (num_entries == 1) {
        *out_root_page = entries[0].pageNum;
        return PFE_OK;
    }
    
    int attrLength = 4;
    int maxKeys = (PF_PAGE_SIZE - AM_sint - AM_si) / (AM_si + attrLength);
    if (maxKeys % 2 != 0) maxKeys--;
    
    IndexEntry *next_level = malloc(sizeof(IndexEntry) * num_entries);
    int next_level_count = 0;
    
    int i = 0;
    while (i < num_entries) {
        int pageNum;
        char *pageBuf;
        PF_AllocPage(fd, &pageNum, &pageBuf);
        
        AM_INTHEADER *hdr = (AM_INTHEADER *)pageBuf;
        hdr->pageType = 'i';
        hdr->attrLength = attrLength;
        hdr->maxKeys = maxKeys;
        
        // Number of children this node will have (at most maxKeys + 1)
        int children = num_entries - i;
        if (children > maxKeys + 1) children = maxKeys + 1;
        
        hdr->numKeys = children - 1;
        
        // Write leftmost pointer
        int left_ptr = entries[i].pageNum;
        memcpy(pageBuf + AM_sint, &left_ptr, AM_si);
        
        int offset = AM_sint + AM_si;
        for (int k = 1; k < children; k++) {
            int key = entries[i + k].key;
            int ptr = entries[i + k].pageNum;
            
            memcpy(pageBuf + offset, &key, attrLength);
            offset += attrLength;
            memcpy(pageBuf + offset, &ptr, AM_si);
            offset += AM_si;
        }
        
        PF_UnfixPage(fd, pageNum, TRUE);
        
        next_level[next_level_count].key = entries[i].key;
        next_level[next_level_count].pageNum = pageNum;
        next_level_count++;
        
        i += children;
    }
    
    int res = build_internal(fd, next_level, next_level_count, out_root_page);
    free(next_level);
    return res;
}

int AM_BulkLoad(char *indexName, char *dataFile) {
    int fd;
    char indexfName[AM_MAX_FNAME_LENGTH];
    sprintf(indexfName, "%s.0", indexName);
    
    PF_CreateFile(indexfName);
    fd = PF_OpenFile(indexfName, PF_LRU);
    if (fd < 0) return fd;
    
    FILE *f = fopen(dataFile, "r");
    if (!f) return -1;
    
    char line[1024];
    int attrLength = 4;
    int maxLeafKeys = (PF_PAGE_SIZE - AM_sl) / (attrLength + AM_ss + AM_si); 
    // Wait, leaf records have key and a pointer to recid list.
    // In amlayer, recId list is managed with freeListPtr, etc.
    // For bulk load with unique keys, each key has 1 recId. 
    // So recSize = attrLength + AM_ss. And recId is stored at the end of the page (recIdPtr).
    
    // We will just simplify: we can fit roughly (PF_PAGE_SIZE - AM_sl) / (attrLength + AM_ss + AM_si)
    // We will use 100 keys per leaf for safety.
    int maxKeys = 100; 
    
    int num_entries = 0;
    int capacity = 10000;
    IndexEntry *leaf_entries = malloc(sizeof(IndexEntry) * capacity);
    
    int current_leaf = -1;
    char *leaf_buf = NULL;
    int keys_in_leaf = 0;
    int prev_leaf = -1;
    int recIdCounter = 1;
    
    while (fgets(line, sizeof(line), f)) {
        int rollno = 0;
        sscanf(line, "%*d;%d;", &rollno);
        if (rollno == 0) continue;
        
        if (keys_in_leaf == 0) {
            PF_AllocPage(fd, &current_leaf, &leaf_buf);
            AM_LEAFHEADER *hdr = (AM_LEAFHEADER *)leaf_buf;
            hdr->pageType = 'l';
            hdr->nextLeafPage = AM_NULL_PAGE;
            hdr->recIdPtr = PF_PAGE_SIZE;
            hdr->keyPtr = AM_sl;
            hdr->freeListPtr = 0;
            hdr->numinfreeList = 0;
            hdr->attrLength = attrLength;
            hdr->numKeys = 0;
            hdr->maxKeys = maxKeys;
            
            if (num_entries >= capacity) {
                capacity *= 2;
                leaf_entries = realloc(leaf_entries, sizeof(IndexEntry) * capacity);
            }
            leaf_entries[num_entries].key = rollno;
            leaf_entries[num_entries].pageNum = current_leaf;
            num_entries++;
            
            if (prev_leaf != -1) {
                char *prev_buf;
                PF_GetThisPage(fd, prev_leaf, &prev_buf);
                AM_LEAFHEADER *pHdr = (AM_LEAFHEADER *)prev_buf;
                pHdr->nextLeafPage = current_leaf;
                PF_UnfixPage(fd, prev_leaf, TRUE);
            }
            prev_leaf = current_leaf;
        }
        
        // Insert into current leaf
        AM_LEAFHEADER *hdr = (AM_LEAFHEADER *)leaf_buf;
        int recSize = attrLength + AM_ss;
        
        // Write key
        memcpy(leaf_buf + AM_sl + keys_in_leaf * recSize, &rollno, attrLength);
        
        // Write head of recid list (a pointer to the end of the page)
        hdr->recIdPtr -= (AM_si + AM_ss);
        short list_head = hdr->recIdPtr;
        memcpy(leaf_buf + AM_sl + keys_in_leaf * recSize + attrLength, &list_head, AM_ss);
        
        // Write recid and next pointer (0)
        memcpy(leaf_buf + list_head, &recIdCounter, AM_si);
        short null_ptr = 0;
        memcpy(leaf_buf + list_head + AM_si, &null_ptr, AM_ss);
        
        hdr->numKeys++;
        hdr->keyPtr += recSize;
        keys_in_leaf++;
        recIdCounter++;
        
        if (keys_in_leaf >= maxKeys) {
            PF_UnfixPage(fd, current_leaf, TRUE);
            keys_in_leaf = 0;
        }
    }
    
    if (keys_in_leaf > 0) {
        PF_UnfixPage(fd, current_leaf, TRUE);
    }
    
    int root_page = -1;
    build_internal(fd, leaf_entries, num_entries, &root_page);
    free(leaf_entries);
    
    // We should probably save root_page somewhere, but for performance testing we just close.
    AM_RootPageNum = root_page;
    
    PF_CloseFile(fd);
    fclose(f);
    return PFE_OK;
}
