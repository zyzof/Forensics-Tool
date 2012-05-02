#include <dirent.h>
#include <string.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <sstream>
#include <iomanip>
#include "case.h"
#include "hexedit.h"

using namespace std;

void replace(Case current_case, string filename);

void displayFile(string filename);

string ToHex(const string& s);

int FromHex(const string &s);

void hexEdit(Case current_case, string filename){
  fstream infile;
  infile.open(filename.c_str(), ios_base::in);
  if (!infile)
  {
	cout << "Invalid file" << endl;
	return;
  }
  string command;
  
  while (true)
  {
    cout << "Editing: " << filename << endl;
    cout << "Usage: [display|replace|back]" << endl;
    cin >> command;
    if (command.compare("replace") == 0)
    {
  	replace(current_case, filename);
    }
    else if (command.compare("display") == 0)
    {
	displayFile(filename);
    }
    else if (command.compare("back") == 0)
    {
	break;
    }
    else
    {
	cout << "Usage: [display|replace|back]" << endl;
    }
  }
  return;
}


void displayFile(string filename){
  int counter = 0;
  int lineLength = 20;
  fstream infile;
  unsigned char c;
  infile.open(filename.c_str(), ios_base::in | ios_base::binary );
  infile >> c;
  cout << endl << "      ";
  for (int i = 1; i <= lineLength; i++)
  {
    string pad = "  ";
    if (i > 9){ pad = " "; }
    cout << dec << i << pad;
  }
  cout << endl;
  for (int i = 0; i <= lineLength; i++)
  {
      cout << "---";
  }
  while (!infile.eof() )
    {    
      if (counter%lineLength == 0)
      {
	cout << endl;
	string pad = "    ";
    	if (counter > 9){ pad = "   "; }
	if (counter > 99){ pad = "  "; }
	if (counter > 999){ pad = " "; }
	if (counter > 9999){ pad = ""; }
	cout << dec << counter << pad << "|";
      }  
      counter++;
      cout << hex << uppercase << setfill('0') << setw(2) << (int)c << " ";
      infile >> noskipws >> c;
    }  
  cout << endl << endl;
  infile.close();
}

void replace(Case current_case, string filename){
  fstream infile;
  int pos;
  string replacement;
  cout << "[pos] [replacement] - e.g. 5 7d" << endl;
  cin >> pos;
  cin >> replacement;

  //Convert it to a char for simplicity
  char h = FromHex(replacement);
  //Open the file - must have input, output and binary flags.
  infile.open(filename.c_str(), ios_base::in | ios_base::out | ios_base::binary );
  //Move the put cursor to pos-1 (pos was offset by the way we displayed the 
  //file on the screen.
  infile.seekp(pos-1);
  //Put the character at that position
  infile.put(h);
  //Close the file
  infile.close();
  cout << "Done" << endl;

  log_text(current_case, "Replacing: ");
  string s = pos+ " with " + replacement;
  log_text(current_case, s.c_str());
  }

int FromHex(const string &s) { 
   return strtoul(s.c_str(), NULL, 16); 
}

string ToHex(const string& s)
{
    ostringstream out;
    for (int i = 0; i < s.length(); ++i)
    {
      out << hex << uppercase << noskipws <<(int)s[i];
    } 
   return out.str();
}

