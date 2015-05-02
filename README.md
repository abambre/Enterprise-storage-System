## Enterprise-storage-System

### Components
This Project Contains three components 
   - Primary File System - using FUSE
   - File tracking Layer
   - Block Data Accessing Layer.

### Dependencies

This Project uses FUSE for FS operations and Dropbox as cold storage
   
* FUSE - Install fuse as described in the below webpage:      
   http://fuse.sourceforge.net/
* Dropbox - Install dropbox dependicies and make dropbox
   * Install the dependencies described here:   
      https://github.com/Dwii/Dropbox-C/blob/master/README.md
      *   CURL library
      *   OAuth library
      *   Jansson library
   * Clone the Dropbox-C library from here - https://github.com/Dwii/Dropbox-C
   * Run "make all" in the folder where dropbox library is extracted.
* Install glibc and open-ssl library packages on Linux

### Execution Instructions
Instructions to run Unified File System Project (Our Project)

   1. Extract the downloaded project source package.
   2. Change directory to Enterprise-storage-System
   3. Run make
      ``` make unifiedFS```
   4. Create a mount point folder - Ex: mkdir /tmp/fuse  
   5. Run the Virtual File System : shell> ./unifiedFS <mount-point> <File system size in MB>
      Ex: ./unifiedFS /tmp/fuse 512
   6. cd to the mount-point and perform general File operations.
   7. See the various scenarious when the file system reaches Maximum (70%) or minimum (30%) threshold.
   8. Track and see the blocks on Cold storage using the below credentials for Dropbox

      **username:** esa.coldstorage@gmail.com
      **password:** coldstorage
    
      

    
