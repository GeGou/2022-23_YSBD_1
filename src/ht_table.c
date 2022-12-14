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
  BF_Block *block, *last_block;
  HT_block_info bl_info;
  HT_block_info *bl_info_ptr;
  void *data;
  int flag = 0;   // flag για μη εγγραφή

  // Φέρνω στην ενδιάμεση μνήμη το 1ο block
  BF_Block_Init(&block);
  CALL_OR_DIE(BF_GetBlock(ht_info->fileDesc, 0, block));
  // Βρισκω το bucket στο οποιο θα "μπει" η εγγραφή record
  int bucket = record.id % ht_info->numBuckets;
  // printf("%d mod %ld = %d\n", record.id, ht_info->numBuckets, bucket);
  
  // Βρισκω ποιο block αντιστοιχεί στο bucket, που βρήκα πιο πάνω, απο τον 
  // πινακα κατακερματισμού οπου κρατάει στοιχεία αντιστοίχισης bucket - block
  int bl = ht_info->ht_array[bucket];


























  // // Ελενχος για το αν υπαρχει block για εγγραφές, δηλαδή παραπάνω απο ένα block
  // if (ht_info->last_block_id != 0) {
  //   // Φέρνω το τελευταιο block στην ενδιάμεση μνήμη
  //   BF_Block_Init(&last_block);
  //   CALL_OR_DIE(BF_GetBlock(hp_info->fileDesc, hp_info->last_block_id, last_block));
  //   // Δείκτης στα δεδομένα του τελευταίου block
  //   data = BF_Block_GetData(last_block);
  //   // Δείκτης στο τέλος του block όπου βρισκεται το struct HP_block_info
  //   bl_info_ptr = data + ht_info->records * sizeof(Record);
  //   // Εύρεση πλήθους εγγραφων στο block
  //   int records = bl_info_ptr->block_records;
  //   // Ελενχος για κενό χωρο οπου θα μπει εγγραφή
  //   if (records < hp_info->records) {
  //     memcpy(data+records*sizeof(Record), &record, sizeof(Record));
  //     bl_info_ptr->block_records++;
  //     BF_Block_SetDirty(last_block);
  //     flag = 1;
  //   }
  //   CALL_OR_DIE(BF_UnpinBlock(last_block));
  //   BF_Block_Destroy(&last_block);
  // }
  // // Δέσμευση νέου block
  // if (flag == 0) {
  //   BF_Block_Init(&last_block);
  //   CALL_OR_DIE(BF_AllocateBlock(hp_info->fileDesc, last_block));
  //   // Ενημερωση σχετικα με το πλεον τελευταιό block
  //   ht_info->last_block_id++;
  //   // Αρχικοποίηση μεταβλητων του struct HP_block_info
  //   bl_info.block_records = 1;
  //   // Δείκτης στα δεδομενα του τελευταίου block
  //   data = BF_Block_GetData(last_block);
  //   // Αποθήκευση δομης HP_block_info στο τελος του block
  //   bl_info_ptr = data + ht_info->records * sizeof(Record);
  //   memcpy(bl_info_ptr, &bl_info, sizeof(HP_block_info));
  //   // Αποθήκευση εγγραφής στο block
  //   memcpy(data, &record, sizeof(Record));
  //   BF_Block_SetDirty(last_block);
  //   CALL_OR_DIE(BF_UnpinBlock(last_block));
  //   BF_Block_Destroy(&last_block);
  // }

  // BF_Block_SetDirty(block);
  // CALL_BF(BF_UnpinBlock(block));
  // BF_Block_Destroy(&block);
  // return hp_info->last_block_id;
  return 0;
}

int HT_GetAllEntries(HT_info* ht_info, int value){
    return 0;
}