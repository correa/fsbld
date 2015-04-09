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
/* Creates a file system image that can be placed at the end of a binary file
   to be uploaded into the FLASH of a LPC1768 based chip such as the mbed
   board.

   Created by Adam Green on July 1, 2011
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "sys/stat.h"
#include <dirent.h>
#include "ffsformat.h"

/* Displays the command line usage to the user. */
static void _DisplayUsage(void)
{
    printf("Usage:   fsbld RootSourceDirectory OutputBinaryFilename\n"
           "  Where: RootSourceDirectory is the name of the directory which\n"
           "           contains the files to be encoded in the output binary\n"
           "           image.\n"
           "         OutputBinaryFilename is the name of the binary file to\n"
           "           contain the resulting file system image.  This file\n"
           "           can be appended to the end of an existing FLASH image\n"
           "           before being deployed to the mbed device.\n");
}

struct stat s; /*include sys/stat.h if necessary */

/* Structure used to hold context for the file system building process. */
typedef struct _SFileSystemBuild
{
    /* Command line parameters */
    const char*         pRootSourceDirectory;
    const char*         pOutputBinaryFilename;
    /* The buffer used to store all of the filenames to be dumped into the
       file system image. */
    char*               pFilenameBuffer;
    /* The array of entries used to describe the files to be placed in the
       file system image. */
    SFileSystemEntry*   pFileEntries;
    /* The size of the pFilenameBuffer */
    unsigned int        FilenameBufferSize;
    /* The number of files to be placed in the file system image. The 
       pFileEntries array will contain this many entries. */
    unsigned int        FileCount;
    /* State tracked as the pFileEntries and pFilenameBuffer is recursiviely
       filled in. */
    SFileSystemEntry*   pCurrEntry;
    char*               pCurrFilename;
    unsigned int        FilesLeft;
    unsigned int        CurrFilenameOffset;
    unsigned int        FilenameStartOffset;
} SFileSystemBuild;



/* Parses the user supplied command line.

    Parameters:
    argc is the number of command line parameters passed on the command line
         including the name of the program itself.
    argv is a pointer to the array of command line parameter strings.
    pFileSystemBuild is a pointer to the structure to be filled in with the
        user specified command line parameters.
    
    Returns:
        0 on success and a negative error code otherwise.
*/
static int _ParseCommandLine(int argc, const char** argv, SFileSystemBuild* pFileSystemBuild)
{
    assert ( argv && pFileSystemBuild );
    
    if (argc < 3)
    {
        fprintf(stderr, "error: Must specify both RootSourceDirectory and OutputBinaryFilename on command line.\n");
        return -1;
    }
    
    pFileSystemBuild->pRootSourceDirectory = argv[1];
    pFileSystemBuild->pOutputBinaryFilename = argv[2];
    
    return 0;
}

/* Global used to record the base address of the file system image so that
   the relative filenames can be found in _CompareFileEntries() */
static const char* g_pFLASHBase;

static int _CompareFileEntries(const void* pvEntry1, const void* pvEntry2)
{
    const SFileSystemEntry*     pEntry1 = (const SFileSystemEntry*)pvEntry1;
    const SFileSystemEntry*     pEntry2 = (const SFileSystemEntry*)pvEntry2;
    const char*                 pEntryName1 = g_pFLASHBase + pEntry1->FilenameOffset;
    const char*                 pEntryName2 = g_pFLASHBase + pEntry2->FilenameOffset;
    
    return strcmp(pEntryName1, pEntryName2);
}


/* Iterates over the files in a directory, counting the number of files each
   contains.  This is a recursive function that it able to find and count
   all files in a directory hierarchy.
   
   Parameters:
    pDirectoryName is the name of the directory on the PC to iterate through.
    ImageDirectoryNameSize is the length of the corresponding directory in the
        destination file system image.
    pFileCount is a pointer to be filled in with the number of files found in
        this directory tree.
    pFilenameSize is a pointer to be filled in with the number of bytes that
        will be needed to contain all of the filenames found in this tree.
        
   Returns:
    0 on success and a positive error code otherwise 
*/
static int _CountFilesInDirectoryTree(const char*   pDirectoryName,
                                      unsigned int  ImageDirectoryNameSize,
                                      unsigned int* pFileCount,
                                      unsigned int* pFilenameSize)
{
    int                 Return = 1;
    unsigned int        DirectoryNameSize = 0;
    unsigned int        TotalFileCount = 0;
    unsigned int        TotalFilenameSize = 0;
    DIR*                pDir = NULL;
    struct dirent*      pDirEntry = NULL;
    
    assert ( pDirectoryName && pFileCount && pFilenameSize );
    
    /* Attempt to open the specified input root directory */
    pDir = opendir(pDirectoryName);
    if (!pDir)
    {
        fprintf(stderr, 
                "error: Failed to open directory %s\n", 
                pDirectoryName);
        goto Error;
    }

    /* Determine the length of this directory name with trailing slash
       separator. */
    DirectoryNameSize = strlen(pDirectoryName);
    
    /* Iterate through the files in the directory and determine size of
       data to be allocated to track these files. */
    while(NULL != (pDirEntry = readdir(pDir)))
    {
        stat(pDirEntry->d_name, &s);
        if (s.st_mode & S_IFDIR)
        {
            /* Skip . and .. directories */
            if (0 != strcmp(pDirEntry->d_name, ".") &&
                0 != strcmp(pDirEntry->d_name, ".."))
            {
                /* Recurse into subdirectories. */
                int          Result = 1;
                unsigned int FileCount = 0;
                unsigned int FilenameSize = 0;
                char         SubdirectoryName[512];
            
                /* Make sure that the complete pathname for this subdirectory will
                   fit in the SubdirectoryName buffer. */
                if (DirectoryNameSize + 1 + pDirEntry->d_namlen > 
                    (sizeof(SubdirectoryName) - 1))
                {
                    fprintf(stderr,
                            "error: %s\%s pathname is too long.\n",
                            pDirectoryName,
                            pDirEntry->d_name);
                    goto Error;
                }
            
                /* Create complete pathname. */
                snprintf(SubdirectoryName, sizeof(SubdirectoryName), 
                         "%s/%s",
                         pDirectoryName,
                         pDirEntry->d_name);
            
                /* Recurse into this directory and gets its counts. */
                Result = _CountFilesInDirectoryTree(SubdirectoryName,
                                                    ImageDirectoryNameSize + 
                                                      pDirEntry->d_namlen + 1,
                                                    &FileCount,
                                                    &FilenameSize);
                if (Result)
                {
                    Return = Result;
                    goto Error;
                }
                TotalFileCount += FileCount;
                TotalFilenameSize += FilenameSize;
            }
        }
        else
        {
            /* Update size statistics based on data for this file. */
            TotalFileCount++;
            TotalFilenameSize += ImageDirectoryNameSize + pDirEntry->d_namlen + 1;
        }
    }
    
    /* Update counters. */
    *pFileCount = TotalFileCount;
    *pFilenameSize = TotalFilenameSize;

    Return = 0;
Error:
    if(pDir)
    {
        closedir(pDir);
        pDir = NULL;
    }

    return Return;
}


/* Recursively iterates over the files in a directory, populating the file 
   entries.
   
   Parameters:
    pFileSystemBuild is a pointer to the structure used both for input and
        output data to/from this procedure.
    pDirectoryName is the name of the directory on the PC to iterate through.
    pImageDirectoryName is the name of the directory in the destination file 
        system image.
        
   Returns:
    0 on success and a positive error code otherwise 
*/
static int _PopulateEntriesFromDirectoryTree(SFileSystemBuild* pFileSystemBuild,
                                             const char*       pDirectoryName,
                                             const char*       pImageDirectoryName)
{
    int                 Return = 1;
    unsigned int        DirectoryNameSize = 0;
    unsigned int        ImageDirectoryNameSize = 0;
    DIR*                pDir = NULL;
    struct dirent*      pDirEntry = NULL;
    
    assert ( pFileSystemBuild && 
             pDirectoryName && 
             pImageDirectoryName );
    
    /* Attempt to open the specified input root directory */
    pDir = opendir(pDirectoryName);
    if (!pDir)
    {
        fprintf(stderr, 
                "error: Failed to open directory %s\n", 
                pDirectoryName);
        goto Error;
    }

    /* Determine the length of this directory name with trailing slash
       separator. */
    DirectoryNameSize = strlen(pDirectoryName);
    ImageDirectoryNameSize = strlen(pImageDirectoryName);

    /* Iterate through the files in the directory and fill in the file system
       entries. */
    while(NULL != (pDirEntry = readdir(pDir)))
    {
        stat(pDirEntry->d_name, &s);
        if (s.st_mode & S_IFDIR)
        {
            /* Skip . and .. directories */
            if (0 != strcmp(pDirEntry->d_name, ".") &&
                0 != strcmp(pDirEntry->d_name, ".."))
            {
                /* Recurse into subdirectories. */
                int          Result = 1;
                char         SubdirectoryName[512];
                char         ImageSubdirectoryName[512];
            
                /* Make sure that the complete pathname for this subdirectory will
                   fit in the buffers. */
                if (DirectoryNameSize + 1 + pDirEntry->d_namlen > 
                    (sizeof(SubdirectoryName) - 1))
                {
                    fprintf(stderr,
                            "error: %s/%s pathname is too long.\n",
                            pDirectoryName,
                            pDirEntry->d_name);
                    goto Error;
                }
                if (ImageDirectoryNameSize + pDirEntry->d_namlen + 1 >
                    sizeof(ImageSubdirectoryName) - 1)
                {
                    fprintf(stderr,
                            "error: %s%s/ pathname is too long.\n",
                            pImageDirectoryName,
                            pDirEntry->d_name);
                    goto Error;
                }
            
                /* Create complete pathnames. */
                snprintf(SubdirectoryName, sizeof(SubdirectoryName), 
                         "%s/%s",
                         pDirectoryName,
                         pDirEntry->d_name);
                snprintf(ImageSubdirectoryName, sizeof(ImageSubdirectoryName), 
                         "%s%s/",
                         pImageDirectoryName,
                         pDirEntry->d_name);
           
                /* Recurse into this directory and gets its counts. */
                Result = _PopulateEntriesFromDirectoryTree(pFileSystemBuild,
                                                           SubdirectoryName,
                                                           ImageSubdirectoryName);
                if (Result)
                {
                    Return = Result;
                    goto Error;
                }
            }
        }
        else
        {
            unsigned int FilenameLength;
        
            /* Make sure that we didn't encounter more files during the second
               iteration. */
            if (0 == pFileSystemBuild->FilesLeft--)
            {
                fprintf(stderr, 
                        "error: File contents of %s appear to have changed while creating file system image.\n", 
                        pFileSystemBuild->pRootSourceDirectory);
                goto Error;
            }
        
            /* Fill in the directory structure for this file.  Can only default 
               the binary start offset and size since we don't know the file size
               yet. */
            pFileSystemBuild->pCurrEntry->FilenameOffset = pFileSystemBuild->FilenameStartOffset + 
                                                           pFileSystemBuild->CurrFilenameOffset;
            pFileSystemBuild->pCurrEntry->FileBinaryOffset = ~0U;
            pFileSystemBuild->pCurrEntry->FileBinarySize = 0;

            /* Make sure that we aren't going to overflow the filename buffer */
            FilenameLength = ImageDirectoryNameSize + pDirEntry->d_namlen + 1; /* Copy NULL terminator as well. */
            if ((pFileSystemBuild->CurrFilenameOffset + FilenameLength) > pFileSystemBuild->FilenameBufferSize)
            {
                fprintf(stderr, 
                        "error: File contents of %s appear to have changed while creating file system image.\n", 
                        pFileSystemBuild->pRootSourceDirectory);
                goto Error;
            }
        
            /* Copy the filename into the filename buffer. */
            memcpy(pFileSystemBuild->pCurrFilename, pImageDirectoryName, ImageDirectoryNameSize);
            memcpy(pFileSystemBuild->pCurrFilename + ImageDirectoryNameSize, pDirEntry->d_name, pDirEntry->d_namlen + 1);

            /* Update the pointers */
            pFileSystemBuild->pCurrFilename += FilenameLength;
            pFileSystemBuild->CurrFilenameOffset += FilenameLength;
            pFileSystemBuild->pCurrEntry++;
        }
    }
    
    Return = 0;
Error:
    if(pDir)
    {
        closedir(pDir);
        pDir = NULL;
    }

    return Return;
}


/* Creates a list of files to be placed in the file syste image based on the
   contents of the user supplied root source directory.  Since this function
   needs to allocate buffers to list out the source files, it needs to know
   suitable sizes for the allocations before populating them, this function
   runs two passes through the directory list: first to calculate the required
   size of the buffers; second fill in the newly allocated buffers.
   
   Parameters:
    pFileSystemBuild is a pointer to the structure used both for input and
        output data to/from this procedure.
        
   Returns:
    0 on success and a positive error code otherwise 
*/
static int _CreateFileList(SFileSystemBuild* pFileSystemBuild)
{
    int                 Return = 1;
    int                 Result = 1;
    unsigned int        TotalFilenameSize = 0;
    unsigned int        TotalFileCount = 0;

    assert ( pFileSystemBuild && pFileSystemBuild->pRootSourceDirectory);

    /* Display information about the enumeration process to be conducted by
       this procedure. */
    printf("Enumerating the contents of the %s directory to be placed in "
           "the file system image...\n",
           pFileSystemBuild->pRootSourceDirectory);
    
    /* Iterate through the files in the directory and determine size of
       data to be allocated to track these files. */
    Result = _CountFilesInDirectoryTree(pFileSystemBuild->pRootSourceDirectory,
                                        0,
                                        &TotalFileCount,
                                        &TotalFilenameSize);
    if (Result)
    {
        Return = Result;
        goto Error;
    }
    pFileSystemBuild->FileCount = TotalFileCount;
    pFileSystemBuild->FilenameBufferSize = TotalFilenameSize;
    
    /* Allocate the buffers to record the file list information. */
    pFileSystemBuild->pFilenameBuffer = malloc(TotalFilenameSize);
    if (!pFileSystemBuild->pFilenameBuffer)
    {
        fprintf(stderr, 
                "error: Failed to allocate %u bytes for filename buffer.\n", 
                TotalFilenameSize);
        goto Error;
    }
    pFileSystemBuild->pFileEntries = malloc(sizeof(pFileSystemBuild->pFileEntries[0]) * 
                                            TotalFileCount);
    if (!pFileSystemBuild->pFileEntries)
    {
        fprintf(stderr, 
                "error: Failed to allocate %u file entry descriptors.\n", 
                TotalFileCount);
        goto Error;
    }
    
    /* Initialize the state used while filling in the file system entries. */
    pFileSystemBuild->CurrFilenameOffset = 0;
    pFileSystemBuild->pCurrEntry = pFileSystemBuild->pFileEntries;
    pFileSystemBuild->pCurrFilename = pFileSystemBuild->pFilenameBuffer;
    pFileSystemBuild->FilesLeft = TotalFileCount;

    /* Calculate the starting relative offset of the filename buffer in 
       the final image. */
    pFileSystemBuild->FilenameStartOffset = sizeof(SFileSystemHeader) + 
                                    TotalFileCount * sizeof(SFileSystemEntry);
                                                      
    /* Iterate through the files in the directory tree again and fill in the
       structures that were just allocated. */
    Result = _PopulateEntriesFromDirectoryTree(pFileSystemBuild,
                                               pFileSystemBuild->pRootSourceDirectory,
                                               "");
    if (Result)
    {
        Return = Result;
        goto Error;
    }

    /* Make sure that we didn't encounter fewer files during the second
       iteration. */
    if (0 != pFileSystemBuild->FilesLeft)
    {
        fprintf(stderr, 
                "error: File contents of %s appear to have changed while creating file system image.\n", 
                pFileSystemBuild->pRootSourceDirectory);
        goto Error;
    }
                                                      
    /* Sort the file entries in case sensitive order. */
    g_pFLASHBase = pFileSystemBuild->pFilenameBuffer - (sizeof(SFileSystemHeader) + 
                                                        pFileSystemBuild->FileCount * 
                                                        sizeof(SFileSystemEntry));
    qsort(pFileSystemBuild->pFileEntries, 
          pFileSystemBuild->FileCount,
          sizeof(pFileSystemBuild->pFileEntries[0]),
          _CompareFileEntries);
          
    Return = 0;
Error:

    return Return;
}


/* Frees up memory allocated in the pointers maintained by the SFileSystemBuild
   structure.
   
   Parameters:
    pFileSystemBuild is a pointer to the structure to be freed.
    
   Returns:
    Nothing.
*/
static void _FreeFileSystemBuild(SFileSystemBuild* pFileSystemBuild)
{
    /* If the pointer is NULL then there is nothing to do */
    if (!pFileSystemBuild)
    {
        return;
    }
    
    if (pFileSystemBuild->pFilenameBuffer)
    {
        free(pFileSystemBuild->pFilenameBuffer);
        pFileSystemBuild->pFilenameBuffer = NULL;
    }
    if (pFileSystemBuild->pFileEntries)
    {
        free(pFileSystemBuild->pFileEntries);
        pFileSystemBuild->pFileEntries = NULL;
    }
}


/* Creates a simple file system image based on the file entries found in the
   caller supplied pFileSystemBuild structure.
   
   Parameters:
    pFileSystemBuild is a pointer to the structure used both for input and
        output data to/from this procedure.
        
   Returns:
    0 on success and a positive error code otherwise */
static int _CreateFileSystemImage(SFileSystemBuild* pFileSystemBuild)
{
    int                 Return = 1;
    int                 Result = 1;
    unsigned int        FileCount = 0;
    FILE*               pFile = NULL;
    FILE*               pSourceFile = NULL;
    long                FileEntryPos = -1;
    long                FilenameBufferPos = -1;
    long                ImageFileSize = -1;
    SFileSystemEntry*   pEntry = NULL;
    unsigned char*      pBuffer = NULL;
    long                BufferSize = 0;
    SFileSystemHeader   Header;
    
    assert ( pFileSystemBuild && 
             pFileSystemBuild->pRootSourceDirectory &&
             pFileSystemBuild->pOutputBinaryFilename &&
             pFileSystemBuild->pFilenameBuffer &&
             pFileSystemBuild->pFileEntries );
    
    /* Output information about the image build process to be started */
    FileCount = pFileSystemBuild->FileCount;
    printf("Creating file system image in %s...\n", 
           pFileSystemBuild->pOutputBinaryFilename);
           
    /* Open the desired file to be populated with the new file system image. */
    pFile = fopen(pFileSystemBuild->pOutputBinaryFilename, "wb");
    if (!pFile)
    {
        fprintf(stderr,
                "Failed to open %s for writing of the file system image.\n",
                pFileSystemBuild->pOutputBinaryFilename);
        goto Error;
    }
    
    /* Write out the file system header */
    printf("    Adding header (%u bytes) to file system image.\n", sizeof(Header));
    memcpy(Header.FileSystemSignature, 
           FILE_SYSTEM_SIGNATURE, 
           sizeof(Header.FileSystemSignature));
    Header.FileCount = pFileSystemBuild->FileCount;
    Result = fwrite(&Header, sizeof(Header), 1, pFile);
    if (Result != 1)
    {
        fprintf(stderr, "error: Failed to write header to file system image.\n");
        goto Error;
    }
    
    /* Write out the partially completed file entries and seek back and
       update with completed entries once the file sizes are known. */
    printf("    Adding file entry descriptors (%u bytes) to file system image.\n",
           sizeof(pFileSystemBuild->pFileEntries[0]) * FileCount);
    FileEntryPos = ftell(pFile);
    if (FileEntryPos != (long)sizeof(Header))
    {
        fprintf(stderr, "error: Failed to write header to file system image.\n");
        goto Error;
    }
    Result = fwrite(pFileSystemBuild->pFileEntries, 
                    sizeof(pFileSystemBuild->pFileEntries[0]) * FileCount,
                    1,
                    pFile);
    if (Result != 1)
    {
        fprintf(stderr, "error: Failed to write file entries to file system image.\n");
        goto Error;
    }
       
    /* Write out the filename buffer */
    printf("    Adding filenames (%u bytes) to file system image.\n",
           pFileSystemBuild->FilenameBufferSize);
    FilenameBufferPos = ftell(pFile);
    if (FilenameBufferPos <= 0)
    {
        fprintf(stderr, "error: Failed to write file entries to file system image.\n");
        goto Error;
    }
    Result = fwrite(pFileSystemBuild->pFilenameBuffer,
                    pFileSystemBuild->FilenameBufferSize, 
                    1, 
                    pFile);
    if (Result != 1)
    {
        fprintf(stderr, "error: Failed to write filename buffer to file system image.\n");
        goto Error;
    }
    
    /* Write out the contents of the files while updating the file entries
       to contain the actual file offset and sizes. */
    printf("    Adding %u entries to file system image.\n", FileCount);
    pEntry = pFileSystemBuild->pFileEntries;
    while (FileCount--)
    {
        char    FilenameBuffer[1024];
        char*   pFilename;
        long    StartPos;
        long    FileSize;
        
        /* Build up path to source file for this entry */
        pFilename = pFileSystemBuild->pFilenameBuffer + 
                    (pEntry->FilenameOffset - (unsigned int)FilenameBufferPos);
        snprintf(FilenameBuffer, sizeof(FilenameBuffer), 
                 "%s\\%s", 
                 pFileSystemBuild->pRootSourceDirectory, 
                 pFilename);
        printf("        %s -> %s ", FilenameBuffer, pFilename);
        
        /* Determine the starting location of this file in the image and
           update file entry with this location. */
        StartPos = ftell(pFile);
        if (StartPos < 0)
        {
            fprintf(stderr, "\nerror: Failed to determine current file location.\n");
            goto Error;
        }
        pEntry->FileBinaryOffset = StartPos;
        
        /* Open the current source file */
        pSourceFile = fopen(FilenameBuffer, "rb");
        if (!pSourceFile)
        {
            fprintf(stderr, "\nerror: Failed to open %s for read.\n", 
                    FilenameBuffer);
            goto Error;
        }
        
        /* Determine the size of the file and update the file entry */
        Result = fseek(pSourceFile, 0, SEEK_END);
        if (Result)
        {
            fprintf(stderr, "\nerror: Failed to determine file size of %s\n",
                    FilenameBuffer);
            goto Error;
        }
        FileSize = ftell(pSourceFile);
        if (FileSize < 0)
        {
            fprintf(stderr, "\nerror: Failed to determine file size of %s\n",
                    FilenameBuffer);
            goto Error;
        }
        Result = fseek(pSourceFile, 0, SEEK_SET);
        if (Result)
        {
            fprintf(stderr, "\nerror: Failed to determine file size of %s\n",
                    FilenameBuffer);
            goto Error;
        }
        pEntry->FileBinarySize = (unsigned int)FileSize;
        printf("(%ld bytes)\n", FileSize);
        
        /* Just skip ahead if we are placing a zero length file in the image. */
        if (FileSize > 0)
        {
            /* Make sure that the read buffer is large enough for this file. */
            if (FileSize > BufferSize)
            {
                unsigned char* pRealloc;
            
                pRealloc = realloc(pBuffer, FileSize);
                if (!pRealloc)
                {
                    fprintf(stderr, 
                            "error: Failed to allocate %ld bytes for read buffer.\n",
                            FileSize);
                    goto Error;
                }
                pBuffer = pRealloc;
                BufferSize = FileSize;
            }
            /* Read the data into the buffer. */
            Result = fread(pBuffer, FileSize, 1, pSourceFile);
            if (Result != 1)
            {
                fprintf(stderr,
                        "error: Failed to read %ld bytes from %s.\n",
                        FileSize,
                        FilenameBuffer);
                goto Error;
            }
        
            /* Write the file data out to the file system image. */
            Result = fwrite(pBuffer, FileSize, 1, pFile);
            if (Result != 1)
            {
                fprintf(stderr,
                        "error: Failed to write %ld bytes to file system image.\n",
                         FileSize);
                goto Error;
            }
        }
        
        pEntry++;
    }
       
    /* Display the final image file size */
    ImageFileSize = ftell(pFile);
    printf("    Total Image Size: %ld bytes\n", ImageFileSize);
    
    /* Seek back to the beginning of the file entries and write out the newly
       updated ones. */
    Result = fseek(pFile, FileEntryPos, SEEK_SET);
    if (Result)
    {
        fprintf(stderr,
                "error: Failed to rewind to file entry location.\n");
        goto Error;
    }
    Result = fwrite(pFileSystemBuild->pFileEntries, 
                    sizeof(pFileSystemBuild->pFileEntries[0]) * pFileSystemBuild->FileCount,
                    1,
                    pFile);
    if (Result != 1)
    {
        fprintf(stderr, "error: Failed to write file entries to file system image.\n");
        goto Error;
    }
    
    Return = 0;
Error:
    free(pBuffer);
    pBuffer = NULL;
    BufferSize = 0;
    if (pSourceFile)
    {
        fclose(pSourceFile);
        pSourceFile = NULL;
    }
    if (pFile)
    {
        fclose(pFile);
        pFile = NULL;
    }
    return Return;
}


int main(int argc, const char** argv)
{
    int                 Return = 1;
    int                 Result = 1;
    SFileSystemBuild    FileSystemBuild;

    /* Initialize all of the pointers in context to NULL */
    memset(&FileSystemBuild, 0, sizeof(FileSystemBuild));

    /* Display header */
    printf("Simple FLASH Binary File System Builder\n"
           "Created by Adam Green in 2011\n\n");
           
    Result = _ParseCommandLine(argc, argv, &FileSystemBuild);
    if (Result)
    {
        _DisplayUsage();
        goto Error;
    }
    
    /* Create list of files to be placed in the file system image by walking
       the source root directory. */
    Result = _CreateFileList(&FileSystemBuild);
    if (Result)
    {
        goto Error;
    }

    /* Create the file system image containing the files just enumerated. */
    Result = _CreateFileSystemImage(&FileSystemBuild);
    if (Result)
    {
        goto Error;
    }
    
    Return = 0;
Error:
    _FreeFileSystemBuild(&FileSystemBuild);
    
    return Return;
}
