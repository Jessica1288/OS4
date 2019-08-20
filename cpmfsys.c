#include <stdint.h> 
#include <stdio.h> 
#include <stdlib.h>
#include <ctype.h>
#include "cpmfsys.h"
#include "diskSimulator.h"

static bool freemap[NUM_BLOCKS];   //Freelist Global var

DirStructType *mkDirStruct(int index,uint8_t *e){
  //create memory space for dir; a pointer
  DirStructType *dir = (DirStructType *)malloc(sizeof(DirStructType));
  memset(dir, 0, sizeof(DirStructType));

  uint8_t *dir_addr = (e + index * EXTENT_SIZE * sizeof(uint8_t));

  dir -> status = dir_addr[0]; 
  for(int i = 0 ; i < 8 ; i++){
    if(dir_addr[i+1] == ' ') break;
    dir -> name[i] = dir_addr[i+1];
  }
  for(int i = 0; i < 3; i++){
    if(dir_addr[i+9] == ' ') break;
    dir -> extension[i] = dir_addr[i+9];
  }
  // memcpy(dir -> name, dir_addr+1, 8);
  // memcpy(dir -> extension, dir_addr+9, 3);
  dir -> XL = dir_addr[12];
  dir -> BC = dir_addr[13];
  dir -> XH = dir_addr[14];
  dir -> RC = dir_addr[15];
  //obtain the dir -> block array from in-memmory block0;
  memcpy(dir -> blocks, dir_addr+16, BLOCKS_PER_EXTENT);

  return dir;
}

void writeDirStruct(DirStructType *d, uint8_t index, uint8_t *e){
  uint8_t *dir_addr = (e + index * EXTENT_SIZE);
  dir_addr[0] = d -> status;
  // memcpy(dir_addr + 1 , d -> name, 8);
  // memcpy(dir_addr + 9, d -> extension, 3);
  for(int i = 0; i < 8; i++){
    if(d -> name[i] == '\0'){
      dir_addr[i+1] = ' ';
      break;
    }
    dir_addr[i+1] = d -> name[i];
  }
  for(int i = 0; i < 3; i++){
    if(d -> extension[i] == '\0'){
      dir_addr[i+9] = ' '; 
      break;
    }
    dir_addr[i+9] = d -> extension[i];
  }

  dir_addr[12] = d -> XL;
  dir_addr[13] = d -> BC;
  dir_addr[14] = d -> XH;
  dir_addr[15] = d -> RC;
  memcpy(dir_addr + 16, d-> blocks, BLOCKS_PER_EXTENT); 
}


void makeFreeList(){
  //initially set all the blocks as free; -> unused
  for(int i = 1 ; i < NUM_BLOCKS; i++){ 
    freemap[i] = true;
  }
  freemap[0] = false; //set directroy block as 'used' freemap[0] = false;

  uint8_t block0[BLOCK_SIZE];
  //read block0 from disk into the main memory
  blockRead(block0, (uint8_t) 0);
  DirStructType *dir;
  int NUM_OF_EXTENT = BLOCK_SIZE / EXTENT_SIZE;

  for(int i = 0 ; i< NUM_OF_EXTENT; i++){
    dir = mkDirStruct(i, block0);
    if(dir -> status != 0xe5){
      for(int j = 0 ; j < BLOCKS_PER_EXTENT; j++){
        if (dir -> blocks[j] != 0){
          freemap[(int) dir -> blocks[j]] = false;
        }
      }
    }
  }
}

void printFreeList(){
  printf("FREE BLOCK LIST: (* means in-use)\n");
  for(int i = 0; i < NUM_BLOCKS; i++){
    if((i % 16) == 0)
      printf("%3x: ", i);
    if(freemap[i] == false)
      printf("* ");
    if(freemap[i] == true)
      printf(". ");
    if(i %16 == 15)
      printf("\n");
  }
}

// internal function, returns -1 for illegal name or name not found
// otherwise returns extent nunber 0-31
int findExtentWithName(char *name, uint8_t *block0){
  char *fileName, *extName;
  char buffer[15];
  strcpy(buffer, name);
  fileName = strtok(buffer, ".");
  extName = strtok(NULL,"\0");
  //split up the name into file name and ext_name;

  //check legal file name 
  if(!checkLegalName(name))
    return -1;
  DirStructType *dir;


  int NUM_OF_EXTENT = BLOCK_SIZE / EXTENT_SIZE;
  for(int index = 0 ; index < NUM_OF_EXTENT; index++){
    dir = mkDirStruct(index, block0);
    // if(dir -> status == 0xe5){
    //   free(dir);
    // }
    if(dir -> status != 0xe5){
      if(strcmp(fileName, dir -> name) == 0){
          if(extName != NULL && strcmp(extName, dir -> extension) == 0){
            return index;
          }
          else{
            if(strcmp("\0", dir-> extension) == 0){
              return index;
            }
          }
      }
      free(dir);
    }
  }
  return -1;
}


bool checkLegalName(char *name){
  int length = strlen(name);
  if(length == 0 || length > 12)
    return false;
  char *dotpoint = strchr(name, '.');
  if(dotpoint == NULL)
    dotpoint = name + length;
  if(dotpoint - name > 8)
    return false;
  char namebuf[9] = {0}, extbuf[4] = {0};
  memcpy(namebuf, name, dotpoint - name);
  memcpy(extbuf , dotpoint + 1, name + length - dotpoint);
  for(int i = 0; i < strlen(namebuf); i++){
    if(!isalpha(namebuf[i]) && !isdigit(namebuf[i]))
      return false;
  }
  for(int i = 0; i < strlen(extbuf); i++){
    if(!isalpha(extbuf[i]) && !isdigit(extbuf[i]) && isspace(extbuf[i]))
      return false;
  }
  return true;
}

void cpmDir(){
  // read block 0 into cpm
  //cpm_block0 <- blockRead(block0);
  uint8_t block0[BLOCK_SIZE];
  blockRead(block0, (uint8_t)0);
  DirStructType *dir;
  int file_length = 0;
  fprintf(stdout, "DIRECTORY LISTING\n");
  //all the extent referred by index in cpm_block0
  int NUM_OF_EXTENT = BLOCK_SIZE / EXTENT_SIZE;
  for(int index =0; index < NUM_OF_EXTENT; index++){
    dir = mkDirStruct(index, block0);
    if(dir -> status != 0xe5){ // != 0xe5 used
      // compute file length
      int block_number = 0;
      for(int b_index = 0; b_index < 16; b_index++){
        if(dir -> blocks[b_index] != 0)
          block_number++;
      }
      // RC = cpm_dir -> RC, BC = cpm_dir -> BC
      file_length = (block_number-1) * 1024 + dir -> RC * 128 + dir -> BC;

      //printf file name from cpm; printf file_length;
      fprintf(stdout, "%s.%s %d \n", dir -> name, dir -> extension, file_length);
      free(dir);
    }//if cmp_dir -> status is used.
  }
}


int cpmRename(char *oldName, char * newName){
  uint8_t block0[BLOCK_SIZE];
  char buf[15];
  strcpy(buf, newName);
  blockRead(block0, (uint8_t)0);

  if(!checkLegalName(oldName) || !checkLegalName(newName))
    return -2;

  int old_index = findExtentWithName(oldName, block0);
  if(old_index < 0)
    return old_index;

  int new_index = findExtentWithName(newName, block0);
  if(new_index > 0)
    return new_index;
  
  DirStructType *dir = mkDirStruct(old_index, block0);
  
  char *fileName = strtok(buf,".");
  char *extName = strtok(NULL, "\0");
  strcpy(dir -> name, fileName);
  if(extName != NULL)
    strcpy(dir -> extension, extName);
  else
    memset(dir -> extension, 0, sizeof(char)*4);

  writeDirStruct(dir, old_index, block0);
  blockWrite(block0, 0);
  free(dir);
  return 0;
}

int cpmDelete(char * name){
  uint8_t block0[BLOCK_SIZE];
  blockRead(block0, (uint8_t)0);
  int index = findExtentWithName(name, block0);
  DirStructType *dir = mkDirStruct(index, block0);
  if(index < 0 )
    return index;
  for(int i = 0 ; i < BLOCKS_PER_EXTENT ; i++){
    if(dir -> blocks[i] != 0){
      freemap[dir -> blocks[i]] = true;
    }
  }
  memset(dir, 0, sizeof(DirStructType));
  dir -> status = 0xe5;
  writeDirStruct(dir, index, block0);
  blockWrite(block0, 0);
  free(dir);
  return 0;
}

/*
// following functions need not be implemented for Lab 2 
int  cpmCopy(char *oldName, char *newName){

}

int  cpmOpen( char *fileName, char mode){

}

int cpmClose(int filePointer){
  // non-zero return indicates filePointer did not point to open file 
}

int cpmRead(int pointer, uint8_t *buffer, int size){
  // returns number of bytes read, 0 = error 
}

int cpmWrite(int pointer, uint8_t *buffer, int size){
  // returns number of bytes written, 0 = error 


}
*/
