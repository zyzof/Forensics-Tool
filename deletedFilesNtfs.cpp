#include <stdio.h>
#include <dirent.h>
#include "case.h"
#include "deletedFiles.h"
#include "deletedFilesNtfs.h"

using namespace std;

typedef struct _NTFS_MFT_FILE_ENTRY_HEADER {
	unsigned char 		fileSignature[4];
	WORD 		wFixupOffset;
	WORD 		wFixupSize;
	LONGLONG 	n64LogSeqNumber;
	WORD 		wSequence;
	WORD 		wHardLinks;
	WORD 		wAttribOffset;
	WORD 		wFlags;
	DWORD		dwRecLength;
	DWORD 		dwAllLength;
	LONGLONG 	n64BaseMftRec;
	WORD 		wNextAttrID;
	WORD		wFixupPattern;
	DWORD		dwMftRecNumber;
} NTFS_MFT_FILE_ENTRY_HEADER, *P_NTFS_MFT_FILE_ENTRY_HEADER;

typedef struct _NTFS_ATTRIBUTE {
	DWORD dwType;
	DWORD dwFullLength;
	BYTE uchNonResFlag;
	BYTE uchNameLength;
	WORD wNameOffset;
	WORD wFlags;
	WORD wID;
	
	union ATTR {
		
		struct RESIDENT {
			DWORD dwLength;
			WORD wAttrOffset;
			BYTE uchIndexedTag;
			BYTE uchPadding;
		} Resident;
		
		struct NONRESIDENT {
			LONGLONG n64StartVCN;
			LONGLONG n64EndVCN;
			WORD wDatarunOffset;
			WORD wCompressionSize;
			BYTE uchPadding[4];
			LONGLONG n64AllocSize;
			LONGLONG n64RealSize;
			LONGLONG n64StreamSize;
		} NonResident;
	} Attr;
} NTFS_ATTRIBUTE, *P_NTFS_ATTRIBUTE;

typedef struct _NTFS_RESIDENT_ATTRIBUTE {
	DWORD dwType;
	DWORD dwFullLength;
	BYTE uchNonResFlag;
	BYTE uchNameLength;
	WORD wNameOffset;
	WORD wFlags;
	WORD wID;
		
			DWORD dwLength;
			WORD wAttrOffset;
			BYTE uchIndexedTag;
			BYTE uchPadding;

} NTFS_RESIDENT_ATTRIBUTE, *P_NTFS_RESIDENT_ATTRIBUTE;

typedef struct _FILENAME_ATTRIBUTE {
	LONGLONG n64ParentRef;
	LONGLONG n64CreationTime;
	LONGLONG n64AlteredTime;
	LONGLONG n64MftChangedTime;
	LONGLONG n64ReadTime;
	LONGLONG n64AllocatedFileSize;
	LONGLONG n64RealFileSize;
	DWORD 	 dwFlags;
	DWORD 	 dwEAsAndReparse;
	BYTE	 uchFilenameLengthInChars;
	BYTE 	 uchFilenameNamespace;
	unsigned char	 fileName[510];	//Max filename length (255 chars, doubly spaced)
} FILENAME_ATTRIBUTE, *P_FILENAME_ATTRIBUTE;


// HELPER METHODS:

int getBytesPerSector(int disk) {
	WORD raw;
	pread(disk, &raw, 2, 0x0B);
	int bytesPerSector = readWord(raw);
	
	printf("  Number of bytes per sector: %d\n", bytesPerSector);
	return bytesPerSector;
}

int getSectorsPerCluster(int disk) {
	BYTE raw;
	pread(disk, &raw, 1, 0x0D);
	int sectorsPerCluster = readByte(raw);
	
	printf("  Number of sectors per cluster: %d\n", sectorsPerCluster);
	return sectorsPerCluster;
}

unsigned long long getMftClusterNumber(int disk) {
	LONGLONG raw;
	
	pread(disk, &raw, sizeof(LONGLONG), 0x30);
	
	unsigned long long mftClusterNumber = readLongLong(raw);
					   
	printf("  $MFT cluster number: %llu\n", mftClusterNumber);
	return mftClusterNumber;
}

unsigned long long getMftOffset(int disk) {
	int bytesPerSector = getBytesPerSector(disk);
	int sectorsPerCluster = getSectorsPerCluster(disk);
	
	int bytesPerCluster = bytesPerSector * sectorsPerCluster;
	printf("  Bytes per cluster: %d\n", bytesPerCluster);
	
	unsigned long long mftClusterNumber = getMftClusterNumber(disk);
	unsigned long long mftOffset = mftClusterNumber * bytesPerCluster;
	
	printf(" ...$MFT found at byte offset: %llu\n\n", mftOffset);
	return mftOffset;
}

NTFS_MFT_FILE_ENTRY_HEADER getFileEntryHeader(int disk, unsigned long long offset) {
	NTFS_MFT_FILE_ENTRY_HEADER header;
	pread(disk, &header, sizeof(NTFS_MFT_FILE_ENTRY_HEADER), offset);
	
	return header;
}

NTFS_RESIDENT_ATTRIBUTE getResidentAttribute(int disk, unsigned long long offset) {
	NTFS_RESIDENT_ATTRIBUTE attribute;
	
	pread(disk, &attribute, sizeof(NTFS_RESIDENT_ATTRIBUTE), offset);
	
	return attribute;
}

unsigned long long getAttributeOffset(int disk, unsigned long long mftOffset, NTFS_MFT_FILE_ENTRY_HEADER header) {
	unsigned long long relativeAttributeOffset = header.wAttribOffset.data[0] + (header.wAttribOffset.data[1] << 8);
	unsigned long long absoluteAttributeOffset = mftOffset + relativeAttributeOffset;
	
	return absoluteAttributeOffset;
}

unsigned long long getAttributeDataLength(NTFS_RESIDENT_ATTRIBUTE attribute) {
	unsigned long long attrDataLength =	attribute.dwLength.data[0] + 
										(attribute.dwLength.data[1] << 8) + 
										(attribute.dwLength.data[2] << 16) + 
										(attribute.dwLength.data[3] << 24);
	
	return attrDataLength;
}

void printType(FILENAME_ATTRIBUTE attr) {
	long flags = readDWord(attr.dwFlags);
	
	cout << "   Type: ";
	if (flags == 0x10000000) {
		cout << "Directory" << endl;
	} else {
		cout << "File" << endl;
	}
}

void printCreationTime(FILENAME_ATTRIBUTE attr) {
	unsigned long long creationTimeRaw = readLongLong(attr.n64CreationTime);
	
	cout << "   Creation time: " << creationTimeRaw << endl;
}

void printLastModifiedTime(FILENAME_ATTRIBUTE attr) {
	unsigned long long modifiedTimeRaw = readLongLong(attr.n64AlteredTime);
	
	cout << "   Last modified: " << modifiedTimeRaw << endl;
}

void printLastAccessedTime(FILENAME_ATTRIBUTE attr) {
	unsigned long long lastAccessedTimeRaw = readLongLong(attr.n64ReadTime);
	
	cout << "   Last accessed: " << lastAccessedTimeRaw << endl;
}

void printFileNameAttribute(int disk, NTFS_RESIDENT_ATTRIBUTE attribute, unsigned long long attributeHeaderOffset) {
	FILENAME_ATTRIBUTE fileNameAttribute;
	
	unsigned long long fileNameAttrOffset = attributeHeaderOffset + readWord(attribute.wAttrOffset);
											
	unsigned long long attrDataLength = getAttributeDataLength(attribute);
						
	pread(disk, &fileNameAttribute, attrDataLength, fileNameAttrOffset);
	
	int fileNameLength = fileNameAttribute.uchFilenameLengthInChars.data;
	
	stringstream ss;
	
	for (int i = 0; i < fileNameLength; i++) {
		ss << fileNameAttribute.fileName[i*2];	//2 Bytes per char so desired char will be twice as far away
	}
	
	cout << "Deleted file: \n   Name: " << ss.str() << endl;
	
	printType(fileNameAttribute);
	//printCreationTime(fileNameAttribute);
	//printLastModifiedTime(fileNameAttribute);
	//printLastAccessedTime(fileNameAttribute);
}

bool isMftRecord(NTFS_MFT_FILE_ENTRY_HEADER header) {
	char buffer[4];
	for (int i = 0; i < 4; i++) {
		buffer[i] = (char)header.fileSignature[i];
	}
	
	if (strncmp(buffer, "FILE", 4) == 0) {
		return true;
	} else {
		return false;
	}
}

bool isDeletedFile(NTFS_MFT_FILE_ENTRY_HEADER header) {
	char deletedFile = 0x00;
	char deletedFolder = 0x02;
	char inUseFlag = header.wFlags.data[0];	//wFlags is actually 2 bytes, but we're only interested in the first one.
	
	if (inUseFlag == deletedFile || inUseFlag == deletedFolder) {
		return true;
	} else {
		return false;
	}
}

bool atEndAttribute(NTFS_RESIDENT_ATTRIBUTE attribute) {
	if (attribute.dwType.data[0] == 0xff) {
		return true;
	} else {
		return false;
	}
}

bool isResidentAttribute(NTFS_RESIDENT_ATTRIBUTE attribute) {
	if (attribute.uchNonResFlag.data == 0x00) {
		return true;
	} else {
		return false;
	}
}

bool isFilenameAttribute(NTFS_RESIDENT_ATTRIBUTE attribute) {
	if (attribute.dwType.data[0] == 0x30) {
		return true;
	} else {
		return false;
	}
}

//END HELPER METHODS



//MAIN METHOD:
int listDeletedFilesNtfs(int disk_fd) {
	int deletedFileCount = 0;
	unsigned long long mftOffset;
	
	unsigned char buffer[1024];
	
	cout << " Finding $MFT..." << endl;
	mftOffset = getMftOffset(disk_fd);
	
	
	NTFS_MFT_FILE_ENTRY_HEADER header;
	NTFS_RESIDENT_ATTRIBUTE attribute;
	
	cout << "Listing deleted files..." << endl;
	header = getFileEntryHeader(disk_fd, mftOffset);
	while (isMftRecord(header)) {
		
		if (isDeletedFile(header)) {
			
			unsigned long long attrOffset = getAttributeOffset(disk_fd, mftOffset, header);

			attribute = getResidentAttribute(disk_fd, attrOffset);
			
			while (!atEndAttribute(attribute)) {
				
				if (isResidentAttribute(attribute)) {
					unsigned long long attrDataLength = getAttributeDataLength(attribute);
					
					if (isFilenameAttribute(attribute)) {	//Filename attr type
						deletedFileCount++;
						stringstream ssCount;
						ssCount << deletedFileCount;
						
						printFileNameAttribute(disk_fd, attribute, attrOffset);
					}
					
					attrOffset += sizeof(NTFS_RESIDENT_ATTRIBUTE) + attrDataLength;
					
					while (attrOffset % 8 != 0) {	//Increase offset to 8byte alignment. Not sure if necessary?
						attrOffset++;
					}					
				} else {
					attrOffset += 64;	//Size of nonresident attr header.
				}
				
				attribute = getResidentAttribute(disk_fd, attrOffset);
			}
		}
		
		mftOffset += 1024; //Size of entry in MFT. (Not just header!)
		header = getFileEntryHeader(disk_fd, mftOffset);
	}
	
	cout << "Reached end of $MFT" << endl << endl;
	
	return deletedFileCount;
}
