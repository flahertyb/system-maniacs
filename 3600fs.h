/*
 * CS3600 Project 2: A User-Level File System
 */

#ifndef __3600FS_H__
#define __3600FS_H__
#include <time.h>
//Block Number
//4 Bytes
typedef struct block_num_t{
  unsigned int is_valid:1;
  unsigned int block:31;
} block_num;

//Free Block
//512 Bytes
typedef struct free_block_t{
  block_num next_block;//4 Bytes
  char padding[508];//Padding
}free_block;

//Volume Control Block
//512 Bytes
typedef struct VCB_t{
  unsigned int is_clean;//If the disk is clean
  block_num free;//Pointer to the first free block//4
  unsigned int size;//Disk size in blocks//4
  block_num root;//Root DINode//4
  char name[492];//Name and padding
  int magic;// = 80085;//Unique identifier//4
} VCB;

//Directory Entry
//36 Bytes
typedef struct DE_t{       // size (Bytes)
  block_num inode;         //  4.00 
  char name[31];           // 31.00
  unsigned int is_valid:1; //   .25 
  unsigned int is_file:1;  //   .25
  unsigned int padding:2;  //   .50
} DE;


//Directory Entry List
//512 Bytes
typedef struct DEL_t{
  block_num next; // 4 bytes
  DE DE_list[14]; // 504 bytes (14 DEs of size 36B)
  char padding[4];// 4 bytes for padding
} DEL;

//Directory INode
//512 Bytes
typedef struct DINode_t{
  //Need to add more metadata
  uid_t user;//4
  gid_t group;//4
  mode_t mode;//4
  time_t access_time;//4
  time_t modify_time;//4
  time_t create_time;//4
  block_num listof_DEL[119]; // an array of 4 byte block numbers
  // pointing to Directory entry lists 
  
  block_num single_in;//4 Bytes
  block_num double_in;//4 Bytes
  unsigned int size;//4 Bytes..In bytes
} DINode;

//File INode
//512 Bytes
typedef struct FINode_t{
  //Need to add more metadata
  uid_t user;
  gid_t group;
  mode_t mode;
  time_t access_time;
  time_t modify_time;
  time_t create_time;
  block_num blocks_used[119];
  block_num single_in;//4 Bytes
  block_num double_in;//4 Bytes
  unsigned int size;//4 Bytes..In bytes

} FINode;

//Single Indirect Block
//512 Bytes
typedef struct single_indirect_t{
  block_num blocks[128];
} single_indirect;


//Double Indirect Block
//512 bytes
typedef struct double_indirect_t{
  block_num single[128];
} double_indirect;

static size_t vfs_read_direct(char *buf, size_t size, block_num blocks[], int count);

//Not Written yet
static int vfs_read_single(char *buf, size_t size);

//Not Written yet
static int vfs_read_double(char *buf, size_t size);

static size_t vfs_write_direct(const char *buf, size_t size, block_num *blocks, int count);

static size_t vfs_write_single(char *buf, size_t size, block_num blocks);

static size_t vfs_write_double(char *buf, size_t size, block_num double_block);

static void vfs_free_direct(block_num* blocks, unsigned int count);

static void get_name(const char *path, char *name);

static DE* vfs_find_de(DEL *del, block_num *del_block);

static void get_subpath(const char* path, char* subpath);

#endif
