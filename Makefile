Ramfs:
	gcc -Wall hello.c  `pkg-config fuse --cflags --libs` -o ramdisk

dropbox:
	gcc -fPIC -std=gnu99 coldDataMove.c -ldropbox -lmemstream -o coldDataMove

clean:
	rm -f coldDataMove
