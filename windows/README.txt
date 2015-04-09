Windows build contributed by Frank Vannieuwkerke (http://developer.mbed.org/users/frankvnk/)

Installing Mingw-w64
--------------------
Pick the SourceForge download at http://mingw-w64.sourceforge.net/download.php
As described in http://www.mingw.org/wiki/Getting_Started, set the preferred installation target directory to C:\mingw-w64


Compiling fsbld with MinGW
--------------------------
Open a DOS prompt using the 'Run Terminal' option from the 'MinGW-W64 project' folder in the start menu.
This ensures all environment variables are set for MinGW.
Goto the folder where the fsbld source is stored and enter following command:
	gcc -O3 -std=c99 -Wall fsbld-win.c -o fsbld


Appending the file system image binary to the compiler binary
-------------------------------------------------------------
Use binary copy : copy /b <mbed file> <file system image> <output file.bin>

Example:
- d:\fsbld contains the fsbld.exe
- In this folder, create a folder named 'flash' and copy the files you want to add to the system image binary here.
  (We add 2 files : data.txt and index.html)
     fsbld --- flash -+- data.txt
                      +- index.html

- Make sure you get back to the fsbld folder and create the system image binary:
     fsbld flash flash.bin
- also save your mbed compiled code into the fsbld folder and use following command to join both binaries:
  (Let's assume your mbed file is 'KL25Z.bin')
     copy /b KL25Z.bin flash.bin KL25Zfl.bin
- Finally, copy the new binary to your mbed device:
  (Let's assume your mbed device uses drive letter G)
     copy KL25Zfl.bin G:


Using FlashFileSystem with other mcu's
--------------------------------------
By default, the FlashFileSystem is setup to be used with a device having 512KB flash memory.
If you want to use it with other devices, do not forget to change the memory size in ffsformat.h
Modify this line (replace 512 with the flash size of your device):
  #define FILE_SYSTEM_FLASH_SIZE  (512 * 1024)

For instance, use '#define FILE_SYSTEM_FLASH_SIZE  (128 * 1024)' for the FRDM-KL25Z board.