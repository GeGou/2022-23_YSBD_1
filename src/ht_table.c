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

int HT_CreateFile(char *fileName, int buckets) {
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
  ht_info.ht_array = malloc(buckets*sizeof(int));
  // Αρχικοποίηση πίνακα κατακερματισμού
  for (int i = 0 ; i < buckets ; i++) {
    ht_info.ht_array[i] = 0;   // Αντιστοίχιση κουβά και block , π.χ κουβας 0 block 1
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

int HT_InsertEntry(HT_info* ht_info, Record record) {
  BF_Block *block_0;
  HT_block_info bl_info;
  // void *data;

  // Βρισκω το bucket στο οποιο θα "μπει" η εγγραφή record απο την Hash Function
  int bucket = record.id % ht_info->numBuckets;
  printf("%d\n", bucket);
  // Βρισκω ποιο block αντιστοιχεί στο bucket, που βρήκα απο πάνω, απο τον 
  // πινακα κατακερματισμού οπου κρατάει στοιχεία αντιστοίχισης bucket - block
  int block_num = ht_info->ht_array[bucket];
  // Ελενχος για το αν εχει υπαρχει allacated block για τον κουβα, αν δεν υπάρχει το δημιουργώ
  if (block_num == 0) {
    BF_Block *new_block;
    BF_Block_Init(&new_block);
    CALL_OR_DIE(BF_AllocateBlock(ht_info->fileDesc, new_block));
    // Φτιαχνω το HT_block_info
    int temp;
    CALL_OR_DIE(BF_GetBlockCounter(ht_info->fileDesc, &temp));
    ht_info->ht_array[bucket] = temp - 1;   // αποθήκευση αριθμού 1ου block με τις εγγραφές του κουβα
    bl_info.block_id = temp - 1;    // το νέο κατα σειρά δημιουργημένο block
    bl_info.block_records = 0;
    bl_info.overflow_block_id = -1;   // -1 αν δεν εχει block υπερχείλισης
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
  void *data;

  // Εύρεση του τελευταίου block με εγγραφές του κουβα προς τοποθέτηση της νέας εγγραφης
  do {
    BF_Block_Init(&block);
    CALL_OR_DIE(BF_GetBlock(ht_info->fileDesc, block_num, block));  //δείκτη στο block του κουβά
    data = BF_Block_GetData(block);
    bl_info_ptr_1 = data + ht_info->records * sizeof(Record);
    bl_id = bl_info_ptr_1->overflow_block_id;
    // Βρέθηκε επόμενο block οπότε αποδεσμεύω το προηγούμενο
    if (bl_id != -1) {
      block_num = bl_id;
      CALL_OR_DIE(BF_UnpinBlock(block));
      BF_Block_Destroy(block);
    }
  } while (bl_id != -1);
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
    int temp;
    CALL_OR_DIE(BF_GetBlockCounter(ht_info->fileDesc, &temp));
    bl_info.block_id = temp - 1;
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
  return block_num;
}


int HT_GetAllEntries(HT_info* ht_info, int value) {
  // HT_block_info bl_info;

  // // Βρισκω το bucket που περιεχει την εγγραφη με id ισο με value με χρήση της hash function
  // BF_Block *block, *next_block;
  // HT_block_info *bl_info_ptr;
  // int bucket = value % ht_info->numBuckets;
  // int block_num = ht_info->ht_array[bucket][0]; 
  // // Έλενχος για το αν εχει εγγραφές το συγκεκριμένο bucket
  // if (ht_info->ht_array[bucket][1] == 0) {
  //   return -1;
  // }
  // // Δεικτη στο 1ο block του κουβα
  // BF_Block_Init(&block);
  // CALL_OR_DIE(BF_GetBlock(ht_info->fileDesc, block_num, block));  
  // void *data = BF_Block_GetData(block);
  // int flag = 0;   // flag για εύρεση εγγραφή(1) ή μη εύρεση(0)
  // int flag_0 = 0;   // Αν το 1ο block του κουβα εχει block υπερχείλισης , 0 αν δεν έχει, 1 αν εχει
  // int next_bl_id;
  // // Εξωτερική επανάληψη για κάθε block του κουβά
  // do {
  //   if (flag_0 == 1) {
  //     next_block = bl_info_ptr->overflow_block;
  //     CALL_OR_DIE(BF_UnpinBlock(block));
  //     void *data = BF_Block_GetData(next_block);
  //   }
  //   flag_0 = 1;
  //   Record* rec = data;
  //   bl_info_ptr = data + ht_info->records * sizeof(Record);
  //   // Ευρεση ζητούμενης εγγραφής στο συγκεκριμενο block
  //   int records = bl_info_ptr->block_records;
  //   for (int y = 0 ; y < records ; y++) {
  //     // printf("%d\n", y);
  //     if (rec[y].id == value) {
  //       printRecord(rec[y]);
  //       flag = 1;
  //       break;
  //     }
  //   }
  //   // CALL_OR_DIE(BF_UnpinBlock(block));
  //   if (flag == 1) {break;}
  // } while (bl_info_ptr->overflow_block != NULL);

  // BF_Block_Destroy(&block);
  // return bl_info_ptr->block_id;




return 0;




















  // HT_block_info bl_info;

  // // Βρισκω το bucket που περιεχει την εγγραφη με id ισο με value με χρήση της hash function
  // BF_Block *block, *next_block;
  // HT_block_info *bl_info_ptr;
  // int bucket = value % ht_info->numBuckets;
  // int block_num = ht_info->ht_array[bucket][0]; 
  // // Έλενχος για το αν εχει εγγραφές το συγκεκριμένο bucket
  // if (ht_info->ht_array[bucket][1] == 0) {
  //   return -1;
  // }
  // // Δεικτη στο 1ο block του κουβα
  // BF_Block_Init(&block);
  // CALL_OR_DIE(BF_GetBlock(ht_info->fileDesc, block_num, block));  
  // void *data = BF_Block_GetData(block);
  // int flag = 0;   // flag για εύρεση εγγραφή(1) ή μη εύρεση(0)
  // int flag_0 = 0;   // Αν το 1ο block του κουβα εχει block υπερχείλισης , 0 αν δεν έχει, 1 αν εχει
  // // Εξωτερική επανάληψη για κάθε block του κουβά
  // do {
  //   if (flag_0 == 1) {
  //     next_block = bl_info_ptr->overflow_block;
  //     CALL_OR_DIE(BF_UnpinBlock(block));
  //     void *data = BF_Block_GetData(next_block);
  //   }
  //   flag_0 = 1;
  //   Record* rec = data;
  //   bl_info_ptr = data + ht_info->records * sizeof(Record);
  //   // Ευρεση ζητούμενης εγγραφής στο συγκεκριμενο block
  //   int records = bl_info_ptr->block_records;
  //   for (int y = 0 ; y < records ; y++) {
  //     // printf("%d\n", y);
  //     if (rec[y].id == value) {
  //       printRecord(rec[y]);
  //       flag = 1;
  //       break;
  //     }
  //   }
  //   // CALL_OR_DIE(BF_UnpinBlock(block));
  //   if (flag == 1) {break;}
  // } while (bl_info_ptr->overflow_block != NULL);

  // BF_Block_Destroy(&block);
  // return bl_info_ptr->block_id;
}