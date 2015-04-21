unifiedFS: dropbox file_tracking
	gcc -w -g -D_FILE_OFFSET_BITS=64 -DFUSE_USE_VERSION=26 unifiedFS.c -o unifiedFS dropbox_storage.o file_tracking.o  `pkg-config --cflags --libs glib-2.0` `pkg-config fuse --cflags --libs` -lpthread -lfuse -lcrypto -lssl -lmemstream -ldropbox -lfuse

dropbox:
	gcc -w -g -D_FILE_OFFSET_BITS=64 dropbox_storage.c -o dropbox_storage.o -c  `pkg-config --cflags --libs glib-2.0` `pkg-config fuse --cflags --libs` -lpthread -lfuse -lcrypto -lssl -lmemstream -ldropbox -lfuse

file_tracking:
	gcc -w -g -D_FILE_OFFSET_BITS=64 file_tracking.c -o file_tracking.o -c  `pkg-config --cflags --libs glib-2.0` `pkg-config fuse --cflags --libs` -lpthread -lfuse -lcrypto -lssl -lmemstream -ldropbox -lfuse

clean:
	rm -f unifiedFS dropbox_storage.o file_tracking.o
