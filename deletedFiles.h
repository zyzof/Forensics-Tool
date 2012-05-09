#include <cstdlib>
#include <cstdio>
#include <string>
#include <sstream>
#include <iostream>
#include <fstream>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include "server.h"




typedef struct _WORD {
	unsigned char data[2];
}  WORD, *P_WORD;

typedef struct _LONGLONG {
	unsigned char data[8];
} LONGLONG, *P_LONGLONG;

typedef struct _DWORD {
	unsigned char data[4];
} DWORD, *P_DWORD;

typedef struct _BYTE {
	unsigned char data;
} BYTE, *P_BYTE;


int readWord(WORD word);

int readByte(BYTE byte);

long long readDWord(DWORD dword);

long long readLongLong(LONGLONG longlong);

void listDeletedFiles(Case current_case, char* arg);
