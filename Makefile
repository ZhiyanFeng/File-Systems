all: ext2_ls ext2_cp ext2_mkdir ext2_ln ext2_rm

ext2_ls: ext2_ls.c ext2.h
	gcc -Wall -g -o ext2_ls ext2_ls.c 

ext2_cp: ext2_cp.c ext2.h
	gcc -Wall -g -o ext2_cp ext2_cp.c 	

ext2_mkdir: ext2_mkdir.c ext2.h
	gcc -Wall -g -o ext2_mkdir ext2_mkdir.c 

ext2_ln: ext2_ln.c ext2.h
	gcc -Wall -g -o ext2_ln ext2_ln.c

ext2_rm: ext2_rm.c ext2.h		 
	gcc -Wall -g -o ext2_rm ext2_rm.c
clean: 
	rm ext2_ls ext2_cp ext2_mkdir ext2_ln ext2_rm