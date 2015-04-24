#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "ext2.h"
#include <string.h>


unsigned char *disk;

/*Another file descriptor to read disk image again when traversing through 
the i_block array in the inode.
*/
int fdd;

//char array to store the current name of the file if it is a file.
char current_name[EXT2_NAME_LEN];

/* reverse:  reverse string s in place */
void reverse(char s[])
{
    int c, i, j;

    for (i = 0, j = strlen(s)-1; i<j; i++, j--) {
        c = s[i];
        s[i] = s[j];
        s[j] = c;
    }
}

 /* itoa:  convert n to characters in s */
 void itoa(int n, char s[])
 {
     int i, sign;
     
 
     if ((sign = n) < 0)  /* record sign */
         n = -n;          /* make n positive */
     i = 0;
     do {       /* generate digits in reverse order */
         s[i++] = n % 2 + '0';   /* get next digit */
             } while ((n /= 2) > 0);     /* delete it */

     while(i<8){
        s[i++] = '0';
         }

     if (sign < 0)
         s[i++] = '-';
     s[i] = '\0';
 }

//Print contents of each inode.
void print_bb(struct ext2_inode *ip, int node_num){
  
  /*char buffer to store contents of the pointers in the indirect pointer if
  there is one. */ 
  char buf[1024];
  
  //char to hold the type of the inode.
  char t;
  //Check if the inode is a file or directory.
  if (S_ISREG(ip->i_mode))
  {
    t = 'f';
  }else{
    t = 'd';
  }
    //Print Inode contents up until Blocks.
    printf("[%d] type: %c size: %d links: %d blocks: %d\n[%d] Blocks: ",
    node_num, t, ip->i_size,ip->i_links_count, ip->i_blocks, node_num);
    int ib;
    //Traverse through each element in the i_block array.
    for (ib = 0; ib < 15; ib++)
    {
      //Check index element if there is an indirect pointer.
      if (ip->i_block[ib] != 0)
      {
        if (ib == 12)
        {
          //Traverse through the pointers in the indirect pointer.
          if (ib == 12 && ip->i_block[ib] != 0)
          {
            //Offset to block, read contents, print out all of its pointers.
            lseek(fdd, ip->i_block[ib]*1024,SEEK_SET);
            read(fdd, buf, 1024);
            int aa;
            for (aa = 0; aa < 1024; aa++)
            {
              if (buf[aa]!= 0)
              {
                printf("%i ", buf[aa]);
              }
       
            }
          }
        }
      //Print inode contents before 12  
      if (ib < 12)
      {
        printf("%i ",ip->i_block[ib] );  
      }
      }
    }
    printf("\n");
    }

//Print directory contents in the directory inode.
void get_iblock0(struct ext2_inode *ip, int node_num){

  int block_num = ip->i_block[0];
  /* offset the address */
  struct ext2_dir_entry_2 *db = (struct ext2_dir_entry_2 *)(DATABLOCK_OFFSET(block_num));

  //Read up until block size.
  int len=0;
  while(len<1024) {
    //Print the name of the directory entry with size of the rec_len.
    printf("%.*s\n", db->name_len, db->name );
    len +=db->rec_len;
  //Traverse to the next directory entry.  
  char * db_inuse = (char*)db;
  db_inuse += db->rec_len;
  db = (struct ext2_dir_entry_2 *)db_inuse;
  }
}

//Find the inode number of the directory entry.
unsigned get_inode_num(struct ext2_inode *ip, char *name){
  int block_num = ip->i_block[0];

  /* offset the address */
  struct ext2_dir_entry_2 *db = (struct ext2_dir_entry_2 *)(DATABLOCK_OFFSET(block_num));

  //Read upuntil block size.
  int len=0;
  while(len<1024) {

    if(!strncmp(db->name,name, db->name_len)){
      if (GETTFILETYPE(db)== 'f')
      {
        //Clear the buffer
        memset(current_name, 0, 255);
        /*Get the name of the directory entry and store it in the global
        char array for later use. */
        strncpy(current_name, db->name, db->name_len);
        return db->inode;
      }else{
        memset(current_name, 0, 255);
        strncpy(current_name, db->name, db->name_len);
        return db->inode;
      }
    }
    //Offset to the next directory entry.
    len +=db->rec_len;
    char * db_inuse = (char*)db;
    db_inuse += db->rec_len;
    db = (struct ext2_dir_entry_2 *)db_inuse;
  }
  return 0;
}

//Given the absolute path, set the contents of the variable path_token in main.  
void parse_path(char *path, char *path_token[], char *delimiter){
     /* make the dest_token points to parsed path */
   int path_idx = 0;
   //Obtain each entry in between the /
   char *token = strtok(path, delimiter);
     while( token != NULL ) 
   {
      path_token[path_idx]=token;
         token = strtok(NULL, "/");
      path_idx++;
   }
   path_token[path_idx] = token; //set the end of path_token to null;
  }

//Print the block bitmap.
void print_block_bm(unsigned char *bitmap){
   int i;
    printf("Block bitmap:\n");
    for(i=0;i<16; i++){
        char *bbm;
        bbm = malloc(128);
        itoa(bitmap[i], bbm);
    printf("%s ", bbm);
      }
    printf("\n");
}

//Print the inode bitmap.
void print_inode_bm(unsigned char *inode_bm, char *bb){
    //Traverse through each byte.
    int k=0;
    int j;
    for(j=0;j<4; j++){
      //Convert byte and turn it into binary format.
      itoa(inode_bm[j], &bb[k]);
      k+=9;
    }
  /* make a string for the bytes for the convience of printing out */
    int p=0;
    while(p < 36){
      p+= 9;
    }
}

//Print the Root directory information. 
void print_inode_info(struct ext2_inode *ip, char *bb){
   int node_num=0;
    if(bb[1] =='1'){
      node_num = 2;
      ip++;
      print_bb(ip, node_num);
      ip--;
    }
}
//Traverse the path, get the inode.
struct ext2_inode *get_inode(struct ext2_inode *current_root, char * inode_bitmap, 
           struct ext2_inode *ip, char *path_token[]){

   int pt_idx=0;        //path token index
   int inode_number;
    while(path_token[pt_idx]!=NULL){
      if(!(S_ISREG(current_root->i_mode) || S_ISDIR(current_root->i_mode))){
        perror("ENOENT");
        exit(1);
      }

      inode_number = get_inode_num(current_root, path_token[pt_idx]);
      if(inode_number == 0 ){
        perror("ENOENT");
        exit(1);
      }
      if (inode_bitmap[inode_number-1] == '1')
      {
        current_root= ip+ inode_number-1;
      }
      
      pt_idx++;
    }
    return current_root;
}

//Print the superblock contents described in exercise 17.
void print_superblock(struct ext2_super_block *sb){
    printf("Inodes: %d\n", sb->s_inodes_count);
    printf("Blocks: %d\n", sb->s_blocks_count);
}

//Print the block grop descriptor described in exercise 17.
void print_block_group (struct ext2_group_desc *gd){
    printf("block bitmap: %d\n", gd->bg_block_bitmap);
    printf("inode bitmap: %d\n", gd->bg_inode_bitmap);
    printf("Inode table: %d\n", gd->bg_inode_table);
    printf("free blocks: %d\n", gd->bg_free_blocks_count);
    printf("free inodes: %d\n", gd->bg_free_inodes_count);
    printf("used_dirs: %d\n", gd->bg_used_dirs_count);
}

int main(int argc, char **argv) {
    //Check the arguments.
    if(argc != 3) {
        fprintf(stderr, "Usage: ext2_ls <image file name> <path of source> \
      <path of destination\n");
        exit(1);
    }
    //Open the file.
    int fd = open(argv[1], O_RDWR);
    fdd =fd;

    //Allocate space for the image, starter code from ex17.
    disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(disk == MAP_FAILED) {
      perror("mmap");
      exit(1);
    }

    //Allocate space for the block bitmap
    unsigned char *bitmap;
    bitmap = malloc(1024*sizeof(char));    /* allocate memory for the bitmap */
    lseek(fd, 1024 * 3, SEEK_SET);
    read(fd, bitmap, 1024);   /* read bitmap from disk */
    
    //Allocate space for the inode bitmap
    unsigned char *inode_bm;
    inode_bm = malloc(1024*sizeof(char));    /* allocate memory for the bitmap */
    lseek(fd, 1024 * 4, SEEK_SET);
    read(fd, inode_bm, 1024);   /* read bitmap from disk */

    //Make a ext2_inode for the root.
    struct ext2_inode *ip = (struct ext2_inode *)(INODETABLE_OFFSET(disk));

  /* /\* char *bb used to point to the binary form string of the bytes *\/ */
    char *bb;
    bb = malloc(36);

    /* print the inode bitmap */
    print_inode_bm(inode_bm,bb);

    /* make the inode_bitmap in a continguous char array */
    char *inode_bitmap = malloc(sizeof(char) * 32);
    int idx;
    for (idx = 0; idx < 4; idx++) {
      memcpy(inode_bitmap, bb, 8*sizeof(char));
      inode_bitmap+=8;
      bb += 9;
    }
    inode_bitmap-=32;

  /* Parse the source and destination path.*/
    //Absolute path 
    char *path=argv[2];
    /* token the path */
    char *delimiter = "/";
    char *path_token[EXT2_NAME_LEN];
    
    parse_path(path, path_token, delimiter);

    /* find the inode number of the source file and destination directory */
    int inode_number=2;
    struct ext2_inode *current_root;
    current_root = ip+1;
      
    current_root = get_inode(current_root, inode_bitmap, ip,path_token);
    
    //loop variable 
    int inode_idx;
    //Loop through each inode skipping the reserved ones.
    for (inode_idx =11; inode_idx < 31; inode_idx++) {
      //Check if the bit is set.
      if (inode_bitmap[inode_idx] == '1')
      {
        ip += inode_idx;
        ip-=inode_idx;
      }
    }

    //
    if (S_ISREG(current_root->i_mode))
    {
      //print filename
      printf("%s\n",current_name );
    }else{
      //Print directory entries
      get_iblock0(current_root, inode_number);
    }
    
    free(bitmap);
    free(inode_bm);
    return 0;
}


