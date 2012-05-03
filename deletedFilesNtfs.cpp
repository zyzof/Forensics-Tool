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

int getBytesPerSector(Case c, int disk) {
	stringstream streambuffer;
	WORD raw;
	pread(disk, &raw, 2, 0x0B);
	int bytesPerSector = readWord(raw);
	
	streambuffer << "  Number of bytes per sector: " << bytesPerSector << endl;
	put_output_string(c, streambuffer.str());
	return bytesPerSector;
}

int getSectorsPerCluster(Case c, int disk) {
	stringstream streambuffer;
	BYTE raw;
	pread(disk, &raw, 1, 0x0D);
	int sectorsPerCluster = readByte(raw);
	
	streambuffer << "  Number of sectors per cluster: " << sectorsPerCluster << endl;
	put_output_string(c, streambuffer.str());
	return sectorsPerCluster;
}

unsigned long long getMftClusterNumber(Case c, int disk) {
	stringstream streambuffer;
	LONGLONG raw;
	
	pread(disk, &raw, sizeof(LONGLONG), 0x30);
	
	unsigned long long mftClusterNumber = readLongLong(raw);
					   
	streambuffer << "  $MFT cluster number: " << mftClusterNumber << endl;
	put_output_string(c, streambuffer.str());
	
	return mftClusterNumber;
}

unsigned long long getMftOffset(Case c, int disk) {
	stringstream streambuffer;
	
	int bytesPerSector = getBytesPerSector(c, disk);
	int sectorsPerCluster = getSectorsPerCluster(c, disk);
	
	int bytesPerCluster = bytesPerSector * sectorsPerCluster;
	streambuffer << "  Bytes per cluster: " << bytesPerCluster << endl;
	
	unsigned long long mftClusterNumber = getMftClusterNumber(c, disk);
	unsigned long long mftOffset = mftClusterNumber * bytesPerCluster;
	
	streambuffer << " ...$MFT found at byte offset: " << mftOffset << endl << endl;
	
	put_output_string(c, streambuffer.str());
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

void printType(Case c, FILENAME_ATTRIBUTE attr) {
	stringstream streambuffer;
	
	long flags = readDWord(attr.dwFlags);
	
	streambuffer << "   Type: ";
	if (flags & 0x0001) {
		streambuffer << "read-only ";
	}
	if (flags & 0x0002) {
		streambuffer << "hidden ";
	}
	if (flags & 0x0004) {
		streambuffer << "system ";
	}
	if (flags & 0x0020) {
		streambuffer << "archive ";
	}
	if (flags & 0x0040) {
		streambuffer << "device ";
	}
	if (flags & 0x0080) {
		streambuffer << "normal ";
	}
	if (flags & 0x0100) {
		streambuffer << "temporary ";
	}
	if (flags & 0x0200) {
		streambuffer << "sparse file ";
	}
	if (flags & 0x0400) {
		streambuffer << "reparse point ";
	}
	if (flags & 0x0800) {
		streambuffer << "compressed ";
	}
	if (flags & 0x1000) {
		streambuffer << "offline ";
	}
	if (flags & 0x2000) {
		streambuffer << "not content indexed ";
	}
	if (flags & 0x4000) {
		streambuffer << "encrypted ";
	}
	if (flags & 0x10000000) {
		streambuffer << "directory" << endl;
	} else if (flags & 0x20000000) {
		streambuffer << "index view" << endl;
	} else {
		streambuffer << "file" << endl;
	}
	
	put_output_string(c, streambuffer.str());
}

void printCreationTime(Case c, FILENAME_ATTRIBUTE attr) {
	stringstream streambuffer;
	
	unsigned long long creationTimeRaw = readLongLong(attr.n64CreationTime);
	
	streambuffer << "   Creation time: " << creationTimeRaw << endl;
	put_output_string(c, streambuffer.str());
	
}

void printLastModifiedTime(Case c, FILENAME_ATTRIBUTE attr) {
	stringstream streambuffer;
	
	unsigned long long modifiedTimeRaw = readLongLong(attr.n64AlteredTime);
	
	streambuffer << "   Last modified: " << modifiedTimeRaw << endl;
	put_output_string(c, streambuffer.str());
}

void printLastAccessedTime(Case c, FILENAME_ATTRIBUTE attr) {
	stringstream streambuffer;
	
	unsigned long long lastAccessedTimeRaw = readLongLong(attr.n64ReadTime);
	
	streambuffer << "   Last accessed: " << lastAccessedTimeRaw << endl;
	put_output_string(c, streambuffer.str());
}

void printFileNameAttribute(Case c, int disk, NTFS_RESIDENT_ATTRIBUTE attribute, unsigned long long attributeHeaderOffset) {
	stringstream streambuffer;
	
	FILENAME_ATTRIBUTE fileNameAttribute;
	
	unsigned long long fileNameAttrOffset = attributeHeaderOffset + readWord(attribute.wAttrOffset);
											
	unsigned long long attrDataLength = getAttributeDataLength(attribute);
						
	pread(disk, &fileNameAttribute, attrDataLength, fileNameAttrOffset);
	
	int fileNameLength = fileNameAttribute.uchFilenameLengthInChars.data;
	
	stringstream ss;
	
	for (int i = 0; i < fileNameLength; i++) {
		ss << fileNameAttribute.fileName[i*2];	//2 Bytes per char so desired char will be twice as far away
	}
	
	streambuffer << "Deleted file found: \n   Name: " << ss.str() << endl;
	put_output_string(c, streambuffer.str());
	streambuffer.str("");
	
	printType(c, fileNameAttribute);
	
	/*
	 * Not currently working correctly. 
	 * Apparently time values are 100nanosecond intervals since 00:00:00 Jan 1, 1960. 
	 * But the values I've been getting would mean it's currently about 00:03:00 Jan 1, 1960...
	 *
	 * printCreationTime(fileNameAttribute);
	 * printLastModifiedTime(fileNameAttribute);
	 * printLastAccessedTime(fileNameAttribute);
	 */
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
int listDeletedFilesNtfs(Case c, int disk_fd) {
	stringstream streambuffer;
	
	int deletedFileCount = 0;
	unsigned long long mftOffset;
	
	unsigned char buffer[1024];
	
	streambuffer << " Finding $MFT..." << endl;
	put_output_string(c, streambuffer.str());
	streambuffer.str("");
	mftOffset = getMftOffset(c, disk_fd);
	
	
	NTFS_MFT_FILE_ENTRY_HEADER header;
	NTFS_RESIDENT_ATTRIBUTE attribute;
	
	streambuffer << "Listing deleted files..." << endl;
	put_output_string(c, streambuffer.str());
	streambuffer.str("");
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
						
						printFileNameAttribute(c, disk_fd, attribute, attrOffset);
						put_output(c, "\n\n");
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
	
	streambuffer << "Reached end of $MFT" << endl << endl;
	put_output_string(c, streambuffer.str());
	streambuffer.str("");
	
	return deletedFileCount;
}
