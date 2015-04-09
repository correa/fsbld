Installing Mingw-w64
====================
Pick the SourceForge download at http://mingw-w64.sourceforge.net/download.php
As described in http://www.mingw.org/wiki/Getting_Started, set the preferred installation target directory to C:\mingw-w64


Compiling fsbld with MinGW
==========================
Open a DOS prompt using the 'Run Terminal' option from the 'MinGW-W64 project' folder in the start menu.
This ensures all environment variables are set for MinGW.
Goto the folder where the fsbld source is stored and enter following command:
	gcc -O3 -std=c99 -Wall fsbld-win.c -o fsbld


There are two options to add the system image binary to your project
====================================================================
1. Append the file system image binary to the compiler binary
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


2. Import the header file into the compiler
-------------------------------------------
A C-style header file (.h) is created from the binary file.
You can import this file into the compiler and use #include in your main file to add it to your project.
There is no need to read the array in this header file as FlasFileSystem will automatically use the contents
of this array (no copy of the array is made, FlashFileSystem reads directly from the flash memory).


Using FlashFileSystem with other mcu's
======================================
By default, the FlashFileSystem is setup to be used with a device having 512KB flash memory.
If you want to use it with other devices, do not forget to change the memory size when you
instantiate the FlashFileSystem.

For instance, for the FRDM-KL25Z board, use
    static FlashFileSystem flash("flash", roFlashDrive, 128)'  // Header file included
    -- OR --
    static FlashFileSystem flash("flash", NULL, 128)'          // Binary file appended

