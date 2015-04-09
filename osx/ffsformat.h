/* Copyright 2011 Adam Green (http://mbed.org/users/AdamGreen/)

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/
/* Specifies constants and structures used within the FLASH File System.  The
   header is used by both the runtime and the tool which builds the image on
   the PC.
*/
#ifndef _FFSFORMAT_H_
#define _FFSFORMAT_H_


/* The signature to be placed in SFileSystemHeader::FileSystemSignature.
   Only the first 8 bytes are used and the NULL terminator discarded. */
#define FILE_SYSTEM_SIGNATURE "FFileSys"

/* The size of the FLASH on the device to search through for the file
   system signature. */
#define FILE_SYSTEM_FLASH_SIZE  (512 * 1024)


/* Header stored at the beginning of the file system image. */
typedef struct _SFileSystemHeader
{
    /* Signature should be set to FILE_SYSTEM_SIGNATURE. */
    char            FileSystemSignature[8];
    /* Number of entries in this file system image. */
    unsigned int    FileCount;
    /* The SFileSystemEntry[SFileSystemHeader::FileCount] array will start here. 
       These entries are to be sorted so that a binary search can be performed
       at file open time. */
} SFileSystemHeader;

/* Information about each file added to the file system image. */
typedef struct _SFileSystemEntry
{
    /* The 2 following offsets are relative to the beginning of the file 
       image. */
    unsigned int    FilenameOffset;
    unsigned int    FileBinaryOffset;
    unsigned int    FileBinarySize;
} SFileSystemEntry;


#endif /* _FFSFORMAT_H_ */
