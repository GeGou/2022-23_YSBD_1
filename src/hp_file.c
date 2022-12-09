#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf.h"
#include "hp_file.h"
#include "record.h"

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
  BF_CreateFile(fileName);
  BF_OpenFile(fileName, &hp_info->fileDesc);
  hp_info->last_block_id = 0;
  hp_info->records = 10000;
  BF_AllocateBlock(hp_info->fileDesc, hp_info->block);

  // BF_GetBlock(hp_info->fileDesc, hp_info->last_block_id, hp_info->block);
  // BF_Block_GetData(hp_info->block);
  
  BF_CloseFile(hp_info->fileDesc);
  return 0;
}

HP_info* HP_OpenFile(char *fileName){
    return NULL ;
}


int HP_CloseFile( HP_info* hp_info ){
    return 0;
}

int HP_InsertEntry(HP_info* hp_info, Record record){
    return 0;
}

int HP_GetAllEntries(HP_info* hp_info, int value){
   return 0;
}

