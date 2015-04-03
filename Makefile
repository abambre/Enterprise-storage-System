Ramfs:
	gcc -Wall -D_FILE_OFFSET_BITS=64 fuse_assignment.c -o fuse_assignment `pkg-config --cflags --libs glib-2.0` `pkg-config fuse --cflags --libs` -pthread -lfuse -lcrypto -lssl -lmemstream -ldropbox -lfuse 

dropbox:
	gcc -fPIC -std=gnu99 coldDataMove.c -ldropbox -lmemstream -o coldDataMove

clean:
	rm -f coldDataMove
	rm -f fuse_assignment
