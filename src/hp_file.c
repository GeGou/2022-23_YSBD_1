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
  BF_PrintError(BF_CreateFile(fileName));
  BF_PrintError(BF_OpenFile(fileName, &hp_info->fileDesc));
  int blocks = 0;
  // Βρισκω το πλήθος των block του αρχείου
  BF_PrintError(BF_GetBlockCounter(hp_info->fileDesc, &blocks));
  printf("%d\n", blocks);
  // Βρισκω το πρώτο block του αρχείου
  BF_PrintError(BF_GetBlock(hp_info->fileDesc, 0, hp_info->block));
  // Βρισκω το τελευταίο block του αρχείου
  BF_PrintError(BF_GetBlock(hp_info->fileDesc, blocks-1, hp_info->last_block));


  // memcpy(hp_info->block, &hp_info, sizeof(HP_info));  //Copying into memory
  BF_PrintError(BF_CloseFile(hp_info->fileDesc));
  return 0;
}

HP_info* HP_OpenFile(char *fileName){
  // παιρνει απο το μπλοκ 0 το info και το επιστρεφει  
  return NULL ;
}


int HP_CloseFile( HP_info* hp_info ){
  BF_ErrorCode error;
  error=BF_CloseFile(hp_info->fileDesc);
  if(error==0){   //BF_OK
    //apodesmeush
  }
  else{
    BF_PrintError(error);   //ektupwsh sfalmatos
  }

    if(error!=0){
      return -1;  //apetuxe h close
    }
    else{
      return 0;   //petuxe h close
    }
    free(hp_info);
}

int HP_InsertEntry(HP_info* hp_info, Record record){
    return 0;
}

int HP_GetAllEntries(HP_info* hp_info, int value){
   return 0;
}

