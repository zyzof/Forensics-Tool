#include <stdio.h>
#include "case.h"
#include "deletedFilesFat32.h"
#include "deletedFilesNtfs.h"
#include "deletedFiles.h"

using namespace std;

int readWord(WORD word) {
	int value = word.data[0] + (word.data[1] << 8);
	return value;
}

int readByte(BYTE byte) {
	int value = byte.data;
	return value;
}

long long readDWord(DWORD dword) {
	long long value = dword.data[0] + (dword.data[1] << 8) + (dword.data[2] << 16) + (dword.data[3] << 24);
	return value;
}

long long readLongLong(LONGLONG longlong) {
	long long value = 0;
	
	for (int i = 0; i < sizeof(longlong); i++) {
		value += longlong.data[i] << (i * 8);
	}
	
	return value;
}


void listDeletedFiles(Case current_case) {
	string disk;
	int disk_fd;
	int deletedFileCount = 0;
	stringstream log;
	
	
	cout << "Enter the name of the drive you want to search for deleted files on: " << endl;
	cin >> disk;	
	
	disk_fd = open(disk.c_str(), O_RDONLY | O_NONBLOCK);
	
	if (disk_fd == -1) {
		cout << "Could not open " << disk << " for reading." << endl;
		return;
	}
	
	cout << "Searching for deleted files on " << disk << "..." << endl << endl;
	
	LONGLONG oemId;
	pread(disk_fd, &oemId, sizeof(LONGLONG), 0x03);
	
	char buffer[4];
	for (int i = 0; i < 4; i++) {
		buffer[i] = (char)oemId.data[i];
	}
	
	log << "Listed deleted files on ";
	if (strncmp(buffer, "NTFS", 4) == 0) {
		log << " NTFS drive ";
		deletedFileCount += listDeletedFilesNtfs(disk_fd);
	} else {
		log << " FAT32 drive ";
		deletedFileCount += listDeletedFilesFat32(disk_fd);
	}
	log << disk << ". ";
	log << deletedFileCount << " deleted files found.";
	
	string logString = log.str();
	
	log_text(current_case, logString.c_str());
}
