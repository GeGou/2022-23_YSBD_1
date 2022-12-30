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
  printf("pairs: %d\n", sht_info.pairs);
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
  SHT_block_info bl_info, *bl_info_ptr_1, *bl_info_ptr_2;
  Pair sht_pair;
  BF_Block *block;
  void *data;

  //φτιάχνω το ζεύγος των δεδομένων που θα μπει στο δευτερεύον ευρετήριο
  sht_pair.block_id=block_id;
  //sht_info->snumBuckets=3;

  //αντιγραφή του name στην μεταβλητη name του struct Pair
  int i;
  for(i=0 ; record.name[i]!='\0' ; i++){
    sht_pair.name[i]=record.name[i];
    printf("%c", sht_pair.name[i]);
  }
  sht_pair.name[i]='\0';
  printf("\n");

  //hash function για το όνομα
  int bucket = 0;
  for (int i = 0 ; sht_pair.name[i] != '\0' ; i++) {
    bucket = bucket + (int)(sht_pair.name[i]);
  }
  bucket = bucket % sht_info->snumBuckets;  //το bucket στο οποίο θα πάει το ζεύγος (record.name,block.id)
  printf("bucket=%d\n", bucket);
  //printf("HERE\n");
  int block_num = sht_info->sht_array[bucket];

  // Ελενχος για το αν εχει υπαρχει allocated block για τον κουβα, αν δεν υπάρχει το δημιουργώ
  if (block_num == 0) {
    BF_Block *new_block;
    BF_Block_Init(&new_block);
    //printf("HERE\n");
    CALL_OR_DIE(BF_AllocateBlock(sht_info->sfileDesc, new_block));
    printf("HERE\n");
    // Φτιαχνω το SHT_block_info
    int temp = 0;
    CALL_OR_DIE(BF_GetBlockCounter(sht_info->sfileDesc, &temp));
    sht_info->sht_array[bucket] = temp - 1;   // αποθήκευση αριθμού 1ου block με τις εγγραφές του κουβα
    bl_info.sblock_id = temp - 1;    // το νέο κατα σειρά δημιουργημένο block
    bl_info.block_pairs = 0;
    bl_info.soverflow_block_id = -1;   // -1 αν δεν εχει block υπερχείλισης
    // Αντιγράφουμε το struct στο τέλος του νέου block
    data = BF_Block_GetData(new_block);
    SHT_block_info *bl_info_ptr = data + sht_info->pairs * sizeof(Pair);
    memcpy(bl_info_ptr, &bl_info, sizeof(SHT_block_info));
    BF_Block_SetDirty(new_block);
    CALL_OR_DIE(BF_UnpinBlock(new_block));
    BF_Block_Destroy(&new_block);
  }
  int bl_id = 0;
  block_num = sht_info->sht_array[bucket];
  // Εύρεση του τελευταίου block με εγγραφές του κουβα προς τοποθέτηση του νέου ζεύγους
  do {
    BF_Block_Init(&block);
    CALL_OR_DIE(BF_GetBlock(sht_info->sfileDesc, block_num, block));  //δείκτη στο block του κουβά
    data = BF_Block_GetData(block);
    bl_info_ptr_1 = data + sht_info->pairs * sizeof(Pair);
    bl_id = bl_info_ptr_1->soverflow_block_id;
    // Βρέθηκε επόμενο block οπότε αποδεσμεύω το προηγούμενο
    if (bl_id != -1) {
      block_num = bl_id;
      CALL_OR_DIE(BF_UnpinBlock(block));
      BF_Block_Destroy(&block);
    }
  } while (bl_id != -1);
  // Βρηκαμε το block στο οποιο πρεπει να γινει η εγγραφή
  int records = bl_info_ptr_1->block_pairs;
  // Ελεγχος για κενό χωρο οπου θα μπει εγγραφή
  if (records < sht_info->pairs) {
    memcpy(data+records*sizeof(Pair), &record, sizeof(Pair));
    bl_info_ptr_1->block_pairs++;
  }
  // Δημιουργία block υπερχείλησης
  else {
    BF_Block *new_block;
    BF_Block_Init(&new_block);
    CALL_OR_DIE(BF_AllocateBlock(sht_info->sfileDesc, new_block));
    // Φτιαχνω το SHT_block_info
    int temp;
    CALL_OR_DIE(BF_GetBlockCounter(sht_info->sfileDesc, &temp));
    // Ενημέρωση των μεταδεδομένων του block-1 (προηγούμενου) που αφορα το τελευταιο κατα 
    // σειρά block που έχει δημιουργηθεί για τον συγκεκριμένο κουβα.
    bl_info_ptr_1->soverflow_block_id = temp - 1; 
    bl_info.sblock_id = temp - 1;
    bl_info.block_pairs = 1;    // θα παρει την νεα εγγραφή
    bl_info.soverflow_block_id = -1;   // δεν έχει block υπερχείλισης
    // Αντιγράφουμε το struct στο τέλος του νέου block
    data = BF_Block_GetData(new_block);
    bl_info_ptr_2 = data + sht_info->pairs * sizeof(Pair);
    memcpy(bl_info_ptr_2, &bl_info, sizeof(SHT_block_info));
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

int SHT_SecondaryGetAllEntries(HT_info* ht_info, SHT_info* sht_info, char* name) {
  SHT_block_info *bl_info_ptr;
  Pair sht_pair;
  BF_Block *block;
  int block_id, flag = 0, bl_counter = 0;
 
  //Βρισκω το bucket που περιεχει την εγγραφη με id ισο με value με χρήση της hash function
  int bucket = 0;
  for (int i = 0 ; name[i] != '\0' ; i++) {
    bucket = bucket + (int)(name[i]);
    printf("%c", name[i]);
  }
  bucket = bucket % sht_info->snumBuckets;  //το bucket στο οποίο θα πάει το ζεύγος (record.name,block.id)
  printf(" is in bucket=%d\n", bucket);

  // Βρισκω ποιο block αντιστοιχεί στο bucket
  int block_num = sht_info->sht_array[bucket];
  // Έλενχος για ύπαρξη block για το συγκεκριμενο bucket
  if (block_num != 0) {   
    do {
      bl_counter++;   // μετρητής των block που ελεγθηκαν και ζητείται να επιστραφεί
      BF_Block_Init(&block);
      CALL_OR_DIE(BF_GetBlock(sht_info->sfileDesc, block_num, block));  //δείκτη στο block του κουβά
      void *data = BF_Block_GetData(block);
      Pair* pair = data;
      bl_info_ptr = data + sht_info->pairs * sizeof(Pair);
      //Ευρεση ζητούμενης εγγραφής στο συγκεκριμενο block
      int pairs = bl_info_ptr->block_pairs;
      for (int y = 0 ; y < pairs ; y++) {
        printf("id: %d\n", pair[y].block_id);
        int temp = 0;
        temp = strcmp(pair[y].name, name);
        if (temp == 0) {
          //////////////////////////////////////////////
          // ΒΡΗΚΑΜΕ ΣΕ ΠΟΙΟ BLOCK ΒΡΙΣΚΕΤΑΙ Η ΕΓΓΡΑΦΗ ΚΑΙ ΘΕΛΟΥΜΕ ΝΑ ΠΑΜΕ ΣΤΟ ΠΡΩΤΕΥΟΝ
          // ΝΑ ΤΗΝ ΕΚΤΥΠΩΣΟΥΜΕ
          BF_Block *new_block;
          HT_block_info *bl_info_ptr;
          CALL_OR_DIE(BF_GetBlock(ht_info->fileDesc, pair[y].block_id, new_block));
          void *data = BF_Block_GetData(new_block);
          Record* rec = data;
          bl_info_ptr = data + ht_info->records * sizeof(Record);
          //Ευρεση ζητούμενης εγγραφής στο συγκεκριμενο block
          int records = bl_info_ptr->block_records;
          for (int y = 0 ; y < records ; y++) {
            temp = strcmp(pair[y].name, name);
            if (temp == 0) {
              printRecord(rec[y]);
              break;
            }
          }
        }
      CALL_OR_DIE(BF_UnpinBlock(block));
      }
        printf("HERE :%d\n", y);
        printf("strcmp : %d\n" , temp);
        // for(int i=0 ; name[i]!='\0' ; i++){
          // if (name[i] != sht_pair.name[i]) {
            // break;
          // }

      }
      CALL_OR_DIE(BF_UnpinBlock(block));
      // Ψαχνω στο επόμενο block του κουβα για την εγγραφη
      block_id = bl_info_ptr->soverflow_block_id;
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



