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
long file_byte;
char *pad_name;
unsigned char *block_bm;
int pad_len;
int file_entry_len;
unsigned short new_rec_len;
unsigned char *inode_bm;
struct ext2_group_desc *gd;
struct ext2_super_block *sb;
struct ext2_inode *ip;
int fn_len;
int token_count;

void to_binary(unsigned int n){ 
  unsigned int i; 
  for (i = 0; i < 8; i++) {
    if (n & (1 << i)) { 
      //printf("1");
    } else { 
      //printf("0");
    } 
    if ((i + 1) % 8 == 0 && i != 8) { 
      //printf(" "); 
    }
  }
}

void print_bb(struct ext2_inode *ip, int node_num){
char t= GETTYPE(ip);
 printf("[%d] type: %c size: %d links: %d blocks: %d\n[%d] Blocks: %d\n",
  node_num, t, ip->i_size,ip->i_links_count, ip->i_blocks, node_num, ip->i_block[0]);
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

void print_inode_info(struct ext2_inode *ip, char *bb){
  int node_num=0;
  if(bb[1] =='1'){
    node_num = 2;
    ip++;
    print_bb(ip, node_num);
    ip--;
  }
}

struct ext2_inode *get_inode(struct ext2_inode *current_root, unsigned char * inode_bm, 
           struct ext2_inode *ip, char *path_token[]){

  int pt_idx=0;        //path token index
  int inode_number;
  int byte = 0;
  int bit = 0;
  while(path_token[pt_idx]!=NULL){
    if(S_ISREG(current_root->i_mode)){
      perror("ENOENT");
      exit(1);
    }
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

void print_superblock(struct ext2_super_block *sb){
  printf("Inodes: %d\n", sb->s_inodes_count);
  printf("Blocks: %d\n", sb->s_blocks_count);
}

void print_block_group (struct ext2_group_desc *gd){
  printf("block block_bm: %d\n", gd->bg_block_bitmap);
  printf("inode block_bm: %d\n", gd->bg_inode_bitmap);
  printf("Inode table: %d\n", gd->bg_inode_table);
  printf("free blocks: %d\n", gd->bg_free_blocks_count);
  printf("free inodes: %d\n", gd->bg_free_inodes_count);
  printf("used_dirs: %d\n", gd->bg_used_dirs_count);
}

int get_file_byte(char *file){
   FILE *fp;

   fp = fopen(file, "r");
   if( fp == NULL ) 
   {
      perror ("ENOENT");
      exit(1);
   }
   fseek(fp, 0, SEEK_END);

   file_byte = ftell(fp);
   fclose(fp);
   return 0;
}

//get first free inode
unsigned int get_free_inode_num(unsigned char *bm, int bm_type){
  int bitcounter=0, byte, test_bit;
  for (byte=0; byte < bm_type; byte++) {
    int j;
    for(j=0;j<8;j++){
      test_bit = 1 << j;

      if(!(bm[byte] & test_bit)){
  //found a free one
  bm[byte] = bm[byte] | test_bit;
  bitcounter += j;
  assert(byte<4);

  return bitcounter +1;
      }
    }
    bitcounter += j;
  }
  return -1;
}

struct ext2_dir_entry_2 *get_free_entry_pos(struct ext2_inode *ip)  
{
  int i=0;
  int block_num = 0;
  struct ext2_dir_entry_2 *db = NULL;
  while((block_num = ip->i_block[i]) != 0){

    /* offset the address */
    struct ext2_dir_entry_2 *db = (struct ext2_dir_entry_2 *)(DATABLOCK_OFFSET(block_num));

    unsigned short len=0;
    int act_len = 0;
    int rec_len=0;
    while(len<1024) {
      act_len = 8 + db->name_len +4 -(db->name_len % 4);
      rec_len = db->rec_len;
      if( (rec_len-act_len) >= pad_len){
        /* modify the rec_len */
        if (len + rec_len == 1024)
        {
          new_rec_len = 1024 - len - act_len;
          db->rec_len = act_len;          
        }else{
        db->rec_len = act_len;
        new_rec_len=rec_len-act_len;
        }
        char * db_inuse = (char*)db;
        db_inuse += db->rec_len;
        return db = (struct ext2_dir_entry_2 *)db_inuse;
      }else{
        len +=rec_len;
        char * db_inuse = (char*)db;
        db_inuse += rec_len;
        db = (struct ext2_dir_entry_2 *)db_inuse;
      }
      i++;
    }
  }
  return db;
}

void pad_file_name(char *filename){
  int padding = 0;
  if(fn_len % 4){
    padding = 4- fn_len%4;
  }
  pad_len = fn_len + padding;
  pad_name = malloc(pad_len);
  strcpy(pad_name, filename);
  while (fn_len<pad_len) {
    pad_name[fn_len] = '\0';
    fn_len++;
  }
  pad_len +=8;
}

int read_file(int fd, char *buffer, int *size){
       
  if((*size = read(fd,buffer,EXT2_BLOCK_SIZE)) != EXT2_BLOCK_SIZE)  return 1;
        printf("%s\n",buffer);
 
        if(lseek(fd,EXT2_BLOCK_SIZE+1,SEEK_SET) < 0) return 1;
 
        return 0;
}

/******************ABOVE IS THE HELPER FUNCITON*************** */
int main(int argc, char **argv) {
  extern long file_byte;
  block_bm = malloc(sizeof(char) * 128);

  if(argc != 4) {
    fprintf(stderr, "Usage: ext2_ls <image file name> <path of source> \
      <path of destination\n");
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
  //print_superblock(sb);


  gd = (struct  ext2_group_desc *)(disk + BLOCK_OFFSET(2));
  //print_block_group(gd);

  //go to block block_bm
    
  block_bm = DATABLOCK_OFFSET(gd->bg_block_bitmap);
    
  inode_bm =  DATABLOCK_OFFSET(gd->bg_inode_bitmap);
  
  ip = (struct ext2_inode *)(INODETABLE_OFFSET(disk));

  /* print the block block_bm */
    //printf("block block_bm:");
  int bm_i = 0;
   
  for (bm_i = 0; bm_i < 16; bm_i++) {
    to_binary(block_bm[bm_i]);
  }
  //printf("\n");

  /* print the inode block_bm */
  //printf("inode block_bm:");
  int ibm_i = 0;
  for (ibm_i=0; ibm_i< 4; ibm_i++) {
    to_binary(inode_bm[ibm_i]);
  }
  //printf("\n");

  /* get the file_len in native system */
  fn_len = strlen(argv[2]);
  get_file_byte(argv[2]);

  pad_file_name(argv[2]);
   int source_fd = open(argv[2], O_RDWR);
  assert(fd!=-1);

  // **************parse the path*******************
  char *path=argv[3];
  /* token the path */
  char *delimiter = "/";
  char *path_token[EXT2_NAME_LEN];
  char *parent_path_token[EXT2_NAME_LEN];

  parse_path(path, path_token, delimiter);
   
  int to_idx;
  for (to_idx = 0; to_idx < token_count; to_idx++)
  {
    parent_path_token[to_idx] = path_token[to_idx];
  }

  parent_path_token[token_count] = NULL;
  /* find the inode of destination dir */

  struct ext2_inode *root_inode= ip+1;
  
  struct ext2_inode *working_inode = get_inode(root_inode, inode_bm, ip, parent_path_token);
 //Check if the file exists
  unsigned int check_node = get_inode_num(working_inode,argv[2]);
  if (check_node != 0)
   {
     perror("EEXIST");
    exit(1);
   } 

 /*get free inode position  */
  unsigned int inode_entry_num = get_free_inode_num(inode_bm,4);
  struct ext2_inode *free_inode_entry = ip + inode_entry_num -1 ;

  /* get the  free entry position*/
  struct ext2_dir_entry_2 *free_entry = get_free_entry_pos(working_inode);

  /* initialize the entry */
  int memcpysize = sizeof(struct ext2_dir_entry_2)+fn_len; 
  struct ext2_dir_entry_2 *new_dir = malloc(memcpysize);
  new_dir->inode = inode_entry_num;
  new_dir->rec_len = new_rec_len;
  new_dir->name_len = strlen(argv[2]);
  new_dir->file_type = 1;
  strcpy(new_dir->name, pad_name);
  memcpy(free_entry, new_dir, memcpysize);
 
  /* initialize the inode */
  int memcpysize_inode = sizeof(struct ext2_inode);
struct ext2_inode *new_inode = malloc(memcpysize_inode);

//Initialize i_block of new_inode to 0
int ibo;
for (ibo = 0; ibo < 15; ibo++)
{
  new_inode->i_block[ibo] = 0;
}

new_inode->i_mode = EXT2_S_IFREG;
new_inode->i_links_count = 0;
unsigned int file_len;
unsigned int block=0;
ibo = 0;
 char *buffer =malloc(EXT2_BLOCK_SIZE);
 int read_size;
 do{
   block = get_free_inode_num(block_bm,16);
   read_file(source_fd, buffer, &read_size);
   char *to_data_block = (char *)DATABLOCK_OFFSET(block);
   memcpy(to_data_block, buffer,read_size);
   new_inode->i_block[ibo]= block;
   ibo++;
   gd->bg_free_blocks_count--;
   file_len += read_size;  
   read_size -= EXT2_BLOCK_SIZE;

 }while(read_size>0);

  /*insert the inode into img  */
  new_inode->i_size = file_len;

  new_inode->i_blocks = 1+ (file_len-1)/512;
  new_inode->i_links_count = 1;
 
  if(gd->bg_free_inodes_count>0){
    memcpy(free_inode_entry, new_inode, 128);
    gd->bg_free_inodes_count--;
  }else{
    fprintf(stderr, " not enough free inode \n");
    exit(1);
  }
  return 0;
}
