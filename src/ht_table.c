#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf.h"
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


int HT_CreateFile(char *fileName, int buckets){
  HT_info ht_info;
  BF_Block* block;
  void* data;

  CALL_OR_DIE(BF_CreateFile(fileName));
  CALL_OR_DIE(BF_OpenFile(fileName, &ht_info.fileDesc));
  // Αρχικοποιώ/δεσμευσω block στην μνήμη
  BF_Block_Init(&block);
  CALL_OR_DIE(BF_AllocateBlock(ht_info.fileDesc, block));
  // Δείκτης που δειχνει στην αρχή των δεδομένων του block
  data = BF_Block_GetData(block);
  // Αρχικοποίηση μεταβλητών
  ht_info.numBuckets = buckets;
  ht_info.block = block;
  ht_info.ht_array = malloc(sizeof(int)*10);
  // ht_info.ht_array[buckets];
  // Αρχικοποίηση πίνακα κατακερματισμού
  for (int i = 0 ; i < buckets ; i++) {
    ht_info.ht_array[i] = i+1;    // Αριθμός και block
  }
  // Βρισκω το πλήθος των εγγραφων που χωρανε σε καθε block του αρχείου
  ht_info.records = (BF_BLOCK_SIZE - sizeof(HT_block_info)) / sizeof(Record);
  // Αποθήκευση struct hp_info στο 1ο block 
  memcpy(data, &ht_info, sizeof(HT_info));
  // Dirty και Unpin για να αποθηκευτεί στον δίσκο
  BF_Block_SetDirty(block);
  CALL_OR_DIE(BF_UnpinBlock(block));
  // Τέλος δημιουργίας του αρχείου
  BF_Block_Destroy(&block);
  CALL_OR_DIE(BF_CloseFile(ht_info.fileDesc));
  return 0;
}

HT_info* HT_OpenFile(char *fileName) {
  int fileDesc;
  BF_Block* block;
  HT_info* info;
  BF_ErrorCode code;
  
  // Βρίσκω και επιστρέφω το περιεχόμενο του 1ου block (block 0)
  BF_Block_Init(&block);
  CALL_OR_DIE(BF_OpenFile(fileName, &fileDesc));
  CALL_OR_DIE(BF_GetBlock(fileDesc, 0, block)); 
  return (HT_info *)BF_Block_GetData(block);
}


int HT_CloseFile(HT_info* HT_info) {
  CALL_OR_DIE(BF_CloseFile(HT_info->fileDesc));
  return 0;
}

int HT_InsertEntry(HT_info* ht_info, Record record){
  BF_Block *block_0, *block;
  HT_block_info bl_info, *bl_info_ptr;
  void *data;
  int flag = 0;   // flag για μη εγγραφή

  // Φέρνω στην ενδιάμεση μνήμη το 1ο block
  BF_Block_Init(&block_0);
  CALL_OR_DIE(BF_GetBlock(ht_info->fileDesc, 0, block_0));
  // printf("%d mod %ld = %d\n", record.id, ht_info->numBuckets, bucket);

  // Δημιουργώ blocks για καθε bucket
  // Αυτο θα τρεχει μια φορα και θα δημιουργει blocks για ολα τα buckets
  int blocks_num;
  BF_GetBlockCounter(ht_info->fileDesc, &blocks_num);
  if (blocks_num == 1) {
    BF_Block *new_block;
    for (int i = 0 ; i < ht_info->numBuckets ; i++) {
      BF_Block_Init(&new_block);
      CALL_OR_DIE(BF_AllocateBlock(ht_info->fileDesc, &new_block));
      // Φτιαχνω το HT_block_info
      bl_info.block_id = i+1;  // διοτι το block 0 ειναι το βασικό block
      bl_info.block_records = 0;
      bl_info.overflow_block = NULL;
      bl_info.prev_bl_id = 0;  
      // Αντιγράφουμε το struct στο τέλος του νέου block
      void *data = BF_Block_GetData(new_block);
      bl_info_ptr = data + ht_info->records * sizeof(Record);
      memcpy(bl_info_ptr, &bl_info, sizeof(HT_block_info));
      BF_Block_SetDirty(new_block);
      CALL_OR_DIE(BF_UnpinBlock(new_block));
      BF_Block_Destroy(&new_block);
    }
  }
  // Βρισκω το bucket στο οποιο θα "μπει" η εγγραφή record απο την Hash Function
  int bucket = record.id % ht_info->numBuckets;
  // Βρισκω ποιο block αντιστοιχεί στο bucket, που βρήκα απο πάνω, απο τον 
  // πινακα κατακερματισμού οπου κρατάει στοιχεία αντιστοίχισης bucket - block
  int block_num = ht_info->ht_array[bucket];
  // Αποθήκευση εγγραφής στο block
  BF_Block_Init(&block);
  void *data = BF_GetBlock(ht_info->fileDesc, block_num, block);
  bl_info_ptr = data + ht_info->records * sizeof(Record);
  int records = bl_info_ptr->block_records;
  // Ελενχος για κενό χωρο οπου θα μπει εγγραφή
  if (records < ht_info->records) {
    memcpy(data+records*sizeof(Record), &record, sizeof(Record));
  }
  // Δημιουργία block υπερχείλησης
  else {
      
  }
  BF_Block_SetDirty(block);
  CALL_OR_DIE(BF_UnpinBlock(block));
  BF_Block_Destroy(&block);

  // Τέλος επεξεργασίας 
  BF_Block_SetDirty(block_0);
  CALL_OR_DIE(BF_UnpinBlock(block_0));
  BF_Block_Destroy(&block_0);
  return block_num;
}

int HT_GetAllEntries(HT_info* ht_info, int value){
    return 0;
}