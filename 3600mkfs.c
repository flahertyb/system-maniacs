/*
 * CS3600 Project 2: A User-Level File System
 *
 * This program is intended to format your disk file, and should be executed
 * BEFORE any attempt is made to mount your file system.  It will not, however
 * be called before every mount (you will call it manually when you format 
 * your disk file).
 */

#include <math.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "3600fs.h"
#include "disk.h"

void myformat(int size) {
  // Do not touch or move this function
  dcreate_connect();

  // first, create a zero-ed out array of memory  
  char *tmp = (char *) malloc(BLOCKSIZE);
  memset(tmp, 0, BLOCKSIZE);

  // now, write that to every block
  int i;
  for (i = 0; i<size; i++) 
    if (dwrite(i, tmp) < 0) 
      perror("Error while writing to disk");

  //Set all the free blocks to point to the next free block
  char* buf;
  for(i = 3; i < size - 1; i++){
    free_block b = {{0, i + 1}, ""};
    buf = (char*) &b;
    dwrite(i, buf);
    
  }
  free_block b;
  b.next_block.is_valid = 0;
  b.next_block.block = -1;
  buf = (char*) &b;
  dwrite(size - 1, buf);

  //Invalid block
  block_num invalid = {0, -1};   
  block_num block_1 = {1, 1};
  block_num block_2 = {1, 2};

  //Root
  //BLock 1
  //Directory INode
  DINode root_node;
  memset(&root_node, 0, sizeof(DINode));
  root_node.user = geteuid();//4
  root_node.group = getgid();//4
  root_node.mode = 0777;//4
  root_node.access_time = time(NULL);//8
  root_node.modify_time = time(NULL);//8
  root_node.create_time = time(NULL);//8
  root_node.size = 512;
  block_num root_DEL = block_2;
  for(i = 1; i<=118; i++)
    root_node.listof_DEL[i] = invalid;;
  root_node.listof_DEL[0] = root_DEL;
  root_node.single_in = invalid;
  root_node.double_in = invalid;

  //empty invalid DE
  DE de_invalid;
  de_invalid.is_valid = 0;
  de_invalid.is_file = 0;
  de_invalid.inode = invalid;
  strcpy(de_invalid.name, "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"); 

  //Block 2
  //Directory Entry List
  DEL del;
  memset(&del, 0, sizeof(DEL));
  del.next = invalid;
  for(i = 0; i<14; i++)
    del.DE_list[i] = de_invalid;

  del.DE_list[0].inode = block_1;
  del.DE_list[0].is_valid = 1;
  del.DE_list[0].is_file = 0;
  strcpy(del.DE_list[0].name, ".");

  //..
  //Dir Ent
  del.DE_list[1].inode = block_1;
  del.DE_list[1].is_valid = 1;
  del.DE_list[1].is_file = 0;
  strcpy(del.DE_list[1].name, "..");

  //Block 0
  VCB vcb;
  memset(&vcb, 0, sizeof(VCB));
  vcb.is_clean = 1;
  block_num free = {0, 3};  
  vcb.free = free;
  vcb.size = size;
  vcb.magic = 80085;
  strcpy(vcb.name, "Filephiles"); 
  vcb.root = block_1;

  //Printf Debugging
  printf("sizeof block_num:%d\n", sizeof(block_num));
  printf("sizeof VCB:%d\n", sizeof(VCB));
  printf("sizeof DE:%d\n", sizeof(DE));
  printf("sizeof DEL:%d\n", sizeof(DEL));
  printf("sizeof DINode:%d\n", sizeof(DINode));
  printf("sizeof FINode:%d\n", sizeof(FINode));
  printf("sizeof time_t:%d\n", sizeof(time_t));
  printf("sizeof uid_t:%d\n", sizeof(uid_t));
  printf("sizeof gid_t:%d\n", sizeof(gid_t));
  printf("sizeof mode_t:%d\n", sizeof(mode_t));
  printf("file name is: %s\n", del.DE_list[0].name);
  printf("file name is: %s\n", del.DE_list[1].name);
  printf("blocknumber: %d\n", root_node.listof_DEL[0].block);
  printf("free_block: %d\n", sizeof(free_block));
  printf("is_valid:%d\n", del.DE_list[2].is_valid);
 
  //Write VCB
  buf = (char*) &vcb;
  dwrite(0, buf);

  //Write root DINode
  buf = (char*) &root_node;
  dwrite(1, buf);
  
  //Write root del
  buf = (char*) &del;
  dwrite(2, buf);

  // Do not touch or move this function
  dunconnect();
}

int main(int argc, char** argv) {
  // Do not touch this function
  if (argc != 2) {
    printf("Invalid number of arguments \n");
    printf("usage: %s diskSizeInBlockSize\n", argv[0]);
    return 1;
  }

  unsigned long size = atoi(argv[1]);
  printf("Formatting the disk with size %lu \n", size);
  myformat(size);
}
