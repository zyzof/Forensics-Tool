#include <stdio.h>
#include <dirent.h>
#include "case.h"
#include "deletedFiles.h"
#include "deletedFilesFat32.h"
#include "server.h"

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

void printLongFileNameEntry(Case c, LONG_FILENAME_ENTRY entry) {
	stringstream streambuffer;
	if(isAscii(entry.char1.data[0])) {
		streambuffer << entry.char1.data[0];
	}
	if(isAscii(entry.char2.data[0])) {
		streambuffer << entry.char2.data[0];
	}
	if(isAscii(entry.char3.data[0])) {
		streambuffer << entry.char3.data[0];
	}
	if(isAscii(entry.char4.data[0])) {
		streambuffer << entry.char4.data[0];
	}
	if(isAscii(entry.char5.data[0])) {
		streambuffer << entry.char5.data[0];
	}
	if(isAscii(entry.char6.data[0])) {
		streambuffer << entry.char6.data[0];
	}
	if(isAscii(entry.char7.data[0])) {
		streambuffer << entry.char7.data[0];
	}
	if(isAscii(entry.char8.data[0])) {
		streambuffer << entry.char8.data[0];
	}
	if(isAscii(entry.char9.data[0])) {
		streambuffer << entry.char9.data[0];
	}
	if(isAscii(entry.char10.data[0])) {
		streambuffer << entry.char10.data[0];
	}
	if(isAscii(entry.char11.data[0])) {
		streambuffer << entry.char11.data[0];
	}
	if(isAscii(entry.char12.data[0])) {
		streambuffer << entry.char12.data[0];
	}
	if(isAscii(entry.char13.data[0])) {
		streambuffer << entry.char13.data[0];
	}
	
	put_output_string(c, streambuffer.str());
}

void printShortFileName(Case c, DIRECTORY_ENTRY entry) {
	stringstream streambuffer;
	
	streambuffer << entry.allocationStatus.data;
	for (int i = 0; i < 7; i++) {
		streambuffer << entry.fileName[i];
	}
	streambuffer << ".";
	for (int i = 0; i < 3; i++) {
		streambuffer << entry.fileExtension[i];
	}
	
	put_output_string(c, streambuffer.str());
}

bool isPartOfLongFileName(DIRECTORY_ENTRY entry) {
	if (entry.fileAttributes.data == 0x0F) {
		return true;
	} else {
		return false;
	}
}

void printType(Case c, DIRECTORY_ENTRY entry) {
	stringstream streambuffer;
	
	put_output(c, "   Type: ");
	if (entry.fileAttributes.data == 0x10) {
		put_output(c, "directory\n");
	} else {
		put_output(c, "file\n");
	}
}

void printDate(Case c, WORD date) {
	stringstream streambuffer;
	
	unsigned int data = readWord(date);
	int day = data & 31;
	int month = (data & 480) >> 5;
	int year = 1980 + ((data & 65042) >> 9);
	streambuffer << day << "/" << month << "/" << year;
	
	put_output_string(c, streambuffer.str());
}

void printTime(Case c, WORD time) {
	stringstream streambuffer;
	
	unsigned int data = readWord(time);
	int hours = (data & 63488) >> 11;
	int mins = (data & 1008) >> 4;
	int secs = (data & 15) * 2;
	
	if (hours < 10) {
		streambuffer << "0";
	}
	
	streambuffer << hours << ":";
	
	if (mins < 10) {
		streambuffer << "0";
	}
	streambuffer << mins << ":";
	
	if (secs < 10) {
		streambuffer << "0";
	}
	streambuffer << secs;
	
	put_output_string(c, streambuffer.str());
}

void printCreationTime(Case c, DIRECTORY_ENTRY entry) {
	stringstream streambuffer;
	
	put_output(c, "   Created: ");
	
	printDate(c, entry.creationDate);
	put_output(c, " ");
	printTime(c, entry.creationTimeHoursMinutesSeconds);
	put_output(c, "\n");
}

void printLastAccessedTime(Case c, DIRECTORY_ENTRY entry) {
	put_output(c, "   Last accessed: ");
	printDate(c, entry.accessDate);
	put_output(c, "\n");
}

void printLastModifiedtime(Case c, DIRECTORY_ENTRY entry) {
	put_output(c, "   Last modified: ");
	printDate(c, entry.modifiedDate);
	put_output(c, " ");
	printTime(c, entry.modifiedTime);
	put_output(c, "\n");
}

void printRemainingFileAttributes(Case c, DIRECTORY_ENTRY entry) {
	printType(c, entry);
	printCreationTime(c, entry);
	printLastAccessedTime(c, entry);
	printLastModifiedtime(c, entry);	
}

int listDeletedFilesFat32(Case c, int disk_fd) {
	stringstream streambuffer;
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
			put_output(c, "Deleted file found:\n");
			put_output(c, "   File name: ");
			
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
					printLongFileNameEntry(c, fileNameEntry);
				}
				
				rootDirLocation += (fileNameRecords+1) * 32;
			} else {
				printShortFileName(c, entry);
			}
			put_output(c, "\n");
			
			printRemainingFileAttributes(c, entry);
			
			put_output(c, "\n\n");
		}
		rootDirLocation += 32;
		n = pread(disk_fd, &entry, sizeof(DIRECTORY_ENTRY), rootDirLocation);
	}
	
	return deletedFileCount;
}
