Ramfs:
	gcc -Wall fuse_assignment.c `pkg-config fuse --cflags --libs`-pthread -o ramdisk

dropbox:
	gcc -fPIC -std=gnu99 coldDataMove.c -ldropbox -lmemstream -o coldDataMove

clean:
	rm -f coldDataMove ramdisk
