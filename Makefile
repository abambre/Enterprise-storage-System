Ramfs:
<<<<<<< HEAD
	gcc -Wall -D_FILE_OFFSET_BITS=64 fuse_assignment.c -o fuse_assignment `pkg-config --cflags --libs glib-2.0` `pkg-config fuse --cflags --libs` -lfuse -lcrypto -lssl -lmemstream -ldropbox -lfuse 
||||||| merged common ancestors
	gcc -Wall hello.c  `pkg-config fuse --cflags --libs` -o ramdisk
=======
	gcc -Wall fuse_assignment.c `pkg-config fuse --cflags --libs`-pthread -o ramdisk
>>>>>>> b639d1db691f36ea0f80ae1553b42c07955df87f

dropbox:
	gcc -fPIC -std=gnu99 coldDataMove.c -ldropbox -lmemstream -o coldDataMove

clean:
<<<<<<< HEAD
	rm -f coldDataMove
	rm -f fuse_assignment
||||||| merged common ancestors
	rm -f coldDataMove
=======
	rm -f coldDataMove ramdisk
>>>>>>> b639d1db691f36ea0f80ae1553b42c07955df87f
