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
    | 10/07/26 | A.Staniforth| Version 0.1.0                                           |
    |          |             |                                                         |
    |          |             | - Replace the deprecated File Manager (FSRef) calls     |
    |          |             |   with getattrlist, getxattr, fts and POSIX file I/O    |
    |          |             | - No longer use CoreServices                            |
    |          |             | - Write the Finder Info as read: it is already in the   |
    |          |             |   big-endian layout of the AppleDouble file             |
    |          |             | - Report a full disk as "the disk is full"              |
    |          |             | - Report resource fork read errors as such              |
    |          |             | - Use a static copy buffer (no failed allocation)       |
    |          |             | - Skip volumes mounted inside the folder tree           |
    |          |             | - Add -l/--linksplit: split hard links (never stripped) |
    |          |             | - Add -w/--warnsymlink: report items split through a    |
    |          |             |   symbolic link                                         |
    |          |             | - Add -d/--dryrun: list what would be split, and any    |
    |          |             |   problems, without writing anything                    |
    |          |             | - Report a volume root that needs splitting clearly     |
    |          |             | - Fix the DEBUG build (logerror macro)                  |
    +----------+-------------+---------------------------------------------------------+
    |          |             |                                                         |
    +----------+-------------+---------------------------------------------------------+

    Notes:

    o To know why this tool is named goldin, see http://en.wikipedia.org/wiki/Sawing_a_woman_in_half
		
    o This tool purpose is to be compatible with SplitForks and FixupResourceForks as best as possible. This explains why we have to create a Entry ID 2 even when it's not needed.

    o The AppleDouble file written for an item (all numbers are big-endian):

        0x00  Magic Number          0x00051607
        0x04  Version Number        0x00020000
        0x08  Filler                16 bytes of 0
        0x18  Number of Entries     2
        0x1A  Entry Descriptor      Finder Info: ID 9, offset 0x32, length 32
        0x26  Entry Descriptor      Resource Fork: ID 2, offset 0x52, length = size of the resource fork (0 if none)
        0x32  Finder Info           16 bytes of Finder Info, then 16 bytes of Extended Finder Info
        0x52  Resource Fork
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <fts.h>

#include <sys/param.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/attr.h>
#include <sys/xattr.h>

#include <libkern/OSByteOrder.h>

#define GOLDIN_VERSION "0.1.0"

long gMaxFileNameLength=0;
bool gStripResourceForks=false;
bool gVerboseMode=false;
bool gNoRecurse=false;
bool gAllowAPFS=false;
bool gSplitHardLinks=false;
bool gWarnSymbolicLinks=false;
bool gDryRun=false;

/* With --warnsymlink: the first symbolic link in the PATH being processed, as it was given ("" if there is none) */

char gSymbolicLink[PATH_MAX]="";

/*#define DEBUG	1*/

#ifdef DEBUG

#include <syslog.h>
#include <stdarg.h>

#define logerror(...) syslog(LOG_ERR, __VA_ARGS__)

#else

#define logerror(...) (void)fprintf(stderr,__VA_ARGS__)

#endif

#define GOLDIN_FINDER_INFO_SIZE				32

#define GOLDIN_NUMBER_OF_ENTRIES			2

#define GOLDIN_HEADER_SIZE					(0x0000001A+GOLDIN_NUMBER_OF_ENTRIES*12)

#define GOLDIN_BUFFER_ONE_MEGABYTE_SIZE		1048576

/* Get the Finder Info and the Extended Finder Info of an item and, for a file, the size of its resource fork */

static int GetCatalogInfo(const char * inPath,bool inIsFile,uint8_t * outFinderInfo,off_t * outResourceForkSize)
{
	struct attrlist tAttributeList;
	struct
	{
		uint32_t length;
		uint8_t finderInfo[GOLDIN_FINDER_INFO_SIZE];
		off_t resourceForkSize;
	} __attribute__((aligned(4), packed)) tAttributeBuffer;

	/* getattrlist, unlike getxattr, doesn't need permission to read the item (FSGetCatalogInfo didn't either), so a folder
	   or a file without a resource fork is still split when it can't be read. The Finder Info comes in the big-endian
	   layout of the AppleDouble file */

	memset(&tAttributeList,0,sizeof(tAttributeList));
	memset(&tAttributeBuffer,0,sizeof(tAttributeBuffer));

	tAttributeList.bitmapcount=ATTR_BIT_MAP_COUNT;
	tAttributeList.commonattr=ATTR_CMN_FNDRINFO;

	/* Only files have a resource fork */

	if (inIsFile==true)
		tAttributeList.fileattr=ATTR_FILE_RSRCLENGTH;

	if (getattrlist(inPath,&tAttributeList,&tAttributeBuffer,sizeof(tAttributeBuffer),FSOPT_NOFOLLOW)!=0)
		return -1;

	memcpy(outFinderInfo,tAttributeBuffer.finderInfo,GOLDIN_FINDER_INFO_SIZE);

	*outResourceForkSize=tAttributeBuffer.resourceForkSize;

	return 0;
}

/* Write a whole buffer to a file */

static int WriteBuffer(int inFileDescriptor,const void * inBuffer,size_t inLength)
{
	const uint8_t * tBytes=(const uint8_t *) inBuffer;

	while (inLength>0)
	{
		ssize_t tWrittenCount=write(inFileDescriptor,tBytes,inLength);

		if (tWrittenCount<0)
		{
			if (errno==EINTR)
				continue;
			
			return -1;
		}

		tBytes+=tWrittenCount;
		inLength-=(size_t) tWrittenCount;
	}

	return 0;
}

/* --dryrun: what would happen to each item, listed once every PATH has been checked */

typedef struct
{
	char * path;
	bool process;			/* The item would be split */
	const char * reason;	/* Why it would be ignored */
	char * warnings;		/* Its "WARNING: " lines */
} DryRunEntry;

typedef struct
{
	dev_t device;
	long long freeSize;
	long long neededSize;
	long long blockSize;
} DryRunVolume;

#define GOLDIN_DRY_RUN_MAX_VOLUMES			32

static DryRunEntry * gDryRunEntries=NULL;
static size_t gDryRunEntriesCount=0;
static size_t gDryRunEntriesCapacity=0;
static bool gDryRunFoundProblems=false;
static DryRunVolume gDryRunVolumes[GOLDIN_DRY_RUN_MAX_VOLUMES];
static size_t gDryRunVolumesCount=0;

static void * DryRunAllocate(void * inPointer,size_t inSize)
{
	void * tPointer=realloc(inPointer,inSize);

	if (tPointer==NULL)
	{
		logerror("There is not enough memory for the dry run\n");

		exit(-1);
	}

	return tPointer;
}

static void DryRunAddItem(const char * inPath,bool inProcess,const char * inReason)
{
	DryRunEntry * tEntry;

	if (gDryRunEntriesCount==gDryRunEntriesCapacity)
	{
		gDryRunEntriesCapacity=(gDryRunEntriesCapacity==0) ? 256 : gDryRunEntriesCapacity*2;

		gDryRunEntries=DryRunAllocate(gDryRunEntries,gDryRunEntriesCapacity*sizeof(DryRunEntry));
	}

	tEntry=&gDryRunEntries[gDryRunEntriesCount++];

	tEntry->path=DryRunAllocate(NULL,strlen(inPath)+1);
	strcpy(tEntry->path,inPath);

	tEntry->process=inProcess;
	tEntry->reason=inReason;
	tEntry->warnings=NULL;
}

/* The latest entry for an item (a folder is met again once its contents have been read) */

static DryRunEntry * DryRunFindItem(const char * inPath)
{
	size_t i=gDryRunEntriesCount;

	while (i>0)
	{
		i--;

		if (strcmp(gDryRunEntries[i].path,inPath)==0)
			return &gDryRunEntries[i];
	}

	DryRunAddItem(inPath,false,NULL);

	return &gDryRunEntries[gDryRunEntriesCount-1];
}

static void DryRunProcessItem(const char * inPath)
{
	DryRunFindItem(inPath)->process=true;
}

static void DryRunWarn(const char * inPath,const char * inFormat,...) __attribute__((format(printf,2,3)));

static void DryRunWarn(const char * inPath,const char * inFormat,...)
{
	static const char sPrefix[]="    WARNING: ";
	DryRunEntry * tEntry=DryRunFindItem(inPath);
	size_t tOldLength=(tEntry->warnings==NULL) ? 0 : strlen(tEntry->warnings);
	va_list tArguments;
	int tLength;

	va_start(tArguments,inFormat);
	tLength=vsnprintf(NULL,0,inFormat,tArguments);
	va_end(tArguments);

	if (tLength<0)
		return;

	tEntry->warnings=DryRunAllocate(tEntry->warnings,tOldLength+sizeof(sPrefix)+(size_t) tLength+1);

	memcpy(tEntry->warnings+tOldLength,sPrefix,sizeof(sPrefix)-1);

	va_start(tArguments,inFormat);
	vsnprintf(tEntry->warnings+tOldLength+sizeof(sPrefix)-1,(size_t) tLength+1,inFormat,tArguments);
	va_end(tArguments);

	strcat(tEntry->warnings+tOldLength,"\n");

	gDryRunFoundProblems=true;
}

/* The length of a name as the file system counts it. HFS+ and APFS both allow 255 UTF-16 units, counted in the form
   the name is stored in (decomposed on HFS+), which is the form it is read back in */

static size_t DryRunNameLength(const char * inName)
{
	const unsigned char * tByte;
	size_t tLength=0;

	for (tByte=(const unsigned char *) inName;*tByte!='\0';tByte++)
	{
		/* Count the first byte of each character; those outside the BMP need 2 UTF-16 units */

		if ((*tByte & 0xC0)!=0x80)
			tLength+=(*tByte>=0xF0) ? 2 : 1;
	}

	return tLength;
}

/* Add an AppleDouble file to the space its volume needs, and check that it still fits */

static bool DryRunSpaceFits(const char * inFolder,dev_t inDevice,long long inSize,long long inReplacedSize,long long * outNeededSize,long long * outFreeSize)
{
	DryRunVolume * tVolume=NULL;
	size_t i;

	for (i=0;i<gDryRunVolumesCount;i++)
	{
		if (gDryRunVolumes[i].device==inDevice)
			tVolume=&gDryRunVolumes[i];
	}

	if (tVolume==NULL)
	{
		struct statfs tStatFileSystem;

		if (gDryRunVolumesCount==GOLDIN_DRY_RUN_MAX_VOLUMES || statfs(inFolder,&tStatFileSystem)!=0 || tStatFileSystem.f_bsize==0)
			return true;		/* We can't tell */

		tVolume=&gDryRunVolumes[gDryRunVolumesCount++];

		tVolume->device=inDevice;
		tVolume->freeSize=(long long) tStatFileSystem.f_bavail*(long long) tStatFileSystem.f_bsize;
		tVolume->neededSize=0;
		tVolume->blockSize=(long long) tStatFileSystem.f_bsize;
	}

	/* A file takes whole allocation blocks, and the space of a ._ file it replaces is reused */

	tVolume->neededSize+=((inSize+tVolume->blockSize-1)/tVolume->blockSize)*tVolume->blockSize-inReplacedSize;

	*outNeededSize=tVolume->neededSize;
	*outFreeSize=tVolume->freeSize;

	return (tVolume->neededSize<=tVolume->freeSize);
}

/* Check, without writing anything, that the AppleDouble file of an item could be written */

static void DryRunCheckAppleDoubleFile(const char * inPath,const struct stat * inFileStat,const char * inFileName,const char * inAppleDoublePath,uint32_t inResourceForkSize)
{
	char tFolder[PATH_MAX];
	struct stat tFolderStat;
	struct stat tAppleDoubleStat;
	bool tHasFolderStat;
	long long tReplacedSize=0;
	long long tNeededSize;
	long long tFreeSize;

	/* With --warnsymlink, report items reached through a symbolic link */

	if (gWarnSymbolicLinks==true && gSymbolicLink[0]!='\0')
		DryRunWarn(inPath,"it is reached through the symbolic link \"%s\"",gSymbolicLink);

	/* The name must leave room for the ._ prefix */

	if ((long) DryRunNameLength(inFileName)>gMaxFileNameLength)
		DryRunWarn(inPath,"its name is too long to add the ._ prefix (the maximum is %ld characters); goldin stops here",gMaxFileNameLength);

	/* The AppleDouble file is created in the item's folder */

	snprintf(tFolder,sizeof(tFolder),"%.*s",(int) (inFileName-inPath),inPath);

	tHasFolderStat=(lstat(tFolder,&tFolderStat)==0);

	if (tHasFolderStat==true && (tFolderStat.st_flags & (UF_IMMUTABLE|SF_IMMUTABLE))!=0)
	{
		DryRunWarn(inPath,"its AppleDouble file can't be created (the folder is locked); goldin stops here");
	}
	else if (access(tFolder,W_OK|X_OK)!=0)
	{
		switch(errno)
		{
			case EACCES:
				DryRunWarn(inPath,"its AppleDouble file can't be created (permission denied); goldin stops here");
				break;
			case EROFS:
				DryRunWarn(inPath,"its AppleDouble file can't be created (the volume is locked); goldin stops here");
				break;
			default:
				DryRunWarn(inPath,"its AppleDouble file can't be created (%s); goldin stops here",strerror(errno));
				break;
		}
	}

	/* An existing ._ file is replaced */

	if (lstat(inAppleDoublePath,&tAppleDoubleStat)==0)
	{
		if (S_ISDIR(tAppleDoubleStat.st_mode))
			DryRunWarn(inPath,"a folder named ._%s is in the way of its AppleDouble file; goldin stops here",inFileName);
		else if ((tAppleDoubleStat.st_flags & (UF_IMMUTABLE|SF_IMMUTABLE))!=0)
			DryRunWarn(inPath,"its existing AppleDouble file is locked, so it can't be replaced; goldin stops here");
		else if (tHasFolderStat==true && (tFolderStat.st_mode & S_ISVTX)!=0 && geteuid()!=0 && tAppleDoubleStat.st_uid!=geteuid() && tFolderStat.st_uid!=geteuid())
			DryRunWarn(inPath,"its existing AppleDouble file belongs to another user in a sticky folder, so it can't be replaced; goldin stops here");
		else
			tReplacedSize=(long long) tAppleDoubleStat.st_blocks*512;
	}

	/* Check there is room for it */

	if (DryRunSpaceFits(tFolder,inFileStat->st_dev,(long long) GOLDIN_HEADER_SIZE+GOLDIN_FINDER_INFO_SIZE+inResourceForkSize,tReplacedSize,&tNeededSize,&tFreeSize)==false)
		DryRunWarn(inPath,"there may not be enough free space for its AppleDouble file (%lld KB needed on the volume so far, %lld KB free); goldin would stop with \"the disk is full\"",tNeededSize/1024,tFreeSize/1024);
}

static bool DryRunIsListed(const DryRunEntry * inEntry)
{
	return (gVerboseMode==true || inEntry->process==true || inEntry->warnings!=NULL);
}

/* List what would be done, with the paths shown from where the path common to all of them ends */

static int DryRunList(void)
{
	const char * tCommonPath=NULL;
	size_t tCommonLength=0;
	size_t i;

	for (i=0;i<gDryRunEntriesCount;i++)
	{
		const char * tPath=gDryRunEntries[i].path;
		size_t tLimit=strlen(tPath);
		size_t k=0;

		if (DryRunIsListed(&gDryRunEntries[i])==false || tLimit==0)
			continue;

		if (tCommonPath==NULL)
		{
			tCommonPath=tPath;
			tCommonLength=tLimit;
		}

		/* The common path ends with a '/', and every path goes on after it */

		while (k<tCommonLength && k<tLimit-1 && tPath[k]==tCommonPath[k])
			k++;

		while (k>0 && tCommonPath[k-1]!='/')
			k--;

		tCommonLength=k;
	}

	if (tCommonPath==NULL)
	{
		printf("Dry run, nothing written. Nothing would be split.\n");

		return 0;
	}

	if (tCommonLength>0)
		printf("Dry run, nothing written. Paths are relative to %.*s\n",(int) tCommonLength,tCommonPath);
	else
		printf("Dry run, nothing written.\n");

	for (i=0;i<gDryRunEntriesCount;i++)
	{
		const DryRunEntry * tEntry=&gDryRunEntries[i];

		if (DryRunIsListed(tEntry)==false || strlen(tEntry->path)==0)
			continue;

		if (gVerboseMode==true)
		{
			printf("%s%s",(tEntry->process==true) ? "Process: " : "Ignored: ",tEntry->path+tCommonLength);

			if (tEntry->process==false && tEntry->reason!=NULL)
				printf(" (%s)",tEntry->reason);

			printf("\n");
		}
		else
		{
			printf("%s\n",tEntry->path+tCommonLength);
		}

		if (tEntry->warnings!=NULL)
			fputs(tEntry->warnings,stdout);
	}

	return (gDryRunFoundProblems==true) ? 1 : 0;
}

/* --warnsymlink: find the first symbolic link in a path as it was given */

static void FindSymbolicLink(const char * inPath,char * outSymbolicLink,size_t inMaxLength)
{
	size_t tLength=strlen(inPath);
	size_t i;

	outSymbolicLink[0]='\0';

	if (tLength>=inMaxLength)
		return;

	for (i=1;i<=tLength;i++)
	{
		if (i==tLength || inPath[i]=='/')
		{
			struct stat tStat;

			memcpy(outSymbolicLink,inPath,i);
			outSymbolicLink[i]='\0';

			if (lstat(outSymbolicLink,&tStat)==0 && S_ISLNK(tStat.st_mode))
				return;
		}
	}

	outSymbolicLink[0]='\0';
}

int SplitFileIfNeeded(const char * inPath,const struct stat * inFileStat)
{
	bool tSplitNeeded=false;
	bool tIsHardLink=(S_ISREG(inFileStat->st_mode) && inFileStat->st_nlink>1);
	bool tStripped=false;
	off_t tForkSize;
	uint32_t tResourceForkSize=0;
	uint8_t tFinderInfo[GOLDIN_FINDER_INFO_SIZE];
	uint8_t tHeader[GOLDIN_HEADER_SIZE+GOLDIN_FINDER_INFO_SIZE];
	const char * tFileName;
	char tAppleDoublePath[PATH_MAX];
	struct statfs tStatFileSystem;
	int tNewFileDescriptor;
	int tErrno;
	int i;

	if (gDryRun==true)
		DryRunAddItem(inPath,false,"nothing to split");

	if (GetCatalogInfo(inPath,S_ISREG(inFileStat->st_mode),tFinderInfo,&tForkSize)!=0)
	{
		tErrno=errno;

		if (gDryRun==true)
		{
			DryRunProcessItem(inPath);
			DryRunWarn(inPath,"its catalog information can't be read (%s); goldin stops here",strerror(tErrno));

			return 0;
		}

		logerror("An error occurred while getting the catalog information of %s (%s)\n",inPath,strerror(tErrno));

		return -1;
	}
	
	/* 1. Check for the presence of a resource fork */
		
	if (tForkSize>0)
	{
		/* Get the size of the resource fork (this also checks that it can be read) */

		ssize_t tReadableForkSize=getxattr(inPath,XATTR_RESOURCEFORK_NAME,NULL,0,0,XATTR_NOFOLLOW);

		if (tReadableForkSize<0)
		{
			switch(errno)
			{
				case ENOATTR:
					/* No resource Fork (any more) */

					tReadableForkSize=0;

					break;
				default:

					if (gDryRun==true)
					{
						/* goldin would stop here, but check the rest of the split */

						DryRunWarn(inPath,"its resource fork can't be read (%s); goldin stops here",strerror(errno));

						tReadableForkSize=(ssize_t) tForkSize;

						break;
					}

					logerror("Unable to read the resource fork of %s (%s)\n",inPath,strerror(errno));

					return -1;
			}
		}
		else if (gDryRun==true && tReadableForkSize>0)
		{
			/* A trial read */

			uint8_t tByte;

			if (getxattr(inPath,XATTR_RESOURCEFORK_NAME,&tByte,1,0,XATTR_NOFOLLOW)<0)
				DryRunWarn(inPath,"its resource fork can't be read (%s); goldin stops here",strerror(errno));
		}

		if ((uint64_t) tReadableForkSize>0xFFFFFFFF)
		{
			/* AppleDouble File format does not support forks bigger than 4GB */

			if (gDryRun==true)
			{
				DryRunProcessItem(inPath);
				DryRunWarn(inPath,"its resource fork is too big for the AppleDouble file format (more than 4 GB); goldin stops here");

				return 0;
			}

			logerror("The resource fork of %s is too big for the AppleDouble file format (more than 4 GB)\n",inPath);
			
			return -1;
		}
		
		tResourceForkSize=(uint32_t) tReadableForkSize;

		if (tResourceForkSize>0)
			tSplitNeeded=true;
	}

	/* 2. Check for the presence of FinderInfo or ExtFinderInfo */

	if (tSplitNeeded==false)
	{
		/* We need to save the Folder(Ext) Info in the ._ file if there are any folder/finder or extend folder/finder info */

		for(i=0;i<GOLDIN_FINDER_INFO_SIZE;i++)
		{
			if (tFinderInfo[i]!=0)
			{
				/* We need to create a ._file */

				tSplitNeeded=true;
				break;
			}
		}
	}

	if (tSplitNeeded==false)
		return 0;

	/* 3. Split */

	if (gDryRun==true)
	{
		DryRunProcessItem(inPath);
	}
	else if (gVerboseMode==true)
	{
		printf("    splitting %s...\n",inPath);
	}

	/* The ._ file goes in the parent folder, which isn't on the same volume as the root of a volume */

	if (statfs(inPath,&tStatFileSystem)==0 && strcmp(tStatFileSystem.f_mntonname,inPath)==0)
	{
		if (gDryRun==true)
		{
			DryRunWarn(inPath,"it is the root of a volume, so its AppleDouble file can't be created; goldin stops here");

			return 0;
		}

		logerror("Unable to create the AppleDouble file of %s (it is the root of a volume)\n",inPath);

		return -1;
	}

	tFileName=strrchr(inPath,'/')+1;

	if (snprintf(tAppleDoublePath,sizeof(tAppleDoublePath),"%.*s._%s",(int) (tFileName-inPath),inPath,tFileName)>=(int) sizeof(tAppleDoublePath))
	{
		if (gDryRun==true)
		{
			DryRunWarn(inPath,"the path of its AppleDouble file is too long; goldin stops here");

			return 0;
		}

		logerror("Unable to create the AppleDouble file of %s (the path is too long)\n",inPath);

		return -1;
	}

	/* A dry run checks what it can, but writes nothing */

	if (gDryRun==true)
	{
		DryRunCheckAppleDoubleFile(inPath,inFileStat,tFileName,tAppleDoublePath,tResourceForkSize);

		return 0;
	}

	/* We need to create a ._file */

tryagain:

	tNewFileDescriptor=open(tAppleDoublePath,O_WRONLY|O_CREAT|O_EXCL,S_IRUSR|S_IWUSR);

	if (tNewFileDescriptor<0)
	{
		switch(errno)
		{
			case ENAMETOOLONG:
				/* The file name is too long */

				/* Write the error */

				logerror("The name of %s is too long to add the ._ prefix (the maximum is %ld characters)\n",inPath,gMaxFileNameLength);

				break;
			case ENOSPC:

				logerror("Unable to create the AppleDouble file of %s (the disk is full)\n",inPath);

				break;

			case EDQUOT:

				logerror("Unable to create the AppleDouble file of %s (your disk quota has been exceeded)\n",inPath);

				break;
			case EEXIST:

				/* The file already exists, we need to try to delete it before recreating it */

				if (unlink(tAppleDoublePath)==0)
				{
					goto tryagain;
				}

				logerror("Unable to replace the existing AppleDouble file of %s (%s)\n",inPath,strerror(errno));

				break;

			case EROFS:

				logerror("Unable to create the AppleDouble file of %s (the volume is locked)\n",inPath);

				break;

			case EACCES:

				logerror("Unable to create the AppleDouble file of %s (permission denied)\n",inPath);

				break;

			default:

				logerror("Unable to create the AppleDouble file of %s (%s)\n",inPath,strerror(errno));

				break;
		}
		
		return -1;
	}

	/* Write the Magic Number */

	OSWriteBigInt32(tHeader,0x00,0x00051607);

	/* Write the Version Number */

	OSWriteBigInt32(tHeader,0x04,0x00020000);

	/* Write the Filler */

	memset(tHeader+0x08,0,16);

	/* Write the Number of Entries */

	OSWriteBigInt16(tHeader,0x18,GOLDIN_NUMBER_OF_ENTRIES);

	/* Write the Entries Descriptor */

	/* **** Finder Info */

	OSWriteBigInt32(tHeader,0x1A,0x00000009);				/* Finder Info ID */
	OSWriteBigInt32(tHeader,0x1E,GOLDIN_HEADER_SIZE);
	OSWriteBigInt32(tHeader,0x22,GOLDIN_FINDER_INFO_SIZE);	/* 32 bytes */

	/* **** Resource Fork */

	OSWriteBigInt32(tHeader,0x26,0x00000002);				/* Resource Fork ID */
	OSWriteBigInt32(tHeader,0x2A,GOLDIN_HEADER_SIZE+GOLDIN_FINDER_INFO_SIZE);
	OSWriteBigInt32(tHeader,0x2E,tResourceForkSize);		/* As you can see the AppleDouble format file is not ready for forks bigger than 4 GB */

	/* Write the Entries */

	/* **** Write Finder Info (it is already in the big-endian layout of the AppleDouble file, so there is nothing to swap) */

	memcpy(tHeader+GOLDIN_HEADER_SIZE,tFinderInfo,GOLDIN_FINDER_INFO_SIZE);

	if (WriteBuffer(tNewFileDescriptor,tHeader,sizeof(tHeader))!=0)
	{
		tErrno=errno;

		goto writebail;
	}

	/* **** Write Resource Fork? */

	if (tResourceForkSize>0)
	{
		/* We need to be clever and copy the Resource Fork by chunks to avoid using too much memory */

		static uint8_t sBuffer[GOLDIN_BUFFER_ONE_MEGABYTE_SIZE];
		uint32_t tCopiedSize=0;

		while (tCopiedSize<tResourceForkSize)
		{
			size_t tReadRequestCount=sizeof(sBuffer);
			ssize_t tReadActualCount;

			if (tReadRequestCount>tResourceForkSize-tCopiedSize)
				tReadRequestCount=tResourceForkSize-tCopiedSize;

			tReadActualCount=getxattr(inPath,XATTR_RESOURCEFORK_NAME,sBuffer,tReadRequestCount,tCopiedSize,XATTR_NOFOLLOW);

			if (tReadActualCount<=0)
			{
				/* A problem occurred while reading the Resource Fork */

				if (tReadActualCount<0)
					logerror("Unable to read the resource fork of %s (%s)\n",inPath,strerror(errno));
				else
					logerror("Unable to read the resource fork of %s (it was shortened while being copied)\n",inPath);

				goto deletebail;
			}

			if (WriteBuffer(tNewFileDescriptor,sBuffer,(size_t) tReadActualCount)!=0)
			{
				/* A problem occurred while writing the Resource Fork Data to the AppleDouble file */

				tErrno=errno;

				goto writebail;
			}

			tCopiedSize+=(uint32_t) tReadActualCount;
		}
	}

	/* Copy the permissions (the owner and group are only copied when run as root) */

	if ((geteuid()==0 && fchown(tNewFileDescriptor,inFileStat->st_uid,inFileStat->st_gid)!=0 && errno!=EPERM) ||
		fchmod(tNewFileDescriptor,inFileStat->st_mode & ALLPERMS)!=0)
	{
		logerror("Permissions, owner and group could not be set for the AppleDouble file of %s (%s)\n",inPath,strerror(errno));

		close(tNewFileDescriptor);

		return -1;
	}

	if (close(tNewFileDescriptor)!=0)
	{
		tErrno=errno;

		tNewFileDescriptor=-1;

		goto writebail;
	}

	/* Strip the resource fork, now that it has been copied. A hard link (split with --linksplit) keeps its resource fork:
	   stripping it would remove it from the file under all its names, and those elsewhere may have no AppleDouble file */

	if (tResourceForkSize>0 && gStripResourceForks==true && tIsHardLink==false)
	{
		if (removexattr(inPath,XATTR_RESOURCEFORK_NAME,XATTR_NOFOLLOW)!=0)
		{
			switch(errno)
			{
				case ENOATTR:
					/* This is not important */
					break;
				default:
					logerror("Resource fork could not be stripped from %s (%s)\n",inPath,strerror(errno));

					return -1;
			}
		}
		else
		{
			tStripped=true;
		}
	}

	/* With --warnsymlink, report items reached through a symbolic link */

	if (gWarnSymbolicLinks==true && gSymbolicLink[0]!='\0')
		logerror("Warning: %s %s, reached through the symbolic link \"%s\"\n",(tStripped==true) ? "split and stripped" : "split",inPath,gSymbolicLink);

	return 0;

writebail:

	switch(tErrno)
	{
		case ENOSPC:
			logerror("Unable to write the AppleDouble file of %s (the disk is full)\n",inPath);
			break;
		case EDQUOT:
			logerror("Unable to write the AppleDouble file of %s (your disk quota has been exceeded)\n",inPath);
			break;
		default:
			logerror("Unable to write the AppleDouble file of %s (%s)\n",inPath,strerror(tErrno));
			break;
	}

deletebail:

	if (tNewFileDescriptor>=0)
		close(tNewFileDescriptor);

	/* Don't leave a partial AppleDouble file behind */

	unlink(tAppleDoublePath);

	return -1;
}

static void LogFolderReadError(const char * inPath,int inErrno)
{
	if (inErrno==EACCES)
		logerror("Unable to read the contents of %s (permission denied)\n",inPath);
	else
		logerror("Unable to read the contents of %s (%s)\n",inPath,strerror(inErrno));
}

/* Split the forks of an item and, for a folder, of its contents */

static int SplitForks(const char * inPath)
{
	char * tPaths[]={(char *) inPath,NULL};
	FTS * tFileSystem;
	FTSENT * tEntry;
	dev_t tDevice=0;
	int tStatus=0;

	/* fts reads the list of a folder's items before returning any of them, so the ._ files written in the folder are not
	   met (and split) again */

	tFileSystem=fts_open(tPaths,FTS_PHYSICAL|FTS_NOCHDIR|FTS_XDEV,NULL);

	if (tFileSystem==NULL)
	{
		if (gDryRun==true)
		{
			DryRunWarn(inPath,"can't get information about it (%s); goldin stops here",strerror(errno));

			return 0;
		}

		logerror("Unable to get information about %s (%s)\n",inPath,strerror(errno));

		return -1;
	}

	while (tStatus==0 && (tEntry=fts_read(tFileSystem))!=NULL)
	{
		switch(tEntry->fts_info)
		{
			case FTS_D:

				if (tEntry->fts_level==FTS_ROOTLEVEL)
				{
					tDevice=tEntry->fts_statp->st_dev;
				}
				else
				{
					/* A volume mounted inside the folder is left alone (and FTS_XDEV doesn't descend into it) */

					if (tEntry->fts_statp->st_dev!=tDevice)
					{
						if (gDryRun==true)
							DryRunAddItem(tEntry->fts_path,false,"a volume is mounted here");

						break;
					}

					/* With --norecurse, folders inside the path are split but not descended into */

					if (gNoRecurse==true)
						fts_set(tFileSystem,tEntry,FTS_SKIP);
				}

				tStatus=SplitFileIfNeeded(tEntry->fts_path,tEntry->fts_statp);

				break;

			case FTS_F:
			case FTS_DEFAULT:

				/* Check this is not a Hard Link (unless --linksplit) */

				if (gSplitHardLinks==false && S_ISREG(tEntry->fts_statp->st_mode) && tEntry->fts_statp->st_nlink>1)
				{
					if (gDryRun==true)
						DryRunAddItem(tEntry->fts_path,false,"hard link");

					break;
				}

				tStatus=SplitFileIfNeeded(tEntry->fts_path,tEntry->fts_statp);

				break;

			case FTS_SL:
			case FTS_SLNONE:

				/* Symbolic links are not split */

				if (gDryRun==true)
					DryRunAddItem(tEntry->fts_path,false,"symbolic link");

				break;

			case FTS_DNR:
			case FTS_ERR:

				/* Report the folder and carry on with the rest of the tree */

				if (gDryRun==true)
				{
					if (tEntry->fts_errno==EACCES)
						DryRunWarn(tEntry->fts_path,"its contents can't be read (permission denied), so they are skipped");
					else
						DryRunWarn(tEntry->fts_path,"its contents can't be read (%s), so they are skipped",strerror(tEntry->fts_errno));

					break;
				}

				LogFolderReadError(tEntry->fts_path,tEntry->fts_errno);

				break;

			case FTS_NS:

				if (gDryRun==true)
				{
					DryRunWarn(tEntry->fts_path,"can't get information about it (%s); goldin stops here",strerror(tEntry->fts_errno));

					break;
				}

				logerror("Unable to get information about %s (%s)\n",tEntry->fts_path,strerror(tEntry->fts_errno));

				tStatus=-1;

				break;

			case FTS_DC:

				/* A folder that contains itself */

				if (gDryRun==true)
					DryRunAddItem(tEntry->fts_path,false,"folder that contains itself");

				break;

			default:
				
				/* FTS_DP: a folder whose contents have been processed */
				
				break;
		}
	}
	
	if (tStatus==0 && errno!=0)
	{
		/* fts_read returns NULL, with errno set to 0 when the whole tree has been read */

		if (gDryRun==true)
			DryRunWarn(inPath,"its contents can't all be read (%s); goldin stops here",strerror(errno));
		else
			logerror("Unable to read the contents of %s (%s)\n",inPath,strerror(errno));

		if (gDryRun==false)
			tStatus=-1;
	}
	
	fts_close(tFileSystem);

	return tStatus;
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
           "  --linksplit, -l      Split hard links too (never stripped)\n"\
           "  --warnsymlink, -w    Report items split through a symbolic link\n"\
           "  --dryrun, -d         List what would be split, and any problems\n"\
           "  --norecurse          Do not descend into subdirectories\n"\
           "  --version            Show the version of this tool\n"\
           "\n");
}

/* --dryrun: the absolute path of a PATH that can't be resolved, to list it */

static void DryRunAbsolutePath(const char * inPath,char * outPath,size_t inMaxLength)
{
	char tCurrentFolder[PATH_MAX];

	if (inPath[0]=='/' || getcwd(tCurrentFolder,sizeof(tCurrentFolder))==NULL)
		snprintf(outPath,inMaxLength,"%s",inPath);
	else
		snprintf(outPath,inMaxLength,"%s/%s",tCurrentFolder,inPath);
}

static int SplitPath(const char * inPath)
{
	char tResolvedPath[PATH_MAX];

	/* With --warnsymlink, find out whether the PATH, as it was given, goes through a symbolic link */

	gSymbolicLink[0]='\0';

	if (gWarnSymbolicLinks==true)
		FindSymbolicLink(inPath,gSymbolicLink,sizeof(gSymbolicLink));

	if (realpath(inPath,tResolvedPath)!=NULL)
	{
		struct statfs tStatFileSystem;

		if (statfs(tResolvedPath,&tStatFileSystem)!=0)
		{
			if (gDryRun==true)
			{
				if (errno==ENOENT)
					DryRunWarn(tResolvedPath,"it was not found; goldin stops here");
				else
					DryRunWarn(tResolvedPath,"its file system information can't be read (%s); goldin stops here",strerror(errno));

				return 0;
			}

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

		bool tIsHFS=(strcmp(tStatFileSystem.f_fstypename,"hfs")==0);
		bool tIsAPFS=(strcmp(tStatFileSystem.f_fstypename,"apfs")==0);

		if (tIsHFS==false && (tIsAPFS==false || gAllowAPFS==false))
		{
			/* Return (-2) if this is not a supported file system */

			if (gDryRun==true)
			{
				if (tIsAPFS==true)
					DryRunWarn(tResolvedPath,"it is on an APFS volume (use -a or --apfs to allow APFS); goldin stops here");
				else if (gAllowAPFS==true)
					DryRunWarn(tResolvedPath,"it is not on an HFS, HFS+ or APFS volume; goldin stops here");
				else
					DryRunWarn(tResolvedPath,"it is not on an hfs disk; goldin stops here");

				return 0;
			}

			if (tIsAPFS==true)
				logerror("\"%s\" is on an APFS volume; use -a or --apfs to allow APFS\n",inPath);
			else if (gAllowAPFS==true)
				logerror("\"%s\" is not on an HFS, HFS+ or APFS volume\n",inPath);
			else
				logerror("\"%s\" is not on an hfs disk\n",inPath);

			return 254;
		}

		gMaxFileNameLength=pathconf(tResolvedPath,_PC_NAME_MAX);

		if (gMaxFileNameLength<0)
		{
			if (gDryRun==true)
			{
				DryRunWarn(tResolvedPath,"the maximum file name length can't be found (%s); goldin stops here",strerror(errno));

				return 0;
			}

			logerror("An error occurred while getting the maximum file name length for \"%s\" (%s)\n",inPath,strerror(errno));

			return -1;
		}

		gMaxFileNameLength-=2;

		if (gVerboseMode==true && gDryRun==false)
			printf("Splitting %s...\n",inPath);

		if (SplitForks(tResolvedPath)!=0)
			return -1;
	}
	else
	{
		if (gDryRun==true)
		{
			char tAbsolutePath[PATH_MAX];
			int tErrno=errno;

			DryRunAbsolutePath(inPath,tAbsolutePath,sizeof(tAbsolutePath));

			if (tErrno==ENOENT)
				DryRunWarn(tAbsolutePath,"it was not found; goldin stops here");
			else
				DryRunWarn(tAbsolutePath,"its path can't be resolved (%s); goldin stops here",strerror(tErrno));

			return 0;
		}

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
            {"linksplit",                   no_argument,        NULL,    'l'},
            {"warnsymlink",                 no_argument,        NULL,    'w'},
            {"dryrun",                      no_argument,        NULL,    'd'},
            {"norecurse",                   no_argument,        NULL,    0},
            {"version",                     no_argument,        NULL,    0},

            {0, 0, 0, 0}
        };

        int tOptionIndex = 0;

        c = getopt_long (argc, (char **) argv, "adhlsuvw",tLongOptions, &tOptionIndex);

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
                    gNoRecurse=true;
                }

                break;

            case 's':

                gStripResourceForks=true;

                break;

            case 'a':

                gAllowAPFS=true;

                break;

            case 'l':

                gSplitHardLinks=true;

                break;

            case 'w':

                gWarnSymbolicLinks=true;

                break;

            case 'd':

                gDryRun=true;

                break;

            case 'v':

                gVerboseMode=true;

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

	/* A dry run writes nothing, so --strip is ignored */

	if (gDryRun==true)
		gStripResourceForks=false;

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

	/* A dry run checks every PATH, then lists what it found */

	if (gDryRun==true)
		return DryRunList();

    return 0;
}
