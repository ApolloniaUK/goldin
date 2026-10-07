/*
Copyright (c) 2006-2014, Stephane Sudre
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

- Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
- Neither the name of the WhiteBox nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY 
AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
SUCH DAMAGE.
*/

/*
            File Name: main.c
              Project: goldin
	  Original Author: S.Sudre
             Creation: 05/27/2006
    Last modification: 10/07/26
    
    +----------+-------------+---------------------------------------------------------+
    |   Date   |    Author   | Comments                                                |
    +----------+-------------+---------------------------------------------------------+
    | 05/27/06 |   S.Sudre   | Version 0.0.1                                           |
    +----------+-------------+---------------------------------------------------------+
    | 05/30/06 |   S.Sudre   | Version 0.0.2                                           |
    |          |             |                                                         |
    |          |             | - We need to close Resource Forks on error and when     |
    |          |             |   empty                                                 |
    |          |             |                                                         |
    |          |             | - Only retrieve the absolute path name when a split is  |
    |          |             |   required => optimization                              |
    |          |             |                                                         |
    |          |             | - Addition of error strings                             |
    |          |             |                                                         |
    +----------+-------------+---------------------------------------------------------+
    | 07/13/06 |   S.Sudre   | Version 0.0.3                                           |
    |          |             |                                                         |
    |          |             | - Fix swap issues with the FinderInfo and ExtFinderInfo |
    +----------+-------------+---------------------------------------------------------+
    | 01/02/07 |   S.Sudre   | Version 0.0.4                                           |
    |          |             |                                                         |
    |          |             | - Properly deal with Symlinks from a FS point of view   |
    +----------+-------------+---------------------------------------------------------+
    | 02/06/07 |   S.Sudre   | Version 0.0.5                                           |
    |          |             |                                                         |
    |          |             | - Properly deal with Symlinks on Intel                  |
    +----------+-------------+---------------------------------------------------------+
    | 02/10/07 |   S.Sudre   | Version 0.0.6                                           |
    |          |             |                                                         |
    |          |             | - Apply permissions, owner and group to the .- file     |
    +----------+-------------+---------------------------------------------------------+
    | 04/22/09 |   S.Sudre   | Version 0.0.7                                           |
    |          |             |                                                         |
    |          |             | - Minor modifications to be 64-bit compatible           |
    +----------+-------------+---------------------------------------------------------+
    | 11/30/14 |   S.Sudre   | - Remove static state                                   |
    |          |             | - Fix text indentation                                  |
    +----------+-------------+---------------------------------------------------------+
    | 10/01/26 | A.Staniforth| Version 0.0.8                                           |
    |          |             |                                                         |
    |          |             | - Accept more than one PATH                             |
    |          |             | - Add long ('--') option flags                          |
    |          |             | - Add -h/--help but keep -u                             |
    |          |             | - Add --norecurse option                                |
    |          |             | - Add -a/--apfs to allow APFS volumes as well           |
    |          |             | - Report errors that previously exited silently         |
    |          |             | - Make error messages clearer and more consistent       |
    |          |             | - Never strip a resource fork that was not copied       |
    |          |             | - Report folders that cannot be read                    |
    |          |             | - Delete a partial AppleDouble file after a write error |
    |          |             | - Name the file in every error message                  |
    |          |             | - Don't close the folder iterator twice                 |
    +----------+-------------+---------------------------------------------------------+
    |          |             |                                                         |
    +----------+-------------+---------------------------------------------------------+

    Notes:

    o "A COMPLETER" means "To be completed"
 
    o To know why this tool is named goldin, see http://en.wikipedia.org/wiki/Sawing_a_woman_in_half
		
    o This tool purpose is to be compatible with SplitForks and FixupResourceForks as best as possible. This explains why we have to create a Entry ID 2 even when it's not needed.
*/

#include <CoreServices/CoreServices.h>

#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <errno.h>

#include <sys/param.h>
#include <sys/mount.h>
#include <sys/stat.h>

#define GOLDIN_VERSION "0.0.8"

long gMaxFileNameLength=0;
Boolean gStripResourceForks=FALSE;
Boolean gVerboseMode=FALSE;
Boolean gNoRecurse=FALSE;
Boolean gAllowAPFS=FALSE;

/*#define DEBUG	1*/

#ifdef DEBUG

#include <syslog.h>
#include <stdarg.h>

#define logerror(...) syslog(LOG_ERR, __VA_ARGS__);

#else

#define logerror(...) (void)fprintf(stderr,__VA_ARGS__)

#endif

#define SWAP_RECT(inRect)   do {												\
								(inRect).top=CFSwapInt16((inRect).top);			\
								(inRect).left=CFSwapInt16((inRect).left);		\
								(inRect).bottom=CFSwapInt16((inRect).bottom);	\
								(inRect).right=CFSwapInt16((inRect).right);		\
							} while (0);
							
#define SWAP_POINT(inPoint)   do {												\
								(inPoint).h=CFSwapInt16((inPoint).h);			\
								(inPoint).v=CFSwapInt16((inPoint).v);			\
							} while (0);


void SplitForks(FSRef * inItemReferencePtr);
void SplitForksChildren(FSRef * inFileReferencePtr,FSRef * inParentReferencePtr);

/* Get the POSIX path of an item for an error message */

static const char * PathForErrorMessage(FSRef * inReferencePtr,UInt8 * outPath,UInt32 inMaxLength)
{
	if (FSRefMakePath(inReferencePtr,outPath,inMaxLength)!=noErr)
		return "a file or directory";
	
	return (const char *) outPath;
}

OSErr SplitFileIfNeeded(FSRef * inFileReference,FSRef * inParentReference,FSCatalogInfo * inFileCatalogInfo,HFSUniStr255 * inFileName,Boolean * outDidSplit)
{
	OSErr tErr;
	Boolean tSplitNeeded=FALSE;
	FSIORefNum tForkRefNum;
	UInt32 tResourceForkSize=0;
	static HFSUniStr255 sResourceForkName={0,{}};
	Boolean tHasResourceFork=FALSE;
	UInt8 tPOSIXPath[PATH_MAX*2+1];
	UInt32 tPOSIXPathMaxLength=PATH_MAX*2;
	struct stat tFileStat;
	FSIORefNum tNewFileRefNum;
	FSRef tNewFileReference;
	
	if (outDidSplit!=NULL)
        *outDidSplit=FALSE;
    
    if (sResourceForkName.length==0)
	{
		tErr=FSGetResourceForkName(&sResourceForkName);
	
		if (tErr!=noErr)
		{
			logerror("An error occurred when obtaining the ResourceFork name\n");
			
			return -1;
		}
	}
	
	/* 1. Check for the presence of a resource fork */
		
	tErr=FSOpenFork(inFileReference,sResourceForkName.length,sResourceForkName.unicode,fsRdPerm,&tForkRefNum);
	
	if (tErr==noErr)
	{
		SInt64 tForkSize;
		
		/* Get the size of the resource fork */
		
		tErr=FSGetForkSize(tForkRefNum,&tForkSize);
		
		if (tErr!=noErr)
		{
			logerror("An error occurred while getting the resource fork size of %s\n",PathForErrorMessage(inFileReference,tPOSIXPath,tPOSIXPathMaxLength));
			
			FSCloseFork(tForkRefNum);
			
			return -1;
		}
		
		if (tForkSize>0xFFFFFFFF)
		{
			FSCloseFork(tForkRefNum);
			
			/* AppleDouble File format does not support forks bigger than 4GB */
			
			logerror("The resource fork of %s is too big for the AppleDouble file format (more than 4 GB)\n",PathForErrorMessage(inFileReference,tPOSIXPath,tPOSIXPathMaxLength));
			
			return -1;
		}
		
		tResourceForkSize=(UInt32) tForkSize;
		
		if (tForkSize>0)
		{
			tHasResourceFork=TRUE;
		
			tSplitNeeded=TRUE;
		}
		else
		{
			FSCloseFork(tForkRefNum);
		}
	}
	else
	{
		switch(tErr)
		{
			case errFSForkNotFound:
			case eofErr:
				/* No resource Fork */
				
				tErr=noErr;
				break;
			default:
				
				logerror("Unable to open the resource fork of %s (%d)\n",PathForErrorMessage(inFileReference,tPOSIXPath,tPOSIXPathMaxLength),tErr);
				
				return -1;
				
				break;
		}
	}
	
	/* 2. Check for the presence of FinderInfo or ExtFinderInfo */
	
	if (tSplitNeeded==FALSE)
	{
		UInt32 * tUnsignedInt32Ptr;
		int i;
		
		
		/* 1. We need to save the Folder(Ext) Info in the ._ file if there are any folder/finder or extend folder/finder info */
			
		tUnsignedInt32Ptr= (UInt32 *) inFileCatalogInfo->finderInfo;
		
		for(i=0;i<4;i++)
		{
			if (tUnsignedInt32Ptr[i]!=0)
			{
				/* We need to create a ._file */
			
				tSplitNeeded=TRUE;
				break;
			}
		}
		
		if (tSplitNeeded==TRUE)		/* 01/02/07: Symbolic link looks like this */
		{
			UInt32 tSymbolicLink;

			tSymbolicLink='s';
			tSymbolicLink='l'+(tSymbolicLink<<8);
			tSymbolicLink='n'+(tSymbolicLink<<8);
			tSymbolicLink='k'+(tSymbolicLink<<8);

			if (tUnsignedInt32Ptr[0]==tSymbolicLink)
			{
				tSplitNeeded=FALSE;
			}
		}
		else
		{
			tUnsignedInt32Ptr= (UInt32 *) inFileCatalogInfo->extFinderInfo;
		
			for(i=0;i<4;i++)
			{
				if (tUnsignedInt32Ptr[i]!=0)
				{
					/* We need to create a ._file */
				
					tSplitNeeded=TRUE;
					break;
				}
			}
		}
	}
	
	/* 3. Split if needed */
	
	if (tSplitNeeded==TRUE)
	{
		HFSUniStr255 tNewFileName;
	
		/* Get the absolute Posix Path Name */
		
		tErr=FSRefMakePath(inFileReference,tPOSIXPath,tPOSIXPathMaxLength);
		
		if (tErr==noErr)
		{
			if (lstat((char *) tPOSIXPath,&tFileStat)==-1)
			{
				logerror("Unable to get information about %s (%s)\n",tPOSIXPath,strerror(errno));
				
				tErr=-1;
				
				goto byebye;
			}
		}
		else
		{
			logerror("An error occurred when trying to get the absolute path of a file or directory\n");
			
			tErr=-1;
				
			goto byebye;
		}
		
		if (gVerboseMode==TRUE)
		{
			printf("    splitting %s...\n",tPOSIXPath);
		}
		
		/* Check that we do not explode the current limit for file names */
		
		if (inFileName->length>gMaxFileNameLength)
		{
			/* We do not have enough space to add the ._ prefix */
		
			/* The file name is too long */
					
			/* Write the error */
			
			logerror("The name of %s is too long to add the ._ prefix (the maximum is %ld characters)\n",tPOSIXPath,gMaxFileNameLength);
			
			return -1;
		}
		
		tNewFileName.length=inFileName->length+2;
		
		tNewFileName.unicode[0]='.';
		tNewFileName.unicode[1]='_';
		
		memcpy(tNewFileName.unicode+2,inFileName->unicode,inFileName->length*sizeof(UniChar));
		
		/* We need to create a ._file */
		
tryagain:

		tErr=FSCreateFileUnicode(inParentReference,tNewFileName.length,tNewFileName.unicode,0,NULL,&tNewFileReference,NULL);
		
		if (tErr!=noErr)
		{
			switch(tErr)
			{
				case bdNamErr:
				case fsmBadFFSNameErr:
				case errFSNameTooLong:
					/* The file name is too long */
					
					/* Write the error */
					
					logerror("The name of %s is too long to add the ._ prefix (the maximum is %ld characters)\n",tPOSIXPath,gMaxFileNameLength);
					
					break;
				case dskFulErr:
					
					logerror("Unable to create the AppleDouble file of %s (the disk is full)\n",tPOSIXPath);
					
					break;
					
				case errFSQuotaExceeded:
					
					logerror("Unable to create the AppleDouble file of %s (your disk quota has been exceeded)\n",tPOSIXPath);
					
					break;
				case dupFNErr:
				
					/* The file already exists, we need to try to delete it before recreating it */
					
					tErr=FSMakeFSRefUnicode(inParentReference,tNewFileName.length,tNewFileName.unicode,kTextEncodingDefaultFormat,&tNewFileReference);
					
					if (tErr==noErr)
					{
						/* Delete the current ._file */
						
						tErr=FSDeleteObject(&tNewFileReference);
						
						if (tErr==noErr)
						{
							goto tryagain;
						}
					}
					
					logerror("Unable to replace the existing AppleDouble file of %s (%d)\n",tPOSIXPath,tErr);
					
					break;
				
				case afpVolLocked:
					
					logerror("Unable to create the AppleDouble file of %s (the volume is locked)\n",tPOSIXPath);
					
					break;
				
				case afpAccessDenied:
					
					logerror("Unable to create the AppleDouble file of %s (permission denied)\n",tPOSIXPath);
					
					break;
					
				default:
					
					logerror("Unable to create the AppleDouble file of %s (%d)\n",tPOSIXPath,tErr);
					
					break;
			}
			
			return -1;
		}
		
		tErr=FSOpenFork(&tNewFileReference,0,NULL,fsWrPerm,&tNewFileRefNum);
		
		if (tErr==noErr)
		{
			unsigned char tAppleDoubleMagicNumber[4]=  {0x00,0x05,0x16,0x07};
			unsigned char tAppleDoubleVersionNumber[4]={0x00,0x02,0x00,0x00};
			unsigned char tAppleDoubleFiller[16]={0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
			ByteCount tRequestCount;
			UInt16 tNumberOfEntries;
			UInt16 tSwappedNumberOfEntries;

			UInt32 tEntryID;
			UInt32 tEntryOffset;
			UInt32 tEntryLength;
			
			/* Write the Magic Number */
			
			tRequestCount=4;
			
			tErr=FSWriteFork(tNewFileRefNum,fsAtMark,0,tRequestCount,tAppleDoubleMagicNumber,NULL);
			
			if (tErr!=noErr)
			{
				goto writebail;
			}
			
			/* Write the Version Number */
			
			tRequestCount=4;
			
			tErr=FSWriteFork(tNewFileRefNum,fsAtMark,0,tRequestCount,tAppleDoubleVersionNumber,NULL);
			
			if (tErr!=noErr)
			{
				goto writebail;
			}
			
			/* Write the Filler */
			
			tRequestCount=16;
			
			tErr=FSWriteFork(tNewFileRefNum,fsAtMark,0,tRequestCount,tAppleDoubleFiller,NULL);
			
			if (tErr!=noErr)
			{
				goto writebail;
			}
			
			/* Compute the Number of Entries */
			
			tNumberOfEntries=0x0002;
			
			tSwappedNumberOfEntries=tNumberOfEntries;
			
#ifdef __LITTLE_ENDIAN__
			
			/* Swap for Intel processor */
			
			tSwappedNumberOfEntries=CFSwapInt16(tSwappedNumberOfEntries);

#endif			
			tRequestCount=2;
			
			tErr=FSWriteFork(tNewFileRefNum,fsAtMark,0,tRequestCount,&tSwappedNumberOfEntries,NULL);
			
			if (tErr!=noErr)
			{
				goto writebail;
			}
			
			/* Write the Entries Descriptor */
			
			/* **** Finder Info */
			
			tEntryID=0x00000009;		/* Finder Info ID */
			
			tEntryOffset=0x0000001A+tNumberOfEntries*12;
			
			tEntryLength=0x00000020;	/* 32 bytes */
			
#ifdef __LITTLE_ENDIAN__
			
			/* Swap for Intel processor */
			
			tEntryID=CFSwapInt32(tEntryID);
			
			tEntryOffset=CFSwapInt32(tEntryOffset);
			
			tEntryLength=CFSwapInt32(tEntryLength);
			
#endif			
			tRequestCount=4;
			
			tErr=FSWriteFork(tNewFileRefNum,fsAtMark,0,tRequestCount,&tEntryID,NULL);
			
			if (tErr!=noErr)
			{
				goto writebail;
			}
			
			tErr=FSWriteFork(tNewFileRefNum,fsAtMark,0,tRequestCount,&tEntryOffset,NULL);
			
			if (tErr!=noErr)
			{
				goto writebail;
			}
			
			tErr=FSWriteFork(tNewFileRefNum,fsAtMark,0,tRequestCount,&tEntryLength,NULL);
			
			if (tErr!=noErr)
			{
				goto writebail;
			}
			
			tEntryID=0x00000002;		/* Resource Fork ID */
				
			tEntryOffset=0x00000052;
				
			if (tHasResourceFork==TRUE)
			{
				/* **** Finder Info */
			
				tEntryLength=tResourceForkSize;	/* As you can see the AppleDouble format file is not ready for forks bigger than 4 GB */
			}
			else
			{
				tEntryLength=0;
			}
			
#ifdef __LITTLE_ENDIAN__
				
			/* Swap for Intel processor */
		
			tEntryID=CFSwapInt32(tEntryID);
			
			tEntryOffset=CFSwapInt32(tEntryOffset);
			
			tEntryLength=CFSwapInt32(tEntryLength);
#endif

			tRequestCount=4;
			
			tErr=FSWriteFork(tNewFileRefNum,fsAtMark,0,tRequestCount,&tEntryID,NULL);
			
			if (tErr!=noErr)
			{
				goto writebail;
			}
			
			tErr=FSWriteFork(tNewFileRefNum,fsAtMark,0,tRequestCount,&tEntryOffset,NULL);
			
			if (tErr!=noErr)
			{
				goto writebail;
			}
		
			tErr=FSWriteFork(tNewFileRefNum,fsAtMark,0,tRequestCount,&tEntryLength,NULL);
		
			if (tErr!=noErr)
			{
				goto writebail;
			}
			
			/* Write the Entries */
			
			/* **** Write Finder Info */
			
#ifdef __LITTLE_ENDIAN__

			/* Intel Processors */
			
			/* Even though it's referenced as a bytes field in the File API, this is actually a structure we need to swap... */

			if (inFileCatalogInfo->nodeFlags & kFSNodeIsDirectoryMask)
			{
				/* It's a fragging folder */
			
				FolderInfo * tFolderInfoStruct;
				ExtendedFolderInfo * tExtendedFolderInfoStruct;
				
				/* Swap FolderInfo Structure */
				
				tFolderInfoStruct=(FolderInfo *) inFileCatalogInfo->finderInfo;
				
				SWAP_RECT(tFolderInfoStruct->windowBounds);
				tFolderInfoStruct->finderFlags=CFSwapInt16(tFolderInfoStruct->finderFlags);
				SWAP_POINT(tFolderInfoStruct->location);
				tFolderInfoStruct->reservedField=CFSwapInt16(tFolderInfoStruct->reservedField);
				
				/* Swap ExtendedFolderInfo Info Structure */
				
				tExtendedFolderInfoStruct=(ExtendedFolderInfo *) inFileCatalogInfo->extFinderInfo;
				
				SWAP_POINT(tExtendedFolderInfoStruct->scrollPosition);
				tExtendedFolderInfoStruct->reserved1=CFSwapInt32(tExtendedFolderInfoStruct->reserved1);
				tExtendedFolderInfoStruct->extendedFinderFlags=CFSwapInt16(tExtendedFolderInfoStruct->extendedFinderFlags);
				tExtendedFolderInfoStruct->reserved2=CFSwapInt16(tExtendedFolderInfoStruct->reserved2);
				tExtendedFolderInfoStruct->putAwayFolderID=CFSwapInt32(tExtendedFolderInfoStruct->putAwayFolderID);
			}
			else
			{
				/* I'm just a file, you know */
				
				FileInfo * tFileInfoStruct;
				ExtendedFileInfo * tExtendedFileInfoStruct;
				
				/* Swap FileInfo Structure */
				
				tFileInfoStruct=(FileInfo *) inFileCatalogInfo->finderInfo;
				
				tFileInfoStruct->fileType=CFSwapInt32(tFileInfoStruct->fileType);
				tFileInfoStruct->fileCreator=CFSwapInt32(tFileInfoStruct->fileCreator);
				tFileInfoStruct->finderFlags=CFSwapInt16(tFileInfoStruct->finderFlags);
				SWAP_POINT(tFileInfoStruct->location);
				tFileInfoStruct->reservedField=CFSwapInt16(tFileInfoStruct->reservedField);
				
				/* Swap ExtendedFileInfo Structure */
				
				tExtendedFileInfoStruct=(ExtendedFileInfo *) inFileCatalogInfo->extFinderInfo;
				
				tExtendedFileInfoStruct->reserved1[0]=CFSwapInt16(tExtendedFileInfoStruct->reserved1[0]);
				tExtendedFileInfoStruct->reserved1[1]=CFSwapInt16(tExtendedFileInfoStruct->reserved1[1]);
				tExtendedFileInfoStruct->reserved1[2]=CFSwapInt16(tExtendedFileInfoStruct->reserved1[2]);
				tExtendedFileInfoStruct->reserved1[3]=CFSwapInt16(tExtendedFileInfoStruct->reserved1[3]);
				tExtendedFileInfoStruct->extendedFinderFlags=CFSwapInt16(tExtendedFileInfoStruct->extendedFinderFlags);
				tExtendedFileInfoStruct->reserved2=CFSwapInt16(tExtendedFileInfoStruct->reserved2);
				tExtendedFileInfoStruct->putAwayFolderID=CFSwapInt32(tExtendedFileInfoStruct->putAwayFolderID);
			}

#endif
			
			tRequestCount=16;
				
			tErr=FSWriteFork(tNewFileRefNum,fsAtMark,0,tRequestCount,inFileCatalogInfo->finderInfo,NULL);
				
			if (tErr!=noErr)
			{
				goto writebail;
			}
				
			tErr=FSWriteFork(tNewFileRefNum,fsAtMark,0,tRequestCount,inFileCatalogInfo->extFinderInfo,NULL);
				
			if (tErr!=noErr)
			{
				goto writebail;
			}
			
			/* **** Write Resource Fork? */
			
			if (tHasResourceFork==TRUE)
			{
				/* We need to be clever and copy the Resource Fork by chunks to avoid using too much memory */
				
				static UInt8 * tBuffer=NULL;
				static ByteCount tReadRequestCount=0;
				ByteCount tReadActualCount;
				OSErr tReadErr;

#define GOLDIN_BUFFER_ONE_MEGABYTE_SIZE		1048576
				
				if (tBuffer==NULL)
				{
					tReadRequestCount=GOLDIN_BUFFER_ONE_MEGABYTE_SIZE;
					
					do
					{
						tBuffer=(UInt8 *) malloc(tReadRequestCount*sizeof(UInt8));
					
						tReadRequestCount/=2;
					}
					while (tBuffer==NULL && tReadRequestCount>1);
					
					if (tBuffer!=NULL && tReadRequestCount>1)
					{
						tReadRequestCount*=2;
					}
					else
					{
						/* A COMPLETER */
					}
				}
				
				do
				{
					tReadErr=FSReadFork(tForkRefNum, fsAtMark,0, tReadRequestCount, tBuffer, &tReadActualCount);
					
					if (tReadErr==noErr || tReadErr==eofErr)
					{
						tErr=FSWriteFork(tNewFileRefNum,fsAtMark,0,tReadActualCount,tBuffer,NULL);
						
						if (tErr!=noErr)
						{
							break;
						}
					}
					else
					{
						break;
					}
				
				}
				while (tReadErr!=eofErr);
				
				if (tReadErr!=eofErr)
				{
					/* A problem occurred while reading the Resource Fork */
					
					goto writebail;
				}
				else
				if (tErr!=noErr)
				{
					/* A problem occurred while writing the Resource Fork Data to the AppleDouble file */
					
					goto writebail;
				}
			}
		
			tErr=FSCloseFork(tNewFileRefNum);
			
			if (tErr==noErr)
			{
                if (outDidSplit!=NULL)
                    *outDidSplit=TRUE;
                
                // A COMPLETER
            }
            
            /* Copy the permissions (the owner and group are only copied when run as root) */
			
			tErr=FSSetCatalogInfo(&tNewFileReference,kFSCatInfoPermissions,inFileCatalogInfo);
			
			if (tErr!=noErr)
			{
				logerror("Permissions, owner and group could not be set for the AppleDouble file of %s\n",tPOSIXPath);
				
				tErr=-1;
				
				goto byebye;
			}
		}
		else
		{
			/* Report the error and stop: carrying on would strip (with --strip) a resource fork that was never copied */
			
			logerror("Unable to open the AppleDouble file of %s for writing (%d)\n",tPOSIXPath,tErr);
			
			FSDeleteObject(&tNewFileReference);
			
			tErr=-1;
			
			goto byebye;
		}
		
		/* Close the Resource Fork if needed */
		
		if (tHasResourceFork==TRUE)
		{
			tErr=FSCloseFork(tForkRefNum);
		
			if (gStripResourceForks==TRUE && tErr==noErr)
			{
				/* Strip the resource fork */
				
				tErr=FSDeleteFork(inFileReference,sResourceForkName.length,sResourceForkName.unicode);
				
				if (tErr!=noErr)
				{
					switch(tErr)
					{
						case errFSForkNotFound:
							/* This is not important */
							tErr=noErr;
							break;
						default:
							logerror("Resource fork could not be stripped from %s (%d)\n",tPOSIXPath,tErr);
							break;
					}
				}
			}
			else
			{
				if (gStripResourceForks==TRUE && tErr!=noErr)
				{
					logerror("Resource fork could not be stripped from %s\n",tPOSIXPath);
				
					/* A COMPLETER */
				}
			}
		}
	}
	
	return tErr;
	
writebail:

	switch(tErr)
	{
		case dskFulErr:
			logerror("Unable to write the AppleDouble file of %s (the disk is full)\n",tPOSIXPath);
			break;
		case errFSQuotaExceeded:
			logerror("Unable to write the AppleDouble file of %s (your disk quota has been exceeded)\n",tPOSIXPath);
			break;
		default:
			logerror("An unknown error occurred while writing the AppleDouble file of %s (%d)\n",tPOSIXPath,tErr);
			break;
	}
	
	FSCloseFork(tNewFileRefNum);
	
	/* Don't leave a partial AppleDouble file behind */
	
	FSDeleteObject(&tNewFileReference);
	
byebye:

	if (tHasResourceFork==TRUE)
	{
		FSCloseFork(tForkRefNum);
	}
	
	return tErr;
}

static void LogCatalogInfoError(FSRef * inReferencePtr,OSErr inErr)
{
	UInt8 tPath[PATH_MAX];
	
	if (FSRefMakePath(inReferencePtr,tPath,sizeof(tPath))==noErr)
		logerror("An error occurred while getting the catalog information of %s (%d)\n",(char *) tPath,inErr);
	else
		logerror("An error occurred while getting the catalog information of a file or directory (%d)\n",inErr);
}

static void LogFolderReadError(FSRef * inReferencePtr,OSErr inErr)
{
	UInt8 tPath[PATH_MAX];
	const char * tName="a directory";
	
	if (FSRefMakePath(inReferencePtr,tPath,sizeof(tPath))==noErr)
		tName=(const char *) tPath;
	
	if (inErr==afpAccessDenied)
		logerror("Unable to read the contents of %s (permission denied)\n",tName);
	else
		logerror("Unable to read the contents of %s (%d)\n",tName,inErr);
}

void SplitForks(FSRef * inItemReferencePtr)
{
	FSCatalogInfo tInfo;
    HFSUniStr255 tUnicodeFileName;
    FSRef tParentReference;
    
    /* We need to split forks of the first level (and it allows us to check whether it's a folder or not) */
    
    OSErr tErr=FSGetCatalogInfo(inItemReferencePtr,kFSCatInfoFinderInfo+kFSCatInfoFinderXInfo+kFSCatInfoPermissions+kFSCatInfoNodeFlags,&tInfo,&tUnicodeFileName,NULL,&tParentReference);
    
    if (tErr==noErr)
    {
        /* Check this is not a Hard Link */
        
        if ((tInfo.nodeFlags & kFSNodeHardLinkMask)==0)
        {
            tErr=SplitFileIfNeeded(inItemReferencePtr,&tParentReference,&tInfo,&tUnicodeFileName,NULL);
            
            if (tErr==noErr)
            {
                if (tInfo.nodeFlags & kFSNodeIsDirectoryMask)
                {
                    /* It's a folder */
                    
                    /* We need to proceed with the contents of the folder */
                    
                    SplitForksChildren(inItemReferencePtr,&tParentReference);
                }
            }
            else
            {
                exit(-1);
            }
        }
    }
    else
    {
        LogCatalogInfoError(inItemReferencePtr,tErr);
        
        exit(-1);
    }
}

void SplitForksChildren(FSRef * inFileReferencePtr,FSRef * inParentReferencePtr)
{
	FSIterator tIterator;
    
	OSErr tErr=FSOpenIterator(inFileReferencePtr,kFSIterateFlat,&tIterator);
	
	if (tErr==noErr)
	{
		Boolean tIteratorIsOpen=TRUE;
		
		FSRef * tFoundReferences=(FSRef *) malloc(sizeof(FSRef));
		
		do
		{
			ItemCount tFoundItems;
            
            tErr=FSGetCatalogInfoBulk(tIterator, 1, &tFoundItems,NULL, 0, NULL,tFoundReferences, NULL, NULL);
			
			if (tErr==noErr)
			{
                FSCatalogInfo tInfo;
                HFSUniStr255 tUnicodeFileName;
                
                /* Retrieve the CatalogInfo with FSGetCatalogInfo because FSGetCatalogInfoBulk is buggy on Yosemite */
                
                tErr=FSGetCatalogInfo(&tFoundReferences[0],kFSCatInfoFinderInfo+kFSCatInfoFinderXInfo+kFSCatInfoPermissions+kFSCatInfoNodeFlags,&tInfo,&tUnicodeFileName,NULL,NULL);
                
                if (tErr==noErr)
                {
                    /* Check this is not a Hard Link */
                    
                    if ((tInfo.nodeFlags & kFSNodeHardLinkMask)==0)
                    {
                        Boolean tDidSplit=FALSE;
                        
                        tErr=SplitFileIfNeeded(&tFoundReferences[0],inFileReferencePtr,&tInfo,&tUnicodeFileName,&tDidSplit);
                            
                        if (tErr==noErr)
                        {
                            /* With --norecurse, folders inside the path are split but not descended into */

                            if ((tInfo.nodeFlags & kFSNodeIsDirectoryMask) && gNoRecurse==FALSE)
                            {
                                /* 2. We need to proceed with the contents of the folder */
                            
                                SplitForksChildren(&tFoundReferences[0],inFileReferencePtr);
                            }
                            
                            /* 3. Check whether the filesystem item was split (i.e. the valence of the folder changed) */
                            
                            if (tDidSplit==TRUE)
                            {
                                FSCloseIterator(tIterator);
                                
                                tIteratorIsOpen=FALSE;
                                
                                tErr=FSOpenIterator(inFileReferencePtr,kFSIterateFlat,&tIterator);
                                
                                if (tErr==noErr)
                                {
                                    tIteratorIsOpen=TRUE;
                                    
                                    FSRef tPreviousCacheRef=tFoundReferences[0];
                                    
                                    do
                                    {
                                        tErr=FSGetCatalogInfoBulk(tIterator, 1, &tFoundItems,NULL, 0, NULL,tFoundReferences, NULL, NULL);
                                        
                                        if (tErr==noErr)
                                        {
                                            OSErr tCompareErr=FSCompareFSRefs(&tPreviousCacheRef,&tFoundReferences[0]);
                                            
                                            if (tCompareErr==noErr)
                                            {
                                                break;
                                            }
                                        }
                                    }
                                    while (tErr==noErr);
                                }
                            }
                        }
                        else
                        {
                            exit(-1);
                        }
                    }
                }
                else
                {
                    LogCatalogInfoError(&tFoundReferences[0],tErr);
                    
                    exit(-1);
                }
			}
		}
		while (tErr==noErr);
		
		free(tFoundReferences);
		
		if (tIteratorIsOpen==TRUE)
			FSCloseIterator(tIterator);
	}
	
	if (tErr!=noErr)
	{
		switch(tErr)
		{
			case errFSNoMoreItems:
				/* No more items in the folder, this is perfectly ok */
				break;
			default:
				/* Report the folder and carry on with the rest of the tree */
				
				LogFolderReadError(inFileReferencePtr,tErr);
				break;
		}
	}
}

static void usage(const char * inProcessName)
{
    printf("usage: %s [options] PATH ...\n", inProcessName);
    printf("\n"\
           "Options:\n"\
           "  --help, -h, -u       Show this usage guide\n"\
           "  --verbose, -v        Verbose mode\n"\
           "  --strip, -s          Strip resource fork from source after splitting\n"\
           "  --apfs, -a           Allow APFS volumes as well as HFS and HFS+\n"\
           "  --norecurse          Do not descend into subdirectories\n"\
           "  --version            Show the version of this tool\n"\
           "\n");
}

static int SplitPath(const char * inPath)
{
	char tResolvedPath[PATH_MAX];

	if (realpath(inPath,tResolvedPath)!=NULL)
	{
		struct statfs tStatFileSystem;

		if (statfs(tResolvedPath,&tStatFileSystem)!=0)
		{
			switch(errno)
			{
				case ENOENT:
					/* No such file or directory */
					
					logerror("\"%s\" was not found\n",tResolvedPath);
					break;
				default:
					logerror("Unable to get file system information for \"%s\" (%s)\n",inPath,strerror(errno));
					break;
			}

			return -1;
		}

		/* HFS, HFS+ and APFS store resource forks and Finder information natively. On other
		   file systems macOS keeps them in its own ._ files, which the AppleDouble files
		   goldin writes would collide with, so those are always refused. APFS is only
		   accepted with --apfs, so that goldin otherwise behaves as SplitForks does. */

		Boolean tIsHFS=(strcmp(tStatFileSystem.f_fstypename,"hfs")==0);
		Boolean tIsAPFS=(strcmp(tStatFileSystem.f_fstypename,"apfs")==0);

		if (tIsHFS==FALSE && (tIsAPFS==FALSE || gAllowAPFS==FALSE))
		{
			/* Return (-2) if this is not a supported file system */

			if (tIsAPFS==TRUE)
				logerror("\"%s\" is on an APFS volume; use -a or --apfs to allow APFS\n",inPath);
			else if (gAllowAPFS==TRUE)
				logerror("\"%s\" is not on an HFS, HFS+ or APFS volume\n",inPath);
			else
				logerror("\"%s\" is not on an hfs disk\n",inPath);

			return 254;
		}

		gMaxFileNameLength=pathconf(tResolvedPath,_PC_NAME_MAX);

		if (gMaxFileNameLength<0)
		{
			logerror("An error occurred while getting the maximum file name length for \"%s\" (%s)\n",inPath,strerror(errno));

			return -1;
		}

		gMaxFileNameLength-=2;

		FSRef tFileReference;
		OSStatus tErr=FSPathMakeRef((const UInt8 *) tResolvedPath,&tFileReference,NULL);

		if (tErr!=noErr)
		{
			logerror("An error occurred while getting the File System Reference of %s\n",inPath);

			return -1;
		}

		if (gVerboseMode==TRUE)
			printf("Splitting %s...\n",inPath);

		SplitForks(&tFileReference);
	}
	else
	{
		switch(errno)
		{
			case ENOENT:
				/* No such file or directory */

				logerror("\"%s\" was not found\n",inPath);
				break;
			
			default:
				logerror("Unable to resolve the path \"%s\" (%s)\n",inPath,strerror(errno));
				break;
		}

		return -1;
	}

	return 0;
}

int main (int argc, const char * argv[])
{
    int c;
    int tIndex;

    while (1)
    {
        static struct option tLongOptions[] =
        {
            {"help",                        no_argument,        NULL,    'h'},
            {"verbose",                     no_argument,        NULL,    'v'},
            {"strip",                       no_argument,        NULL,    's'},
            {"apfs",                        no_argument,        NULL,    'a'},
            {"norecurse",                   no_argument,        NULL,    0},
            {"version",                     no_argument,        NULL,    0},

            {0, 0, 0, 0}
        };

        int tOptionIndex = 0;

        c = getopt_long (argc, (char **) argv, "ahsuv",tLongOptions, &tOptionIndex);

        /* Detect the end of the options. */
        if (c == -1)
            break;

        switch (c)
        {
            case 0:

                if (strncmp(tLongOptions[tOptionIndex].name,"version",strlen("version"))==0)
                {
                    printf("%s\n",GOLDIN_VERSION);

                    exit(0);
                }
                else if (strncmp(tLongOptions[tOptionIndex].name,"norecurse",strlen("norecurse"))==0)
                {
                    gNoRecurse=TRUE;
                }

                break;

            case 's':

                gStripResourceForks=TRUE;

                break;

            case 'a':

                gAllowAPFS=TRUE;

                break;

            case 'v':

                gVerboseMode=TRUE;

                break;

			case 'h':
			case 'u':		/* -u is kept for backwards compatibility */
				usage(argv[0]);
				exit(0);

			default:
				usage(argv[0]);
				exit(2);

                break;
        }
    }

	argv+=optind;
    argc-=optind;

    if (argc==0)
    {
        logerror("No file or directory was specified\n");

		return -1;
    }

	/* Process each path in turn, stopping at the first one that fails */

	for (tIndex=0;tIndex<argc;tIndex++)
	{
		int tStatus=SplitPath(argv[tIndex]);

		if (tStatus!=0)
			return tStatus;
	}

    return 0;
}
