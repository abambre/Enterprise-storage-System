#include <stdio.h>
#include <stdlib.h>
#include "dll.h"

struct mdata_Node {
	char fname[1000];
	long last_access;
}mdata_Node;




int main() {

        int res,i;
		struct dList list;
        printf("Inside main\n");
        char fnames[4] = {"foo", "cat", "limp", "bat"};

        for (i=0; i<4; i++)
        {
                res = populate_metaData_list(fnames[i], i+10);
                if(res  < 0) {
                        printf("Could not insert into metaData list\n");
                        exit(0);
                }
        }
        printf("All file names inserted succesfully\n");
        printf("Files are :\n");
        print_metaData_list();
        return 0;
}
	
