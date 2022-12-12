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
  HP_info hp_info;
  BF_Block* block;
  void* data;

  BF_PrintError(BF_CreateFile(fileName));
  BF_PrintError(BF_OpenFile(fileName, &hp_info.fileDesc));
  // Αρχικοποιώ/δεσμευσω block στην μνήμη
  BF_Block_Init(&block);
  BF_PrintError(BF_AllocateBlock(hp_info.fileDesc, block));
  // Δείκτης που δειχνει στην αρχή των δεδομένων του block
  data = BF_Block_GetData(block);
  // Αρχικοποίηση μεταβλητών
  hp_info.block = block;
  hp_info.last_block_id = 0;
  hp_info.is_heap = 1;
  // Βρισκω το πλήθος των εγγραφων που χωρανε σε καθε block του αρχείου
  hp_info.records = (BF_BLOCK_SIZE - sizeof(HP_block_info)) / sizeof(Record);
  // Αποθήκευση struct hp_info στο 1ο block 
  memcpy(data, &hp_info, sizeof(HP_info));
  // Dirty και Unpin για να αποθηκευτεί στον δίσκο
  BF_Block_SetDirty(block);
  BF_PrintError(BF_UnpinBlock(block));
  // Τέλος δημιουργίας του αρχείου
  BF_Block_Destroy(&block);
  BF_PrintError(BF_CloseFile(hp_info.fileDesc));
  return 0;
}

HP_info* HP_OpenFile(char *fileName){
  int f_d;
  BF_Block* block;
  
  // Βρίσκω και επιστρέφω το περιεχόμενο του 1ου block (block 0)
  BF_Block_Init(&block);
  BF_PrintError(BF_OpenFile(fileName, &f_d));
  BF_PrintError(BF_GetBlock(f_d, 0, block));
  return (HP_info *)BF_Block_GetData(block);
}

int HP_CloseFile(HP_info* hp_info){
  // BF_Block_SetDirty(hp_info->block);
  // BF_PrintError(BF_UnpinBlock(hp_info->block));
  // BF_Block_Destroy(&hp_info->block);
  // BF_PrintError(BF_CloseFile(hp_info->fileDesc));
  return 0;
}


int HP_InsertEntry(HP_info* hp_info, Record record){
  BF_Block *block, *last_block;
  HP_block_info bl_info;
  HP_block_info *bl_info_ptr;
  void *data;
  int flag = 0;   // flag για μη εγγραφή

  // Φέρνω στην ενδιάμεση μνήμη το 1ο block
  BF_Block_Init(&block);
  BF_PrintError(BF_GetBlock(hp_info->fileDesc, 0, block));

  // Ελενχος για το αν υπαρχει block για εγγραφές, δηλαδή παραπάνω απο ένα block
  if (hp_info->last_block_id != 0) {
    // Φέρνω το τελευταιο block στην ενδιάμεση μνήμη
    BF_Block_Init(&last_block);
    BF_PrintError(BF_GetBlock(hp_info->fileDesc, hp_info->last_block_id, last_block));
    // Δείκτης στα δεδομένα του τελευταίου block
    data = BF_Block_GetData(last_block);
    // Δείκτης στο τέλος του block όπου βρισκεται το struct HP_block_info
    bl_info_ptr = data + hp_info->records * sizeof(Record);
    // Εύρεση πλήθους εγγραφων στο block
    int records = bl_info_ptr->block_records;
    // Ελενχος για κενό χωρο οπου θα μπει εγγραφή
    if (records < hp_info->records) {
      memcpy(data+records*sizeof(Record), &record, sizeof(Record));
      bl_info_ptr->block_records++;
      BF_Block_SetDirty(last_block);
      flag = 1;
    }
    BF_PrintError(BF_UnpinBlock(last_block));
    BF_Block_Destroy(&last_block);
  }
  // Δέσμευση νέου block
  if (flag == 0) {
    BF_Block_Init(&last_block);
    BF_PrintError(BF_AllocateBlock(hp_info->fileDesc, last_block));
    // Ενημερωση σχετικα με το πλεον τελευταιό block
    hp_info->last_block_id++;
    // Αρχικοποίηση μεταβλητων του struct HP_block_info
    bl_info.block_records = 1;
    bl_info.next_block = NULL;  // ΘΕΛΕΙ ΚΟΙΤΑΓΜΑ!
    // Δείκτης στα δεδομενα του τελευταίου block
    data = BF_Block_GetData(last_block);
    // Αποθήκευση δομης HP_block_info στο τελος του block
    bl_info_ptr = data + hp_info->records * sizeof(Record);
    memcpy(bl_info_ptr, &bl_info, sizeof(HP_block_info));
    // Αποθήκευση εγγραφής στο block
    memcpy(data, &record, sizeof(Record));
    BF_Block_SetDirty(last_block);
    BF_PrintError(BF_UnpinBlock(last_block));
    BF_Block_Destroy(&last_block);
  }
  // ΙΣΩΣ ΝΑ ΜΕΝΕΙ ΑΚΟΜΑ ΚΑΠΟΙΟ PINNED BLOCK ΓΙΑΤΙ 
  // ΚΑΝΕΙ UNPIN ΜΟΝΟ ΤΟ ΤΕΛΕΥΤΑΙΟ BLOCK
  BF_Block_SetDirty(block);
  BF_PrintError(BF_UnpinBlock(block));
  BF_Block_Destroy(&block);
  return hp_info->last_block_id;
}

int HP_GetAllEntries(HP_info* hp_info, int value){
  int blocks = 0, flag = 0;
  int i = 1;   // Απο 1 διοτι δεν θα ελένξει το 1ο block με το βασικό struct
  BF_Block *block;

  BF_Block_Init(&block);
  BF_PrintError(BF_GetBlockCounter(hp_info->fileDesc, &blocks));
  printf("\n>%d\n", hp_info->records);
  //ΜΕΤΑ ΤΟ ΜΠΛΟΚ 101 Ή 101 ΣΚΑΕΙ , ΓΕΜΙΖΕΙ Ο BUFFER
  // for (i = 1; i < blocks; i++) {
  for (i = 1; i < 100; i++) {
    // printf("\n--->%d\n", i);
    BF_PrintError(BF_GetBlock(hp_info->fileDesc, i, block));
    // Δείκτης στα δεδομένα του block και στο HP_block_info
    void* data = BF_Block_GetData(block);
    Record* rec = data;
    HP_block_info *bl_info_ptr = data + hp_info->records * sizeof(Record);
    // Ευρεση εγγραφών στο συγκεκριμενο block
    int temp = bl_info_ptr->block_records;
    // ΔΙΑΒΑΖΕΙ ΠΑΡΑΠΑΝΩ ΑΠΟ 6 ΕΓΓΡΑΦΕΣ ΠΟΥ ΧΩΡΑΕΙ ΤΟ BLOCK ΧΩΡΙΣ ΝΑ ΣΚΑΕΙ
    for (int y = 0 ; y < temp ; y++) {
      if (rec[y].id == value) {
        printRecord(rec[y]);
        flag = 1;
      }
    }
    BF_PrintError(BF_UnpinBlock(block));
    if (flag == 1) {break;}
  }
  BF_Block_Destroy(&block); 
  printf("--->%d\n", i);
  return i;
}