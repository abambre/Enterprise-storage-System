# Enterprise-storage-System

This Project Contains three components - 
1. Primary File System - using FUSE
2. File tracking Layer
3. Block Data Accessing Layer.

Dependencies
This Project uses FUSE for FS operations and Dropbox as cold storage
1. FUSE - Install fuse as described in the below webpage
   http://fuse.sourceforge.net/
2. Dropbox - Install dropbox dependicies and make dropbox
    a) Install the dependencies described here - https://github.com/Dwii/Dropbox-C/blob/master/README.md
       cURL library
       OAuth library
       Jansson library
    b) Install glibc and open-ssl library packages on Linux
    c) Clone the Dropbox-C library from here - https://github.com/Dwii/Dropbox-C
    d) run "make all" in the folder where dropbox library is extracted.
    
Instructions to run Unified File System Project (Our Project)
1. Extract the downloaded project source package.
2. cd to Enterprise-storage-System
3. run make
4. create a mount point folder - Ex: mkdir /tmp/fuse
5. Run the Virtual File System : shell> ./unifiedFS <mount-point> <File system size in MB>
      Ex: ./unifiedFS /tmp/fuse 512
6. cd to the mount-point and perform general File operations.
7. See the various scenarious when the file system reaches Maximum (70%) or minimum (30%) threshold.
8. Track and see the blocks on Cold storage using the below credentials for Dropbox
    username: esa.coldstorage@gmail.com
    password: coldstorage
    
      

    
