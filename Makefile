Ramfs:
	gcc -Wall -g -D_FILE_OFFSET_BITS=64 fuse_assignment.c -o fuse_assignment `pkg-config --cflags --libs glib-2.0` `pkg-config fuse --cflags --libs` -lpthread -lfuse -lcrypto -lssl -lmemstream -ldropbox -lfuse 

clean:
	rm -f fuse_assignment
