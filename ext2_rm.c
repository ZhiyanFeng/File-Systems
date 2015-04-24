#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "ext2.h"
#include <string.h>
#include <assert.h>


unsigned char *disk;
char *pad_name;
unsigned char *block_bm;
unsigned short new_rec_len;
unsigned char *inode_bm;
struct ext2_group_desc *gd;
struct ext2_super_block *sb;
struct ext2_inode *ip;
int token_count;
int count2;
struct ext2_dir_entry_2 *before;
struct ext2_dir_entry_2 *current;

void to_binary(unsigned int n){ 
  unsigned int i; 
  for (i = 0; i < 8; i++) {
    if (n & (1 << i)) { 
      printf("1");
    } else { 
      printf("0");
    } 
    if ((i + 1) % 8 == 0 && i != 8) { 
      printf(" "); 
    }
  }
}

unsigned get_inode_num(struct ext2_inode *ip, char *name){
  int block_num = ip->i_block[0];

  /* offset the address */
  struct ext2_dir_entry_2 *db = (struct ext2_dir_entry_2 *)(DATABLOCK_OFFSET(block_num));

  int len=0;
  while(len<1024) {
    if(!strncmp(db->name,name, db->name_len)){
      return db->inode;
    }
    len +=db->rec_len;
    char * db_inuse = (char*)db;
    db_inuse += db->rec_len;
    db = (struct ext2_dir_entry_2 *)db_inuse;
  }
  return 0;
}

int get_inodes(struct ext2_inode *ip, char *name){
  int block_num = ip->i_block[0];

  /* offset the address */
  struct ext2_dir_entry_2 *db = (struct ext2_dir_entry_2 *)(DATABLOCK_OFFSET(block_num));

  int len=0;
   while(len<1024) {
    if(!strncmp(db->name,name, db->name_len)){
      current = db;
      return 1;
    }
    len +=db->rec_len;
    char * db_inuse = (char*)db;
    before = db;
    db_inuse += db->rec_len;
    db = (struct ext2_dir_entry_2 *)db_inuse;
  }
  return 0;
}

void parse_path(char *path, char *path_token[], char *delimiter){
  /* make the dest_token points to parsed path */
 
  int path_idx = 0;
  char *token = strtok(path, delimiter);
  while( token != NULL ) 
    {
      path_token[path_idx]=token;
      token = strtok(NULL, "/");
      path_idx++;
    }
  token_count = path_idx;  
  path_token[path_idx] = token; //set the end of path_token to null;
}

struct ext2_inode *get_inode(struct ext2_inode *current_root, unsigned char * inode_bm, 
           struct ext2_inode *ip, char *path_token[]){

  int pt_idx=0;        //path token index
  int inode_number;
  int byte = 0;
  int bit = 0;
  while(path_token[pt_idx]!=NULL){
      inode_number = get_inode_num(current_root, path_token[pt_idx]);
    if(inode_number==0){
      //perror("ENOENT");
      //exit(1);
      return NULL;
    }
    byte = inode_number / 8;
    bit = inode_number % 8;
    if (inode_bm[byte -1] & 1 << (bit-1))
      {
  current_root= ip+ inode_number-1;
      }
      
    pt_idx++;
  }
  if(current_root->i_mode & 0x8000){
       perror("EISDIR");
      exit(1);
    }
  return current_root;
}

struct ext2_inode *check_inode(struct ext2_inode *current_root, unsigned char * inode_bm, 
           struct ext2_inode *ip, char *path_token[]){

  int pt_idx=0;        //path token index
  int inode_number;
  int byte = 0;
  int bit = 0;
  while(path_token[pt_idx]!=NULL && pt_idx < token_count-1){
    inode_number = get_inode_num(current_root, path_token[pt_idx]);
    if(inode_number==0){
      perror("ENOENT");
      exit(1);
    }
    byte = inode_number / 8;
    bit = inode_number % 8;
    if (inode_bm[byte -1] & 1 << (bit-1))
      {
  current_root= ip+ inode_number-1;
      }
      
    pt_idx++;
  }

  return current_root;
}

//set first free inode
void set_free_inode_num(unsigned char *bm,int pos){
  int byte =pos/8;
  int shift = pos%8;
  bm[byte] = bm[byte] ^ 1<< shift;
}


/******************ABOVE IS THE HELPER FUNCITON*************** */
int main(int argc, char **argv) {
  block_bm = malloc(sizeof(char) * 128);

  if(argc != 3) {
    fprintf(stderr, "Usage: ext2_rm <image file name> <path of destination>\n");
    exit(1);
  }

  int fd = open(argv[1], O_RDWR);
  assert(fd!=-1);
    
  disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if(disk == MAP_FAILED) {
    perror("mmap");
    exit(1);
  }

  /* *******print information just for convience************ */
  
  sb = (struct ext2_super_block *)(disk + BASE_OFFSET);
   gd = (struct  ext2_group_desc *)(disk + BLOCK_OFFSET(2));
   block_bm = DATABLOCK_OFFSET(gd->bg_block_bitmap);
   inode_bm =  DATABLOCK_OFFSET(gd->bg_inode_bitmap);
  ip = (struct ext2_inode *)(INODETABLE_OFFSET(disk));

 
  // **************parse the path*******************
  /* token the path */
  char *delimiter = "/";
  char *path_token[EXT2_NAME_LEN];
  char *parent_path_token[EXT2_NAME_LEN];
  //char *source;
  char *target;

 parse_path(argv[2], path_token, delimiter);
 target = path_token[token_count-1];
 count2 = token_count;
   
  /* find the inode of destination dir */
   struct ext2_inode *root_inode= ip+1;
  
  struct ext2_inode *target_inode = get_inode(root_inode, inode_bm, ip, path_token);
 if (target_inode == NULL)
 {
   perror("ENOENT");
   exit(1);
 }

 int j;
 for(j=0;j<token_count-1;j++){
   parent_path_token[j]= path_token[j];
}
 parent_path_token[token_count-1] = NULL;


struct ext2_inode *parent_inode = get_inode(root_inode, inode_bm, ip, parent_path_token);

 get_inodes(parent_inode, target);

 before->rec_len +=current->rec_len;
 unsigned inode_to_delete = current->inode;

 if(target_inode->i_links_count > 1){
  target_inode->i_links_count --;
  return 0;
}

 int i=0;
 while (target_inode->i_block[i] != 0) {
   set_free_inode_num(block_bm, target_inode->i_block[i]);
   i++;
 }
 set_free_inode_num(inode_bm,inode_to_delete);
 return 0;
}
