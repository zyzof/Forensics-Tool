#include <stdio.h>
#include <dirent.h>
#include "case.h"
#include "deletedFiles.h"
#include "deletedFilesFat32.h"

using namespace std;

typedef struct _BPB {
	WORD bytesPerSector;	//
	BYTE sectorsPerCluster;	//
	WORD reservedSectors;	//
	BYTE numFats;	//
	WORD maxEntriesInRootDir;
	WORD smallSectors;
	BYTE mediaDescriptor;
	WORD sectorsPerFatSmallv;
	WORD sectorsPerTrack;
	WORD numHeads;
	DWORD hiddenSectors;
	DWORD largeSectors;
	DWORD sectorsPerFat;	//
	WORD unused;
	WORD fileSysVersion;
	DWORD rootClusterNumber;	//
	WORD fileSysInfoSectorNum;
	WORD backupBootSector;
	unsigned char reserved[12];
} BPB;

typedef struct _DIRECTORY_ENTRY {
	BYTE allocationStatus;
	unsigned char fileName[7];
	unsigned char fileExtension[3];
	BYTE fileAttributes;
	BYTE reserved;
	BYTE creationTimeTenthsOfSeconds;
	WORD creationTimeHoursMinutesSeconds;
	WORD creationDate;
	WORD accessDate;
	WORD firstClusterHigh;
	WORD modifiedTime;	//hhMMss
	WORD modifiedDate;
	WORD firstClusterLow;
	DWORD fileSize;
} DIRECTORY_ENTRY;

typedef struct _LONG_FILENAME_ENTRY {
	BYTE ordinalField;
	WORD char1;
	WORD char2;
	WORD char3;
	WORD char4;
	WORD char5;
	BYTE flag;
	BYTE reserved;
	BYTE checksum;
	WORD char6;
	WORD char7;
	WORD char8;
	WORD char9;
	WORD char10;
	WORD char11;
	WORD constant0000h;
	WORD char12;
	WORD char13;
} LONG_FILENAME_ENTRY;

unsigned long long getRootDirLocation(int disk) {
	BPB bpb;
	pread(disk, &bpb, sizeof(BPB), 0xB);
	
	unsigned long long fdtClusterNum = readDWord(bpb.rootClusterNumber);
	
	int numReservedSectors = readWord(bpb.reservedSectors);
	long long numSectorsPerFat = readDWord(bpb.sectorsPerFat);
	int numFats = readByte(bpb.numFats);
	long long fdtStartCluster = readDWord(bpb.rootClusterNumber);
	int sectorsPerCluster = readByte(bpb.sectorsPerCluster);
	
	long long bytesPerSector = readWord(bpb.bytesPerSector);
	
	unsigned long long fdtStartSector = numReservedSectors + (numSectorsPerFat * numFats) + (fdtStartCluster - 2) * sectorsPerCluster;
	
	return fdtStartSector * bytesPerSector;
}

bool isDeletedEntry(DIRECTORY_ENTRY entry) {
	if (entry.allocationStatus.data == 0xE5) {
		return true;
	} else {
		return false;
	}
}

bool isAscii(unsigned char c) {
	if ( c >= 32 && c <= 126) {
		return true;
	} else {
		return false;
	}
}

void printLongFileNameEntry(LONG_FILENAME_ENTRY entry) {
	if(isAscii(entry.char1.data[0])) {
		printf("%c", entry.char1.data[0]);
	}
	if(isAscii(entry.char2.data[0])) {
		printf("%c", entry.char2.data[0]);
	}
	if(isAscii(entry.char3.data[0])) {
		printf("%c", entry.char3.data[0]);
	}
	if(isAscii(entry.char4.data[0])) {
		printf("%c", entry.char4.data[0]);
	}
	if(isAscii(entry.char5.data[0])) {
		printf("%c", entry.char5.data[0]);
	}
	if(isAscii(entry.char6.data[0])) {
		printf("%c", entry.char6.data[0]);
	}
	if(isAscii(entry.char7.data[0])) {
		printf("%c", entry.char7.data[0]);
	}
	if(isAscii(entry.char8.data[0])) {
		printf("%c", entry.char8.data[0]);
	}
	if(isAscii(entry.char9.data[0])) {
		printf("%c", entry.char9.data[0]);
	}
	if(isAscii(entry.char10.data[0])) {
		printf("%c", entry.char10.data[0]);
	}
	if(isAscii(entry.char11.data[0])) {
		printf("%c", entry.char11.data[0]);
	}
	if(isAscii(entry.char12.data[0])) {
		printf("%c", entry.char12.data[0]);
	}
	if(isAscii(entry.char13.data[0])) {
		printf("%c", entry.char13.data[0]);
	}
}

void printShortFileName(DIRECTORY_ENTRY entry) {
	printf("%c", entry.allocationStatus.data);
	for (int i = 0; i < 7; i++) {
		printf("%c", entry.fileName[i]);
	}
	printf(".");
	for (int i = 0; i < 3; i++) {
		printf("%c", entry.fileExtension[i]);
	}
}

bool isPartOfLongFileName(DIRECTORY_ENTRY entry) {
	if (entry.fileAttributes.data == 0x0F) {
		return true;
	} else {
		return false;
	}
}

void printType(DIRECTORY_ENTRY entry) {
	printf("   Type: ");
	if (entry.fileAttributes.data == 0x10) {
		printf("directory\n");
	} else {
		printf("file\n"); 	//TODO: account for readonly, system etc flags.
	}
}

void printDate(WORD date) {
	unsigned int data = readWord(date);
	int day = data & 31;
	int month = (data & 480) >> 5;
	int year = 1980 + ((data & 65042) >> 9);
	printf("%d/%d/%d", day, month, year);
}

void printTime(WORD time) {
	unsigned int data = readWord(time);
	int hours = (data & 63488) >> 11;
	int mins = (data & 1008) >> 4;
	int secs = (data & 15) * 2;
	
	if (hours < 10) {
		printf("0%d:", hours);
	} else {
		printf("%d:", hours);
	}
	
	if (mins < 10) {
		printf("0%d:", mins);
	} else {
		printf("%d:", mins);
	}
	
	if (secs < 10) {
		printf("0%d", secs);
	} else {
		printf("%d", secs);
	}
}

void printCreationTime(DIRECTORY_ENTRY entry) {
	printf("   Created: ");
	printDate(entry.creationDate);
	printf(" ");
	printTime(entry.creationTimeHoursMinutesSeconds);
	printf("\n");
}

void printLastAccessedTime(DIRECTORY_ENTRY entry) {
	printf("   Last accessed: ");
	printDate(entry.accessDate);
	printf("\n");
}

void printLastModifiedtime(DIRECTORY_ENTRY entry) {
	printf("   Last modified: ");
	printDate(entry.modifiedDate);
	printf(" ");
	printTime(entry.modifiedTime);
	printf("\n");
}

void printRemainingFileAttributes(DIRECTORY_ENTRY entry) {
	printType(entry);
	printCreationTime(entry);
	printLastAccessedTime(entry);
	printLastModifiedtime(entry);	
}

int listDeletedFilesFat32(int disk_fd) {
	int deletedFileCount = 0;
	
	unsigned long long rootDirLocation = getRootDirLocation(disk_fd);
	
	DIRECTORY_ENTRY entry;
	pread(disk_fd, &entry, sizeof(DIRECTORY_ENTRY), rootDirLocation);
	int n = 0;
	
	while (n != -1) {
		bool hasLongFileName = false;
		int fileNameRecords = 0;
		
		if (isDeletedEntry(entry)) {
			deletedFileCount++;
			printf("Deleted file found:\n ");
			printf("  File name: ");
			while (isPartOfLongFileName(entry)) {
				fileNameRecords++;
				hasLongFileName = true;
				rootDirLocation += 32;
				n = pread(disk_fd, &entry, sizeof(DIRECTORY_ENTRY), rootDirLocation);
			}
			if (hasLongFileName) {
				LONG_FILENAME_ENTRY fileNameEntry;
				
				for (int i = 0; i <= fileNameRecords; i++) {
					rootDirLocation -= 32;
					n = pread(disk_fd, &fileNameEntry, sizeof(LONG_FILENAME_ENTRY), rootDirLocation);
					printLongFileNameEntry(fileNameEntry);
				}
				
				rootDirLocation += (fileNameRecords+1) * 32;
			} else {
				printShortFileName(entry);
			}
			printf("\n");
			printRemainingFileAttributes(entry);
			
			printf("\n\n");
		}
		rootDirLocation += 32;
		n = pread(disk_fd, &entry, sizeof(DIRECTORY_ENTRY), rootDirLocation);
	}
	
	return deletedFileCount;
}
