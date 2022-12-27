#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf.h"
#include "sht_table.h"
#include "ht_table.h"
#include "record.h"

#define CALL_OR_DIE(call)     \
  {                           \
    BF_ErrorCode code = call; \
    if (code != BF_OK) {      \
      BF_PrintError(code);    \
      exit(code);             \
    }                         \
  }


int SHT_CreateSecondaryIndex(char *sfileName,  int buckets, char* fileName) {
  SHT_info sht_info;
  BF_Block* block;
  void* data;

  // δημιουργία ενός αρχείο πρωτεύοντος κατακερματισμού


  // δημιουργία ενός αρχείου δευτερεύοντος κατακερματισμού
  CALL_OR_DIE(BF_CreateFile(sfileName));
  CALL_OR_DIE(BF_OpenFile(sfileName, &sht_info.sfileDesc));
  // Αρχικοποιώ/δεσμευσω block στην μνήμη
  BF_Block_Init(&block);
  CALL_OR_DIE(BF_AllocateBlock(sht_info.sfileDesc, block));
  // Δείκτης που δειχνει στην αρχή των δεδομένων του block
  data = BF_Block_GetData(block);
  // Αρχικοποίηση μεταβλητών
  sht_info.snumBuckets = buckets;
  sht_info.sblock = block;
  sht_info.is_sht = 1;
  sht_info.sht_array = malloc(buckets*sizeof(int));
  // Αρχικοποίηση πίνακα κατακερματισμού
  for (int i = 0 ; i < buckets ; i++) {
    sht_info.sht_array[i] = 0;   // Αρχικά για κανεναν κουβα δεν εχει δημιουργηθεί block
  }
  // Βρισκω το πλήθος των ζευγών που χωράνε σε καθε block του αρχείου
  sht_info.pairs = (BF_BLOCK_SIZE - sizeof(SHT_block_info)) / sizeof(Pair);
  printf("%d\n", sht_info.pairs);
  // Αποθήκευση struct SHT_info στο 1ο block 
  memcpy(data, &sht_info, sizeof(SHT_info));
  // Dirty και Unpin για να αποθηκευτεί στον δίσκο
  BF_Block_SetDirty(block);
  CALL_OR_DIE(BF_UnpinBlock(block));
  // Τέλος δημιουργίας του αρχείου
  BF_Block_Destroy(&block);
  CALL_OR_DIE(BF_CloseFile(sht_info.sfileDesc));
  return 0;
}

SHT_info* SHT_OpenSecondaryIndex(char *indexName) {
  int sfileDesc;
  BF_Block* block;
  SHT_info* info = malloc(sizeof(SHT_info));
  
  // Βρίσκω και επιστρέφω το περιεχόμενο του 1ου block (block 0)
  BF_Block_Init(&block);
  CALL_OR_DIE(BF_OpenFile(indexName, &sfileDesc));
  CALL_OR_DIE(BF_GetBlock(sfileDesc, 0, block));    // ΙΣΩΣ ΝΑ ΜΗΝ ΘΕΛΕΙ 0
  char* data = BF_Block_GetData(block); 
  memcpy(data, &sfileDesc, sizeof(int));
  memcpy(info, data, sizeof(SHT_info));
  if (info->is_sht == 0) {
    return NULL;
  }
  CALL_OR_DIE(BF_UnpinBlock(block)); 
  BF_Block_Destroy(&block);
  return info;
}


int SHT_CloseSecondaryIndex( SHT_info* SHT_info ) {
  CALL_OR_DIE(BF_CloseFile(SHT_info->sfileDesc));
  free(SHT_info->sht_array);
  free(SHT_info);
  return 0;
}

int SHT_SecondaryInsertEntry(SHT_info* sht_info, Record record, int block_id) {

}

int SHT_SecondaryGetAllEntries(HT_info* ht_info, SHT_info* sht_info, char* name) {

}



