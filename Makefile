all: dropbox file_tracking fuse
	gcc -o ufs dropbox.o file_tracking.o fuse_assignment.o 

dropbox:
	gcc -D_FILE_OFFSET_BITS=64 dropbox.c -o dropbox.o -c `pkg-config --cflags --libs glib-2.0` `pkg-config fuse --cflags --libs` -lfuse -lcrypto -lssl -lmemstream -ldropbox -lfuse

file_tracking:
	gcc -D_FILE_OFFSET_BITS=64 file_tracking.c -o file_tracking.o -c `pkg-config --cflags --libs glib-2.0` `pkg-config fuse --cflags --libs` -lpthread -lfuse -lcrypto -lssl -lmemstream -ldropbox -lfuse

fuse:
	gcc -D_FILE_OFFSET_BITS=64 -DFUSE_USE_VERSION=26 fuse_assignment.c -o fuse_assignment.o -c `pkg-config --cflags --libs glib-2.0` `pkg-config fuse --cflags --libs` -lpthread -lfuse -lcrypto -lssl -lmemstream -ldropbox

clean:
	rm -f fuse_assignment

