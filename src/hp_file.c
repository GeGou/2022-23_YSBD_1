#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf.h"
#include "hp_file.h"
#include "record.h"

#define RECORDS_NUM

#define CALL_BF(call)       \
{                           \
  BF_ErrorCode code = call; \
  if (code != BF_OK) {         \
    BF_PrintError(code);    \
    return HP_ERROR;        \
  }                         \
}

int HP_CreateFile(char *fileName) {
  HP_info *hp_info = malloc(sizeof(HP_info));
  BF_Block* block;
  void* data;

  BF_PrintError(BF_CreateFile(fileName));
  BF_PrintError(BF_OpenFile(fileName, &hp_info->fileDesc));
  
  // Αρχικοποιώ και δεσμεύω το 1ο block στην μνήμη
  BF_Block_Init(&block);
  // Δεσμεύω νέο block στον δίσκο
  BF_PrintError(BF_AllocateBlock(hp_info->fileDesc, block));
  // Δείκτης που δειχνει στην αρχή των δεδομένων του block
  data = BF_Block_GetData(block);
  hp_info->block = block;
  hp_info->last_block = block;
  
  // Βρισκω το πλήθος των εγγραφων που χωρανε σε καθε block του αρχείου
  hp_info->records = (BF_BLOCK_SIZE - sizeof(HP_block_info)) / sizeof(Record);

  // Βρισκω το πλήθος των block του αρχείου
  // int blocks = 0;
  // BF_PrintError(BF_GetBlockCounter(hp_info->fileDesc, &blocks));
  // printf("%d\n", blocks);

  memcpy(data, &hp_info, sizeof(HP_info));  //Copying into memory
  // Dirty και Unpin για να αποθηκευτεί στον δίσκο
  BF_Block_SetDirty(block);
  BF_PrintError(BF_UnpinBlock(block));
  // Τερματίζω την επεξεργασια του αρχείου
  BF_PrintError(BF_CloseFile(hp_info->fileDesc));
  BF_PrintError(BF_Close());
  return 0;
}

HP_info* HP_OpenFile(char *fileName){
  
  // παιρνει απο το μπλοκ 0 το info και το επιστρεφει  
  return NULL ;
}

int HP_CloseFile(HP_info* hp_info){
  BF_PrintError(BF_CloseFile(hp_info->fileDesc));
  free(hp_info);
  return 0;   //petuxe h close
}

int HP_InsertEntry(HP_info* hp_info, Record record){
    return 0;
}

int HP_GetAllEntries(HP_info* hp_info, int value){
   return 0;
}

