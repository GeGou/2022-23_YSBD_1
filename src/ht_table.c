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
    ht_info.ht_array[i] = 0;   // Αρχικά για κανεναν κουβα δεν εχει δημιουργηθεί block
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
  char* data = BF_Block_GetData(block); 
  memcpy(data, &fileDesc, sizeof(int));
  memcpy(info, data, sizeof(HT_info));
  if (info->is_ht == 0) {
    return NULL;
  }
  CALL_OR_DIE(BF_UnpinBlock(block)); 
  BF_Block_Destroy(&block);
  return info;
}


int HT_CloseFile(HT_info* HT_info) {
  CALL_OR_DIE(BF_CloseFile(HT_info->fileDesc));
  free(HT_info->ht_array);
  free(HT_info);
  return 0;
}

int HT_InsertEntry(HT_info* ht_info, Record record) {
  HT_block_info bl_info, *bl_info_ptr_1, *bl_info_ptr_2;
  BF_Block *block;
  void *data;

  // Βρισκω το bucket στο οποιο θα "μπει" η εγγραφή record απο την Hash Function
  int bucket = record.id % ht_info->numBuckets;
  // Βρισκω ποιο block αντιστοιχεί στο bucket, που βρήκα απο πάνω, απο τον 
  // πινακα κατακερματισμού οπου κρατάει στοιχεία αντιστοίχισης bucket - block
  int block_num = ht_info->ht_array[bucket];
  // Ελενχος για το αν εχει υπαρχει allacated block για τον κουβα, αν δεν υπάρχει το δημιουργώ
  if (block_num == 0) {
    BF_Block *new_block;
    BF_Block_Init(&new_block);
    CALL_OR_DIE(BF_AllocateBlock(ht_info->fileDesc, new_block));
    // Φτιαχνω το HT_block_info
    int temp = 0;
    CALL_OR_DIE(BF_GetBlockCounter(ht_info->fileDesc, &temp));
    ht_info->ht_array[bucket] = temp - 1;   // αποθήκευση αριθμού 1ου block με τις εγγραφές του κουβα
    bl_info.block_id = temp - 1;    // το νέο κατα σειρά δημιουργημένο block
    bl_info.block_records = 0;
    bl_info.overflow_block_id = -1;   // -1 αν δεν εχει block υπερχείλισης
    // Αντιγράφουμε το struct στο τέλος του νέου block
    data = BF_Block_GetData(new_block);
    HT_block_info *bl_info_ptr = data + ht_info->records * sizeof(Record);
    memcpy(bl_info_ptr, &bl_info, sizeof(HT_block_info));
    BF_Block_SetDirty(new_block);
    CALL_OR_DIE(BF_UnpinBlock(new_block));
    BF_Block_Destroy(&new_block);
  }
  int bl_id = 0;
  block_num = ht_info->ht_array[bucket];
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
      BF_Block_Destroy(&block);
    }
  } while (bl_id != -1);
  // Βρηκαμε το block στο οποιο πρεπει να γινει η εγγραφή
  int records = bl_info_ptr_1->block_records;
  // Ελενχος για κενό χωρο οπου θα μπει εγγραφή
  if (records < ht_info->records) {
    memcpy(data+records*sizeof(Record), &record, sizeof(Record));
    bl_info_ptr_1->block_records++;
  }
  // Δημιουργία block υπερχείλησης
  else {
    BF_Block *new_block;
    BF_Block_Init(&new_block);
    CALL_OR_DIE(BF_AllocateBlock(ht_info->fileDesc, new_block));
    // Φτιαχνω το HT_block_info
    int temp;
    CALL_OR_DIE(BF_GetBlockCounter(ht_info->fileDesc, &temp));
    // Ενημέρωση των μεταδεδομένων του block-1 (προηγούμενου) που αφορα το τελευταιο κατα 
    // σειρά block που έχει δημιουργηθεί για τον συγκεκριμένο κουβα.
    bl_info_ptr_1->overflow_block_id = temp - 1; 
    bl_info.block_id = temp - 1;
    bl_info.block_records = 1;    // θα παρει την νεα εγγραφή
    bl_info.overflow_block_id = -1;   // δεν έχει block υπερχείλισης
    // Αντιγράφουμε το struct στο τέλος του νέου block
    data = BF_Block_GetData(new_block);
    bl_info_ptr_2 = data + ht_info->records * sizeof(Record);
    memcpy(bl_info_ptr_2, &bl_info, sizeof(HT_block_info));
    BF_Block_SetDirty(new_block);
    CALL_OR_DIE(BF_UnpinBlock(new_block));
    BF_Block_Destroy(&new_block);
  }
  // Τέλος επεξεργασίας 
  BF_Block_SetDirty(block);
  CALL_OR_DIE(BF_UnpinBlock(block));
  BF_Block_Destroy(&block);
  return block_num;
}


int HT_GetAllEntries(HT_info* ht_info, int value) {
  HT_block_info *bl_info_ptr;
  BF_Block *block;
  int block_id, flag = 0, bl_counter = 0;

  //Βρισκω το bucket που περιεχει την εγγραφη με id ισο με value με χρήση της hash function
  int bucket = value % ht_info->numBuckets;
  // Βρισκω ποιο block αντιστοιχεί στο bucket
  int block_num = ht_info->ht_array[bucket];
  // Έλενχος για ύπαρξη block για το συγκεκριμενο bucket
  if (block_num != 0) {   
    do {
      bl_counter++;   // μετρητής των block που ελεγθηκαν και ζητείται να επιστραφεί
      BF_Block_Init(&block);
      CALL_OR_DIE(BF_GetBlock(ht_info->fileDesc, block_num, block));  //δείκτη στο block του κουβά
      void *data = BF_Block_GetData(block);
      Record* rec = data;
      bl_info_ptr = data + ht_info->records * sizeof(Record);
      //Ευρεση ζητούμενης εγγραφής στο συγκεκριμενο block
      int records = bl_info_ptr->block_records;
      for (int y = 0 ; y < records ; y++) {
        if (rec[y].id == value) {
          printRecord(rec[y]);
          flag = 1;   // σε περιπτωση που βρηκε την εγγραφη ,τερματίζει την αναζήτηση
          break;
        }
      }
      CALL_OR_DIE(BF_UnpinBlock(block));
      if (flag == 1) {break;}
      // Ψαχνω στο επόμενο block του κουβα για την εγγραφη
      block_id = bl_info_ptr->overflow_block_id;
      if (block_id != -1) {
        block_num = block_id;
        CALL_OR_DIE(BF_UnpinBlock(block));
        BF_Block_Destroy(&block);
      }
    } while (block_id != -1);
  }
  BF_Block_Destroy(&block); 
  if (flag == 0) {
    return -1;
  }
  return bl_counter;
}

int HashStatistics(char* filename) {
  int fileDesc;
  BF_Block *block, *block_0;
  HT_block_info *bl_info_ptr;
  HT_info *ht_info;

  // Παίρνω δείκτη στα δεδομένα του 1ο block (block 0)
  BF_Block_Init(&block);
  CALL_OR_DIE(BF_OpenFile(filename, &fileDesc));
  CALL_OR_DIE(BF_GetBlock(fileDesc, 0, block)); 
  void* data = BF_Block_GetData(block);
  ht_info = data;
  // 1o ζητούμενο
  int max_file_blocks;
  CALL_OR_DIE(BF_GetBlockCounter(ht_info->fileDesc, &max_file_blocks));
  printf("File blocks : %d\n", max_file_blocks);
  
  int overflowed_buckets = 0, min_rec = 0, max_rec = 0, file_rec_sum = 0, flag = 0;
  float avg_rec = 0;
  for (int i = 0 ; i < ht_info->numBuckets ; i++) {
    // Βρισκω για κάθε bucket ποιο ειναι το 1ο block που αρχιζει να βαζει εγγραφές 
    int block_num = ht_info->ht_array[i];
    int overflow_bl_id, overflow_blocks = 0, rec_sum = 0;
    int flag_0 = 0, bl_per_bucket = 0;
    // Έλενχος για το αν εχει δημιουργηθει block για τον συγκεκριμένο κουβά
    if (block_num != 0) {
      do {
        bl_per_bucket++;
        overflow_blocks++;    // αυξάνεται μονο αν υπάρχει block υπερχείλισης
        BF_Block_Init(&block_0);
        CALL_OR_DIE(BF_GetBlock(ht_info->fileDesc, block_num, block_0));  //δείκτη στο block του κουβά
        void* data_0 = BF_Block_GetData(block_0);
        bl_info_ptr = data_0 + ht_info->records * sizeof(Record);
        // Έλενχος για το αν υπάρχει block υπερχείλισης
        overflow_bl_id = bl_info_ptr->overflow_block_id;
        // Άθροιση των εγγραφών καθε κουβά για το 2ο ζητούμενο
        rec_sum += bl_info_ptr->block_records;
        if (overflow_bl_id != -1) {
          if (flag_0 == 0) {
            overflowed_buckets++;   // αυξάνεται όταν ο κουβάς εχει block υπερχείλισης
            flag_0 = 1;
          }
          block_num = overflow_bl_id;
          CALL_OR_DIE(BF_UnpinBlock(block_0));
          BF_Block_Destroy(&block_0);
        }
      } while (overflow_bl_id != -1);
      CALL_OR_DIE(BF_UnpinBlock(block_0)); 
      BF_Block_Destroy(&block_0);
      
      // Υλοποίηση 2ου ζητούμενoυ, μπαινει μια φορα για τον 1ο κουβα ωστε να αρχικοποιηθού τα min και max
      if (flag == 0) {
        min_rec = rec_sum;
        max_rec = rec_sum;
        flag = 1;
      }
      // Σε αυτο θα μπει για όλους τους υπόλοιπους κουβαδες
      if (rec_sum < min_rec) {
        min_rec = rec_sum;
      }
      if (rec_sum > max_rec) {
        max_rec = rec_sum;
      }
      file_rec_sum += rec_sum;    
    }
    // 4o ζητούμενο
    printf("Bucket: %d has %d overflow blocks.\n", i, bl_per_bucket - 1);
  }

  // 2o ζητούμενο
  avg_rec = (float)file_rec_sum / (float)ht_info->numBuckets;
  printf("Min records: %d  -> Avg records: %f -> Max records: %d\n", min_rec, avg_rec, max_rec);
  // 3o ζητούμενο , δεν υπολογίζουμε το block 0
  int avg_bucket_blocks = (max_file_blocks - 1)/ ht_info->numBuckets;
  printf("Average block per bucket: %d\n", avg_bucket_blocks);
  // 4o ζητούμενο
  printf("Amount of buckets with overflow blocks: %d\n", overflowed_buckets);

  CALL_OR_DIE(BF_UnpinBlock(block)); 
  BF_Block_Destroy(&block);
  return 0;
}