Ramfs:
	gcc -Wall hello.c  `pkg-config fuse --cflags --libs` -o ramdisk

dropbox:
	gcc -fPIC -std=gnu99 example2.c -ldropbox -lmemstream -o example2

clean:
	rm -f example
