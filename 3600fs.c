

/*
 *   CS3600 Project 2: A User-Level File System
 */

#define FUSE_USE_VERSION 26

#ifdef linux
/* For pread()/pwrite() */
#define _XOPEN_SOURCE 500
#endif

#define _POSIX_C_SOURCE 199309

#include <fuse.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <assert.h>
#include <sys/statfs.h>
#include <sys/types.h>
#include <sys/stat.h>


#ifdef HAVE_SETXATTR
#include <sys/xattr.h>
#endif

#include "3600fs.h"
#include "disk.h"

//Remove a free block from the stack
void get_free(block_num* b){
  //Read the VCB
  VCB vcb;
  dread(0, (char *) &vcb);
  
  //Set the block_num that is being returned
  b->block = vcb.free.block;
  b->is_valid = 1;

  //Set the new free_block pointer
  free_block fb;
  dread(vcb.free.block,(char*) &fb);
  vcb.free = fb.next_block; 

  //Write the vcb back to disk
  dwrite(0, (char*)&vcb);
}

//Add a block to the free block stack
void add_free(block_num b){
  //Read in the VCB
  VCB vcb;
  dread(0, (char*)&vcb);
  
  //Create the free_block
  //and write
  free_block fb = {vcb.free, ""};
  dwrite(b.block, (char*)&fb);

  //Make the vcb point to the newly freed block_num
  //and write
  vcb.free = b;
  dwrite(0, (char*) &vcb);
}


// dirlevel
// char * path -> int
// root dir returns 0, 2nd dir returns 1, etc
int dirlevel(char *path){
  int level = 0;
  path++;

  char *our_path = (char*) malloc(50);
  char *original = our_path;
  strcpy(our_path, path);
  while(our_path != NULL){
    //printf("our_path:%s\n", our_path);
    our_path = strchr(our_path, '/');

    if(our_path != NULL){
      our_path++;
      level++;
    }
  }
  free(original);
  return level;
}

void pathtoarray(char *path, char **result){
  //Special case for root
  if(!strcmp(path, "/")){
    result[0] = "";
    return;
  }
    
  // set levels to equal the directory level
  int levels = dirlevel(path);
  printf("levels: %d\n", levels);
 
  // increment working_result, return the original pointer
  //Ask bart about this
  char * working_result[++levels];

  //remove forward slash
   path++;

  //change the arg path to a char[]
  char our_path[120];
  strcpy(our_path, path); 
  //printf("our_path: %s\n", our_path);

  // store the first part of our_path in part
  char *part;
  part = strtok(our_path, "/");
  //printf("part: %s\n", part);

  // store part in the first index of working_result
  // and increment it
  working_result[0] = part;
  strcpy(result[0], part);
  //printf("result[0]: %s\n",working_result[0]);
 
  int i;
  for(i = 1; i < levels; i++){
    part = strtok(NULL, "/");
    //printf("part: %s\n", part);
    working_result[i] = part;
    strcpy(result[i], part);
    printf("result[%d]: %s\n", i, working_result[i]);
    
  }
}

// delookup is a helper for the traverse function
// it takes the block of a del to iterate through
// if the dename is not found in one of the dir entries
// of the del, it recurses with the block_num of the next block
// returns an invalid DE if the dename wasn't found
// returns a valid DE from which you can look up the name or read the inode
void delookup(block_num * delblock, char * dename, DE *de){

  DE invalid;
  invalid.is_valid = 0; // initializes an invalid DE to return in error
  printf("dename parameter passed is :  %s\n", dename);
  DEL del;
  dread(delblock->block, (char*) &del);
  printf("on recursive call, dename is:  %s\n", dename);        
  int i;
  for (i = 0; i < 14; i++){
    if (del.DE_list[i].is_valid){
      printf("   within delookup, de_list_name[%d]:  %s\n",i, del.DE_list[i].name);
      if (strcmp(del.DE_list[i].name, dename) == 0){
	printf("    within delookup, returning this value: %s\n", del.DE_list[i].name);
	//delblock->is_valid = 1;

	de->is_valid = del.DE_list[i].is_valid;
	de->inode.is_valid = del.DE_list[i].inode.is_valid;
	de->inode.block = del.DE_list[i].inode.block;
	de->is_file = del.DE_list[i].is_file;
	strcpy(de->name, del.DE_list[i].name);
	return;
      }
    }
  }
  printf("on recursive call, dename is:  %s\n", dename);
  
  if (del.next.is_valid == 0)
    de->is_valid = 0;
  else {
    printf("on recursive call, dename is:  %s\n", dename);
    delblock->is_valid = del.next.is_valid;
    delblock->block = del.next.block;
    delookup(delblock, dename, de);
  }
}

// traverse
// it takes a path and returns the block_num of 
// the DEL that the path is referring to
// the pathname can refer to a file, directory, etc.
// the returning block_num will be a DEL (Directory Entry List)
// so when calling traverse you'll still have to do a for loop through
// the 14 directory entries in the DEL to get to the DE of the file/directory

void traverse(char * path, block_num * delblock){
  //Special case for root
  if(!strcmp(path, "/") || !(strcmp(path, ""))){
    delblock->is_valid = 1;
    delblock->block = 2;
    return;
  }
  printf("Sizeof char**:%d\n", sizeof(char**));
  char **patharray = malloc(DEPTH * sizeof(char *));
  int i;
  for(i = 0; i < DEPTH; i++)
    patharray[i] = malloc(31 * sizeof(char));

  pathtoarray(path, patharray); 
  
  printf("path[0]:%s\n", patharray[0]);
  int directorylevel = dirlevel(path);

  DE de; // address to load a DE into
  DINode inode; // address to load an Dinode into
  delblock->is_valid = 1;
  delblock->block = 2; // initialized to root DEL, gets returned
  
  for (i = 0; i <= directorylevel; i++){ //dirlevel returns 0-based level
    printf("part of the path being looked up:  %s\n", patharray[i]);

    //char * acharstar = (char *) malloc(strlen(patharray[i] + 1));
    //strcpy(acharstar, (char *)patharray[i]);
    // now, find the de whose name matches this part of the path
    delookup(delblock, patharray[i], &de); 
    printf("delblock.block post delookup is:  %d\n", delblock->block);
    
    printf("de.name post delookup:  %s\n", de.name);

    if (de.is_valid == 0){
      delblock->is_valid = 0;
    }
      // if delookup returns an invalid de, that means it 
      // didn't find it in the directory, so either the path 
      // is wrong or the file doesn't exist
    
    // if de is a file, then break and return the block_num
    if (de.is_file)
      break;
    
    // if not, it's a directory, and we want delblock to be
    // the block_num of the next DEL
    else if( i + 1 <= directorylevel){
      dread(de.inode.block, (char*) &inode);
      delblock->is_valid = inode.listof_DEL[0].is_valid;
      delblock->block = inode.listof_DEL[0].block;
    }
    

  } // for loop
  printf("name of de at return: %s\n",de.name);
  for(i = 0; i < DEPTH; i++)
    free((void *)patharray[i]);
  free((void *)patharray);
  printf("Just freed the path array\n");
}

static int exists(char* path){
  block_num bn;
  traverse(path, &bn);
  return bn.is_valid;
}

/*
 * Initialize filesystem. Read in file system metadata and initialize
 * memory structures. If there are inconsistencies, now would also be
 * a good time to deal with that. 
 *
 * HINT: You don't need to deal with the 'conn' parameter AND you may
 * just return NULL.
 *
 */
static void* vfs_mount(struct fuse_conn_info *conn) {
  conn = conn;
  
  fprintf(stderr, "vfs_mount called\n");
  /*
  VCB vcb;
  dread(0, &vcb);
  //Inconsistent
  if(vcb.magic != 80085){
    return -1;
  }
  else{
    vcb.magic = 55378008;
    dwrite(0, &vcb);
  }
  */
  // Do not touch or move this code; connects the disk
  dconnect();

  /* 3600: YOU SHOULD ADD CODE HERE TO CHECK THE CONSISTENCY OF YOUR DISK
           AND LOAD ANY DATA STRUCTURES INTO MEMORY */

  printf("About to return out of vsf_mount\n");
  return NULL;
}

/*
 * Called when your file system is unmounted.
 *
 */
static void vfs_unmount (void *private_data) {
  private_data = private_data;
  fprintf(stderr, "vfs_unmount called\n");

  /* 3600: YOU SHOULD ADD CODE HERE TO MAKE SURE YOUR ON-DISK STRUCTURES
           ARE IN-SYNC BEFORE THE DISK IS UNMOUNTED (ONLY NECESSARY IF YOU
           KEEP DATA CACHED THAT'S NOT ON DISK */
  
  VCB vcb;
  dread(0, &vcb);
  vcb.magic = 80085;
  dwrite(0, &vcb);

  // Do not touch or move this code; unconnects the disk
  dunconnect();
}

/* 
 *
 * Given an absolute path to a file/directory (i.e., /foo ---all
 * paths will start with the root directory of the CS3600 file
 * system, "/"), you need to return the file attributes that is
 * similar stat system call.
 *
 * HINT: You must implement stbuf->stmode, stbuf->st_size, and
 * stbuf->st_blocks correctly.
 *
 */
static int vfs_getattr(const char *path, struct stat *stbuf) {
  printf("**In vfs_getattr**\n");
  printf("**Getting attributes of '%s'** \n", path);
  fprintf(stderr, "vfs_getattr called \n");

  if(!strcmp(path, "")){
    printf("**Path was empty, breaking early and returning 0**\n");
    return 0;
  }
  if(!exists(path))
    return -2;

  int i;
  block_num bn;
  traverse(path, &bn);
  char* name = (char*)malloc(31);
  get_name(path, name);
  DEL del;
  dread(bn.block, (char*)&del);
  FINode node;
  int is_file = 1;
  for(i = 0; i < 14; i++){
    //If root don't go on
    if(strcmp(name, "")) {
      //If the file name and DE name is the same
      if(!strcmp(name, del.DE_list[i].name)){
	is_file = del.DE_list[i].is_file;
	dread(del.DE_list[i].inode.block, (char*)&node);
	node.access_time = time(NULL);
	break;
      }
    }
  }
  
  free(name);
 
  if(i == 14 && !strcmp(path, "/"))
    dread(1, (char*)&node);
  
  // Do not mess with this code 
  stbuf->st_nlink = 1; // hard links3600fs.h

  stbuf->st_rdev  = 0;
  stbuf->st_blksize = BLOCKSIZE;
  
  /* 3600: YOU MUST UNCOMMENT BELOW AND IMPLEMENT THIS CORRECTLY */
  
  if (strcmp(path, "/") == 0)
    stbuf->st_mode  = 0777 | S_IFDIR;
  else if (!is_file)
    stbuf->st_mode = node.mode | S_IFDIR;
  else
    stbuf->st_mode  = node.mode | S_IFREG;
    
    
  stbuf->st_uid     = node.user;// file uid
  stbuf->st_gid     = node.group;// file gid
  stbuf->st_atime   = node.access_time;// access time 
  stbuf->st_mtime   = node.modify_time;// modify time
  stbuf->st_ctime   = node.create_time;// create time
    
  stbuf->st_blocks= node.size/BLOCKSIZE;
  if(node.size%BLOCKSIZE)
    stbuf->st_blocks++;
  stbuf->st_size    = node.size;// file size in bytes
  printf("Size:%d, Exiting vfs_getattr\n", node.size);
  
  return 0;
}

/*
 * Given an absolute path to a directory (which may or may not end in
 * '/'), vfs_mkdir will create a new directory named dirname in that
 * directory.
 *
 * HINT: Don't forget to create . and .. while creating a
 * directory. You can ignore 'mode'.
 *
 */
  /* 3600: YOU CAN IGNORE THIS METHOD, UNLESS YOU ARE COMPLETING THE 
          EXTRA CREDIT PORTION OF THE PROJECT 
  */
static int vfs_mkdir(const char *path, mode_t mode) {
  printf("**vsf_mkdir**");
  printf("**Path passed in:%s\n", path);
  if(exists(path))
    return -1;
  int return_code = 0;
  int i;
  char* name = (char*)malloc(31);
  get_name(path, name);
  char* subpath = malloc(strlen(path) + 3);
  get_subpath(path, subpath);
  strcat(subpath, ".");
  // char*tmp = strrchr(path, '/');
  //*tmp = '\0';
  printf("Path that the directory should be added to:%s", path);
  printf("Name of directory:%s\n", name);
  block_num bn;
  traverse(path, &bn);
  
  block_num dnode_b;//parent del needs to point to this
  block_num del_b;
  get_free(&dnode_b);
  get_free(&del_b);
  printf("dnode block:%d\n", dnode_b.block);
  printf("del block:%d\n", del_b.block);
  printf("parent_del block:%d\n", bn.block);
   
  DEL new_del;
  new_del.next.is_valid = 0;
  for(i = 2; i<14; i++){
    new_del.DE_list[i].is_valid = 0;
  }

  DINode dnode;
  dnode.user = geteuid();//4
  dnode.group = getgid();//4
  dnode.mode = 0777;//4
  dnode.access_time = time(NULL);
  dnode.modify_time = time(NULL);
  dnode.create_time = time(NULL);
  dnode.listof_DEL[0] = del_b;
  for( i = 1; i < 119; i++)
    dnode.listof_DEL[i].is_valid = 0;
  dnode.single_in.is_valid = 0;
  dnode.double_in.is_valid = 0;
  dnode.size = 0;

  DEL parent_del;
  dread(bn.block, (char*)&parent_del);
      
  //.
  //dir entry 0
  new_del.DE_list[0].inode = dnode_b;
  new_del.DE_list[0].is_valid = 1;
  new_del.DE_list[0].is_file = 0;
  strcpy(new_del.DE_list[0].name, ".");
    
  //..
  //dir entry 1
  new_del.DE_list[1].inode = parent_del.DE_list[0].inode;   
  new_del.DE_list[1].is_valid = 1;
  new_del.DE_list[1].is_file = 0;
  strcpy(new_del.DE_list[1].name, "..");


    
  //Set the parent's DEL to include the new DIR
  for(i = 0; i < 14; i++){
      if(!parent_del.DE_list[i].is_valid){
	parent_del.DE_list[i].inode.block = dnode_b.block;
	parent_del.DE_list[i].inode.is_valid = 1;
	strcpy(parent_del.DE_list[i].name, name);
	parent_del.DE_list[i].is_valid = 1;
	parent_del.DE_list[i].is_file = 0;
	break;
    }
  }
  dwrite(bn.block, (char*)&parent_del);
  dwrite(dnode_b.block, (char*)&dnode);
  dwrite(del_b.block, (char*)&new_del);
  
  printf("I'm about to return:%d\n", return_code);
  return return_code;
} 

/** Read directory
 *
 * Given an absolute path to a directory, vfs_readdir will return 
 * all the files and directories in that directory.
 *
 * HINT:
 * Use the filler parameter to fill in, look at fusexmp.c to see an example
 * Prototype below
 *
 * Function to add an entry in a readdir() operation
 *
 * @param buf the buffer passed to the readdir() operation
 * @param name the file name of the directory entry
 * @param stat file attributes, can be NULL
 * @param off offset of the next entry or zero
 * @return 1 if buffer is full, zero otherwise
 * typedef int (*fuse_fill_dir_t) (void *buf, const char *name,
 *                                 const struct stat *stbuf, off_t off);
 *			   
 * Your solution should not need to touch offset and fi
 *
 */
static int vfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                       off_t offset, struct fuse_file_info *fi)
{
  printf("**In vsf_readdir**\n");
  printf("**Path is %s\n", path);
  offset = offset;
  fi = fi;

  // reads in the first DE list to del
  block_num bn;
  char* name = malloc(strlen(path) + 3);
  strcpy(name, path);
  //If not root add a "/."
  if(strcmp(path, "/"))
     strcat(name, "/.");
  traverse(name, &bn);
  DEL del;
  printf("Disk block for our DEL is:%d", bn.block);
  int diskblock = bn.block;
  dread(diskblock, (char*)&del);
  
  int i;
  for(i = 0; i < 14; i++){
    if(del.DE_list[i].is_valid){
      
      printf("Directory Entry name:%s\n", del.DE_list[i].name);

      filler(buf, del.DE_list[i].name, NULL, 0);
    }
    if( i == 13 && del.next.is_valid){
      dread(del.next.block, (char*)&del);
      i = -1;
    }
    // will need an else clause eventually if there are more entries
    // than fit in the DEL
  }
  free(name);
  printf("Exiting vfs_readdir\n");  
  return 0;
}

static void get_subpath(const char* path, char* subpath){
  strcpy(subpath, path);
  char* tmp = strrchr(subpath, '/');
  tmp++;
  *tmp = '\0';
}
 
/*
 * Given an absolute path to a file (for example /a/b/myFile), vfs_create 
 * will create a new file named myFile in the /a/b directory.
 *
 * HINT: Your solution should ignore rdev
 *
 */
static int vfs_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
  printf("**In vsf_create**\n");
  int return_code = 0;
  mode = mode;
  fi = fi;
  if(exists(path))
    return -1;


  // remove left slash from path, save the file name
  char* name = (char*) malloc(31);
  char* subpath = (char*) malloc(130);
  get_subpath(path, subpath);
  strcat(subpath, ".");
  get_name(path, name);
  printf("**Subpath:%s\n", subpath);
  printf("**Name:%s\n", name);
 
  block_num bn;
  DEL del;
  DE *de = (DE*) malloc(sizeof(DE));
  traverse(subpath, &bn);
  dread(bn.block, (char*) &del);
 
  de = vfs_find_de(&del, &bn);
  
  //Create the new INode
  FINode finode;
  finode.user = geteuid();
  finode.group = getgid();
  finode.mode = mode;
  finode.access_time = time(NULL);
  finode.create_time = time(NULL);
  finode.modify_time = time(NULL);
  finode.single_in.is_valid = 0;
  finode.double_in.is_valid = 0;
  finode.size = 0;
  int i;
  for(i = 0; i < 119; i++)
    finode.blocks_used[i].is_valid = 0;
  block_num b;
  get_free(&b);
  dwrite(b.block, (char*)&finode);
  //For final change inode
    
  strcpy(de->name, name);
  de->is_valid = 1;
  de->is_file = 1;
  de->inode.is_valid = 1;
  de->inode.block = b.block;
  
  dwrite(bn.block, (char*)&del);

  
  return return_code;
}

static DE* vfs_find_de(DEL *del, block_num *del_block){
  char * name;
  mode_t mode;
  int i;
  //find an empty DE
  for(i = 0; i < 14; i++){
    if(!del->DE_list[i].is_valid){
      return &del->DE_list[i];
    }
  }
  
  //original DEL is full go to next
  if(i == 14){
    //If next exists recurse
    if(del->next.is_valid){
      dread(del->next.block, (char*)del);
      del_block->block = del->next.block;
      return vfs_find_de(del, del_block);
    }
    //If not create a new DEL and set return the address of the
    // first spot in the new DEL because we know it is empty
    else{
      //Write the old DEL to disk
      block_num bn;
      get_free(&bn);
      
      
      del->next.is_valid = 1;
      del->next.block = bn.block;
      dwrite(del_block->block, (char*) del);
      del_block->is_valid = 1;
      del_block->block = bn.block;
      
      //Now assign a new values to the del
      del->next.is_valid = 0;
      del->DE_list[0].is_valid = 1;
      for(i = 1; i < 14; i++)
	del->DE_list[i].is_valid = 0;
      return &del->DE_list[0];
    }
  }
}
/*
 * Sets name to the file name
 * Currently only works for files in the root directory
 */
static void get_name(const char *path, char *name){
  if(!strcmp(path, "")){
    strcpy(name, "");
    return;
  }
  char* tmp = strrchr(path, '/');
  tmp++;
  if(name == NULL)
    strcpy(tmp, "/");
  strcpy(name, tmp);
} 

/*
 * The function vfs_read provides the ability to read data from 
 * an absolute path 'path,' which should specify an existing file.
 * It will attempt to read 'size' bytes from the specified file 
 * on your filesystem into the memory address 'buf'. The return 
 * value is the amount of bytes actually read; if the file is 
 * smaller than size, vfs_read will simply return the most amount
 * of bytes it could read. 
 *
 * HINT: You should be able to ignore 'fi' and 'offset'
 *
 */
static int vfs_read(const char *path, char *buf, size_t size, off_t offset,
                    struct fuse_file_info *fi)
{
  printf("**In vfs_read**");
  fi = fi;
  offset = offset;

  int return_code = 0;
  char* buf_temp = buf;
  int i;
  block_num bn;
  int size_read;
  traverse(path, &bn);
  char* name = (char*) malloc(31);
  get_name(path, name);
  DEL del;
  dread(bn.block, (char*)&del);
  for(i = 0; i < 14; i++){
    if(!strcmp(name, del.DE_list[i].name)){
      FINode finode;
      dread(del.DE_list[i].inode.block, (char*)&finode);
      finode.access_time = time(NULL);
      size_read = finode.size;
      if(size > finode.size)
	size = finode.size;
      size = vfs_read_direct(buf, size, finode.blocks_used, 119);
      if(size > 0)
	size = vfs_read_single(buf, size);
      if(size > 0)
	size = vfs_read_double(buf, size);
      buf = buf_temp;
      break;
    } 
  } 
  if(i == 14){
    return_code = -1;
    printf("File '%s' not found.", path);
  }
  
  return size_read;
}


//Sets as much as size data into the buffer, returns how much data still needs to be assigned from other sources(i.e. indirects)
static size_t vfs_read_direct(char *buf, size_t size, block_num blocks[], int count){
  int i;
  char* tmp = (char*) malloc(512);
  for(i = 0; i < count && size > 0; i++){
    if(!blocks[i].is_valid){
      printf("ERROR, trying to read an invalid block, this should never happen");
    }
    dread(blocks[i].block,(char*) tmp);
    if(size > 512){
      memcpy(buf, tmp, 512);
      buf += 512;
      size -= 512;
    }
    else{
      memcpy(buf, tmp, size);
      size = 0;
    }
  }
  free(tmp);
  return size;
}

//Not Written yet
static int vfs_read_single(char *buf, size_t size){
  buf = buf;
  return size;
}


//Not Written yet
static int vfs_read_double(char *buf, size_t size){
  buf = buf;
  return size;
  
}

/*
 * The function vfs_write will attempt to write 'size' bytes from 
 * memory address 'buf' into a file specified by an absolute 'path'
 *
 * HINT: Ignore 'fi' and 'offset'
 *
 */
//TODO: Write should not overwrite data if the new size is smaller then the old size
static int vfs_write(const char *path, const char *buf, size_t size,
                     off_t offset, struct fuse_file_info *fi)
{
  printf("**In vfs_write**\n");
  fi = fi;
  if(!exists(path))
    return -1;
  
  /* 3600: NOTE THAT IF THE OFFSET+SIZE GOES OFF THE END OF THE FILE, YOU
           MAY HAVE TO EXTEND THE FILE (ALLOCATE MORE BLOCKS TO IT). */
  size_t original_size = size;
  int size_wrote = 0;
  int return_code = 0;
  char* buf_temp = buf;
  int i;
  block_num bn;
  traverse(path, &bn);
  char *name = (char*) malloc(31);
  get_name(path, name);
  DEL del;
  dread(bn.block, (char*)&del);
  for(i = 0; i < 14; i++){
    if(!strcmp(name, del.DE_list[i].name)){
      FINode finode;
      //    finode.access_time = time(NULL);
      //    finode.modify_time = time(NULL);
      
      
      dread(del.DE_list[i].inode.block, (char*)&finode);

      //      if(size > finode.size)
      //	size = finode.size;

      int block_count = size / BLOCKSIZE;
      if(size  % BLOCKSIZE)
	block_count++;

      printf("Number of blocks needed:%d\n", block_count);

       size = vfs_write_direct(buf, original_size, finode.blocks_used, 119);
      if(size > 0){
	if(!finode.single_in.is_valid){
	  block_num single_block;
	  get_free(&single_block);
	  finode.single_in.is_valid = 1;
	  finode.single_in.block = single_block.block;
	  block_num s_blocks[128];
	  int k;
	  for(k = 0; k < 128; k++)
	    s_blocks[i].is_valid = 0;
	  dwrite(single_block.block, &s_blocks);
	}

	size = vfs_write_single(buf, size, finode.single_in);
      }
      if(size > 0){
	if(!finode.double_in.is_valid){
	  block_num double_block;
	  get_free(&double_block);
	  finode.double_in.is_valid = 1;
	  finode.double_in.block = double_block.block;
	  block_num d_blocks[128];
	  int k;
	  for(k = 0; k < 128; k++)
	    d_blocks[i].is_valid = 0;
	  dwrite(double_block.block, &d_blocks);
	}
	size = vfs_write_double(buf, size, finode.double_in);
      }

      size_wrote = original_size;
      buf = buf_temp;
      finode.size = size_wrote;
      dwrite(del.DE_list[i].inode.block, (char*)&finode);
    } 
  }

  free(name);

  if(i == 14)
    return -1;
  
  return original_size;
}

//Sets as much as size data into the buffer, returns how much data still
//needs to be assigned from other sources(i.e. indirects)
  static size_t vfs_write_direct(const char *buf, size_t size, block_num *blocks, int count){
    int i;
    char* tmp = (char*)malloc(BLOCKSIZE);
    
    for(i = 0; i < count && size > 0; i++){
      if(!blocks[i].is_valid){
	get_free(&blocks[i]);
      }
      
      if(size > BLOCKSIZE){
	dwrite(blocks[i].block, buf);
	size -= BLOCKSIZE;	
	buf  += BLOCKSIZE;
      } else {
	memset(tmp, 0, sizeof(tmp));
	memcpy(tmp, buf, size);
	dwrite(blocks[i].block, tmp);
	size = 0;
	buf += size;
      }
      printf("Size:%d\n", size);
    }
    return size;
  }

static size_t vfs_write_single(char *buf, size_t size, block_num single_block){
  block_num blocks[128];
  dread(single_block.block,(char*) &blocks);
  return vfs_write_direct(buf, size, &blocks, 128);

}

static size_t vfs_write_double(char *buf, size_t size, block_num double_block){
  int i;
  block_num blocks[128];
  dread(double_block.block, &blocks);
  for(i = 0; i < 128 && size > 0; i++){
    if(!blocks[i].is_valid)
      get_free(&blocks[i]);
    size = vfs_write_single(buf, size, blocks[i]);  
  }
  dwrite(double_block.block, (char*)&blocks);

  return size;

}

/**
 * This function deletes the last component of the path (e.g., /a/b/c you 
 * need to remove the file 'c' from the directory /a/b).
 */
static int vfs_delete(const char *path)
{
  int return_code = 0;
  printf("**In vfs_delete**\n");
  int i;
  block_num bn;
  traverse(path, &bn);
  char* name = (char*)malloc(31);
  get_name(path, name);
  DEL del;
  dread(bn.block, (char*)&del);
  for(i = 0; i < 14; i++){
    if(del.DE_list[i].is_valid)
      if(!strcmp(name, del.DE_list[i].name)){
	del.DE_list[i].is_valid = 0;
    
      FINode finode;
      dread(del.DE_list[i].inode.block, (char*)&finode);
      vfs_free_direct(finode.blocks_used, 119);
      add_free(del.DE_list[i].inode);
      break;
    }
  }
  
  if(i == 14)
    return_code = -1;
  else
    dwrite(bn.block, (char*)&del);

  free(name);
  return return_code;
}

static void vfs_free_direct(block_num * blocks, unsigned int count){
  unsigned  int i;
  for(i = 0; i < count; i++){
    if(blocks[i].is_valid)
      add_free(blocks[i]);
  }
}



/*
 * The function rename will rename a file or directory named by the
 * string 'oldpath' and rename it to the file name specified by 'newpath'.
 *
 * HINT: Renaming could also be moving in disguise
 *
 */
static int vfs_rename(const char *from, const char *to)
{
  printf("**vfs_rename**\n");

  char * src = (char*) malloc(strlen(from) + 1);
  strcpy(src, from); // src is the pathname of the source
  block_num srcblock;
  traverse(src, &srcblock);

  char * dest = (char*) malloc(strlen(to) + 1);
  strcpy(dest, to); // dest is the pathname of the destination

  int srclvl = dirlevel(src);
  int destlvl = dirlevel(dest);

  char **srcarray = malloc(DEPTH * sizeof(char *));

  int i;
  for(i = 0; i < DEPTH; i++)
    srcarray[i] = malloc(31 * sizeof(char));

  pathtoarray(src, srcarray); 

  char **destarray = malloc(DEPTH * sizeof(char *));
  for(i = 0; i < DEPTH; i++)
    destarray[i] = malloc(31 * sizeof(char));

  pathtoarray(dest, destarray); 
  char *srcname = (char*) malloc(31);
  char *destname = (char*) malloc(31);
  srcname = srcarray[srclvl]; // the file name without the path
  destname = destarray[destlvl]; // the file name without the path

  DEL srcdel; // del gets the DEL where the file currently exists
  dread(srcblock.block, &srcdel);
  DE srcde; // de gets the directory entry with name srcname
  delookup(&srcblock, srcname, &srcde);
  FINode srcinode; // load the inode we just created
  dread(srcde.inode.block,(char*) &srcinode);
  
  // create a new file at path dest, and load 
  // its DEL's address to destblock
  struct fuse_file_info *fi;
  vfs_create(dest, srcinode.mode, fi);
  block_num destblock;
  traverse(dest, &destblock);
  
  // use the destblock address to load that DEL into local memory
  DEL destdel;
  dread(destblock.block, (char*) &destdel);
  DE destde; // find the de that we just created
  delookup(&destblock, destname, &destde);
  FINode destinode; // load the inode we just created
  dread(destde.inode.block,(char*) &destinode);

  // free the inode that the new file points to
  // because that's blank
  vfs_free_direct(destinode.blocks_used, 119);
  add_free(destde.inode);

  // set the new file's DE to point to the source de's inode
  // we can't use destde - we have to update the de within destdel
  // because dwrite is writing the content from destdel
  for (i = 0; i < 14; i++){
    if (destdel.DE_list[i].is_valid){
      if (strcmp(destdel.DE_list[i].name, destname) == 0){
	destdel.DE_list[i].inode.block = srcde.inode.block;
	destdel.DE_list[i].inode.is_valid = srcde.inode.is_valid;
      }
      if (strcmp(destdel.DE_list[i].name, srcname) == 0){
	destdel.DE_list[i].is_valid = 0;
      }
    }
  }
  dwrite(destblock.block, (char *)&destdel);

  free(src);
  free(srcarray);
  free(dest);
  free(destarray);
  free(srcname);
  free(destname);

  return 0;
}


/*
 * This function will change the permissions on the file
 * to be mode.  This should only update the file's mode.  
 * Only the permission bits of mode should be examined 
 * (basically, the last 16 bits).  You should do something like
 * 
 * fcb->mode = (mode & 0x0000ffff);
 *
 */
static int vfs_chmod(const char *file, mode_t mode)
{

  //static void get_name(const char *path, char *name){

  printf("**vfs_chmod**\n");
  char * name = (char*) malloc(strlen(file) + 1);
  strcpy(name, file);
  block_num dirblock;
  // name is currently the path, with a forward slash to start
  traverse(name, &dirblock);
  get_name(name, name);
  // now name is just the name of the file (root dir only for now)  

  DEL del;
  dread(dirblock.block, &del);
  DE de;
  delookup(&dirblock, name, &de);
  FINode inode;
  dread(de.inode.block, &inode);
  
  inode.mode = (mode & 0x0000ffff);

  dwrite(de.inode.block, (char *) &inode);
  free(name);
  return 0;
}

/*
 * This function will change the user and group of the file
 * to be uid and gid.  This should only update the file's owner
 * and group.
 */
static int vfs_chown(const char *file, uid_t uid, gid_t gid)
{
  printf("**vfs_**\n");
  char * name = (char*) malloc(strlen(file) + 1);
  strcpy(name, file);
  block_num dirblock;
  traverse(name, &dirblock);
  get_name(name, name);

  DEL del;
  dread(dirblock.block, &del);
  DE de;
  delookup(&dirblock, name, &de);
  FINode inode;
  dread(de.inode.block, &inode);
  
  inode.user = uid;
  inode.group = gid;

  dwrite(de.inode.block, (char *) &inode);
  free(name);
  return 0;
}

/*
 * This function will update the file's last accessed time to
 * be ts[0] and will update the file's last modified time to be ts[1].
 */
static int vfs_utimens(const char *file, const struct timespec ts[2])
{
  printf("**vfs_**\n");

  char * name = (char*) malloc(strlen(file) + 1);
  strcpy(name, file);
  block_num dirblock;
  traverse(name, &dirblock);

  DEL del;
  dread(dirblock.block, &del);
  DE de;
  delookup(&dirblock, &name, &de);
  FINode inode;
  dread(de.inode.block, &inode);
  
  inode.access_time = clock_gettime(CLOCK_REALTIME, &ts[0]);
  inode.modify_time = clock_gettime(CLOCK_REALTIME, &ts[1]);

  dwrite(de.inode.block, (char *) &inode);

  free(name);
  return 0;
}

/*
 * This function will truncate the file at the given offset
 * (essentially, it should shorten the file to only be offset
 * bytes long).
 */
static int vfs_truncate(const char *file, off_t offset)
{
  printf("**vfs_**\n");
  file = file;
  offset = offset;
  /* 3600: NOTE THAT ANY BLOCKS FREED BY THIS OPERATION SHOULD
           BE AVAILABLE FOR OTHER FILES TO USE. */

    return 0;
}


/*
 * You shouldn't mess with this; it sets up FUSE
 *
 * NOTE: If you're supporting multiple directories for extra credit,
 * you should add 
 *
 *     .mkdir	 = vfs_mkdir,
 */
static struct fuse_operations vfs_oper = {
    .mkdir   = vfs_mkdir,
    .init    = vfs_mount,
    .destroy = vfs_unmount,
    .getattr = vfs_getattr,
    .readdir = vfs_readdir,
    .create	 = vfs_create,
    .read	 = vfs_read,
    .write	 = vfs_write,
    .unlink	 = vfs_delete,
    .rename	 = vfs_rename,
    .chmod	 = vfs_chmod,
    .chown	 = vfs_chown,
    .utimens	 = vfs_utimens,
    .truncate	 = vfs_truncate,
};

int main(int argc, char *argv[]) {
    /* Do not modify this function */
    umask(0);
    if ((argc < 3) || (strcmp("-d", argv[1]))) {
      printf("Usage: ./3600fs -d <dir>\n");
      exit(-1);
    }
    return fuse_main(argc, argv, &vfs_oper, NULL);
}
