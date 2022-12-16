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
  ht_info.is_ht = 1;
  ht_info.ht_array = malloc(buckets*sizeof(int*));
  // Αρχικοποίηση πίνακα κατακερματισμού
  for (int i = 0 ; i < buckets ; i++) {
    ht_info.ht_array[i] = malloc(2*sizeof(int));
    ht_info.ht_array[i][0] = i+1;   // Αριθμός κουβά και block , κουβας 0 block 1
    ht_info.ht_array[i][1] = 0;     // 0 αν δεν εχει δημιουργηθει block για τον εκάστοτε κουβά
                                    // 1 αν εχει δημιουργηθει block για τον κουβα 
  }
  // Βρισκω το πλήθος των εγγραφων που χωρανε σε καθε block του αρχείου
  ht_info.records = (BF_BLOCK_SIZE - sizeof(HT_block_info)) / sizeof(Record);
  // Αποθήκευση struct ht_info στο 1ο block 
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
  HT_info* info = malloc(sizeof(HT_info));
  
  // Βρίσκω και επιστρέφω το περιεχόμενο του 1ου block (block 0)
  BF_Block_Init(&block);
  CALL_OR_DIE(BF_OpenFile(fileName, &fileDesc));
  CALL_OR_DIE(BF_GetBlock(fileDesc, 0, block)); 
  info = (HT_info *)BF_Block_GetData(block);
  if (info->is_ht == 0) {
    return NULL;
  }
  BF_Block_SetDirty(block);
  CALL_OR_DIE(BF_UnpinBlock(block)); 
  BF_Block_Destroy(&block);
  return info;
}


int HT_CloseFile(HT_info* HT_info) {
  CALL_OR_DIE(BF_CloseFile(HT_info->fileDesc));
  return 0;
}

int HT_InsertEntry(HT_info* ht_info, Record record){
  BF_Block *block_0;
  HT_block_info bl_info;
  // void *data;
  int flag = 0;   // flag για μη εγγραφή

  // Φέρνω στην ενδιάμεση μνήμη το 1ο block
  BF_Block_Init(&block_0);
  CALL_OR_DIE(BF_GetBlock(ht_info->fileDesc, 0, block_0));
  // printf("%d mod %ld = %d\n", record.id, ht_info->numBuckets, bucket);

  // Βρισκω το bucket στο οποιο θα "μπει" η εγγραφή record απο την Hash Function
  int bucket = record.id % ht_info->numBuckets;
  printf("%d\n", bucket);
  // Βρισκω ποιο block αντιστοιχεί στο bucket, που βρήκα απο πάνω, απο τον 
  // πινακα κατακερματισμού οπου κρατάει στοιχεία αντιστοίχισης bucket - block
  int block_num = ht_info->ht_array[bucket][0];
  // Ελενχος για το αν εχει υπαρχει allacated block για τον κουβα
  if (ht_info->ht_array[bucket][1] == 0) {
    ht_info->ht_array[bucket][1] = 1;
    BF_Block *new_block;
    BF_Block_Init(&new_block);
    CALL_OR_DIE(BF_AllocateBlock(ht_info->fileDesc, new_block));
    // Φτιαχνω το HT_block_info
    bl_info.block_id = block_num;
    bl_info.block_records = 0;
    bl_info.overflow_block = NULL; 
    // Αντιγράφουμε το struct στο τέλος του νέου block
    void *data = BF_Block_GetData(new_block);
    HT_block_info *bl_info_ptr = data + ht_info->records * sizeof(Record);
    memcpy(bl_info_ptr, &bl_info, sizeof(HT_block_info));
    BF_Block_SetDirty(new_block);
    CALL_OR_DIE(BF_UnpinBlock(new_block));
    BF_Block_Destroy(&new_block);
  }
  // Αποθήκευση εγγραφής στο block 
  HT_block_info *bl_info_ptr_1, *bl_info_ptr_2;
  BF_Block *block;
  int bl_id = 0;    // Αυτο θα επιστραφεί
  BF_Block_Init(&block);
  // Ελενχος για υπαρξη overflowed block
  CALL_OR_DIE(BF_GetBlock(ht_info->fileDesc, block_num, block));
  void *data = BF_Block_GetData(block);
  bl_info_ptr_1 = data + ht_info->records * sizeof(Record);
  // int bl_id = bl_info_ptr_1->block_id;    // Αυτο θα επιστραφεί
  while (bl_info_ptr_1->overflow_block != NULL) {
    BF_Block *next_block = bl_info_ptr_1->overflow_block;
    void *data = BF_Block_GetData(next_block);
    bl_info_ptr_1 = data + ht_info->records * sizeof(Record);
    bl_id = bl_info_ptr_1->block_id;
  }
  // Βρηκαμε το block στο οποιο πρεπει να γινει η εγγραφή
  int records = bl_info_ptr_1->block_records;
  // Ελενχος για κενό χωρο οπου θα μπει εγγραφή
  if (records < ht_info->records) {
    memcpy(data+records*sizeof(Record), &record, sizeof(Record));
  }
  // Δημιουργία block υπερχείλησης
  else {
    BF_Block *new_block;
    BF_Block_Init(&new_block);
    CALL_OR_DIE(BF_AllocateBlock(ht_info->fileDesc, new_block));
    // Φτιαχνω το HT_block_info
    bl_info.block_records = 1;    // θα παρει την νεα εγγραφή
    bl_info.overflow_block = NULL; 
    // Αντιγράφουμε το struct στο τέλος του νέου block
    void *data = BF_Block_GetData(new_block);
    bl_info_ptr_2 = data + ht_info->records * sizeof(Record);
    memcpy(bl_info_ptr_2, &bl_info, sizeof(HT_block_info));
    bl_info_ptr_1->overflow_block = new_block; 
    BF_Block_SetDirty(new_block);
    CALL_OR_DIE(BF_UnpinBlock(new_block));
    BF_Block_Destroy(&new_block);
  }
  // Τέλος επεξεργασίας 
  BF_Block_SetDirty(block);
  CALL_OR_DIE(BF_UnpinBlock(block));
  BF_Block_Destroy(&block);

  BF_Block_SetDirty(block_0);
  CALL_OR_DIE(BF_UnpinBlock(block_0));
  BF_Block_Destroy(&block_0);
  return bl_id;
}


int HT_GetAllEntries(HT_info* ht_info, int value){
  BF_Block *block_0;
  HT_block_info bl_info;
  // void *data;
  int flag = 0;   // flag για μη εγγραφή

  // Φέρνω στην ενδιάμεση μνήμη το 1ο block
  BF_Block_Init(&block_0);
  CALL_OR_DIE(BF_GetBlock(ht_info->fileDesc, 0, block_0));
  // printf("%d mod %ld = %d\n", record.id, ht_info->numBuckets, bucket);

  // Βρισκω το bucket που περιεχει την εγγραφη με id ισο με value
  BF_Block *block;
  HT_block_info *bl_info_ptr;
  int bucket = value % ht_info->numBuckets;
  int block_num = ht_info->ht_array[bucket][0]; 
  CALL_OR_DIE(BF_GetBlock(ht_info->fileDesc, block_num, block));  

  void *data = BF_Block_GetData(block);
  bl_info_ptr = data + ht_info->records * sizeof(Record);
  while (bl_info_ptr->overflow_block != NULL) {
    BF_Block *next_block = bl_info_ptr->overflow_block;
    void *data = BF_Block_GetData(next_block);
    bl_info_ptr = data + ht_info->records * sizeof(Record);
  }

  





  // int blocks = 0, flag = 0;
  // int i;   // Απο 1 διοτι δεν θα ελένξει το 1ο block με το βασικό struct
  // BF_Block *block;
  // HT_block_info;
  // BF_Block_Init(&block);
  // CALL_BF(BF_GetBlockCounter(ht_info->fileDesc, &blocks));

  // for(int i=1 ; i<ht_info->numBuckets ; i++){

  //     CALL_BF(BF_GetBlock(ht_info->fileDesc, i, block));

  //   // Δείκτης στα δεδομένα του block και στο HP_block_info
  //   void* data = BF_Block_GetData(block);
  //   Record* rec = data;
  //   HT_block_info *bl_info_ptr = data + ht_info->records * sizeof(Record);
  //   // Ευρεση εγγραφών στο συγκεκριμενο block
  //   int temp = bl_info_ptr->block_records;

      
  //   for (int y = 0 ; y < temp ; y++) {
  //     // printf("%d\n", y);
  //     if (rec[y].id == value) {
  //       printRecord(rec[y]);
  //       flag = 1;
  //       break;
  //     }
  //   }
  return 0;
}