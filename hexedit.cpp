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
#include "server.h"

using namespace std;

void replace(Case current_case, string filename);

void displayFile(Case current_case, string filename);

string ToHex(const string& s);

int FromHex(const string &s);

void hexEdit(Case current_case, string filename){
  fstream infile;
  infile.open(filename.c_str(), ios_base::in);
  if (!infile)
  {
	put_output(current_case, "Invalid file\n");
	return;
  }
  string command;
  string path = "Editing: " + filename + "\n";
  while (true)
  {
    put_output(current_case, path.c_str());
    put_output(current_case, "Usage: [display|replace|back]\n");
    command = get_input_string(current_case);
    if (command.compare("replace") == 0)
    {
  	replace(current_case, filename);
    }
    else if (command.compare("display") == 0)
    {
	displayFile(current_case, filename);
    }
    else if (command.compare("back") == 0)
    {
	break;
    }
    else
    {
	put_output(current_case, "Usage: [display|replace|back]\n");
    }
  }
  return;
}


void displayFile(Case current_case, string filename){
  int counter = 0;
  int lineLength = 20;
  fstream infile;
  unsigned char c;
  infile.open(filename.c_str(), ios_base::in | ios_base::binary );
  infile >> c;
  put_output(current_case, "\n      ");
  for (int i = 1; i <= lineLength; i++)
  {
    string pad = "  ";
    if (i > 9){ pad = " "; }
    stringstream ss;
    ss << dec << i << pad;
    put_output(current_case, ss.str().c_str());
  }
  put_output(current_case, "\n");
  for (int i = 0; i <= lineLength; i++)
  {
      put_output(current_case, "---");
  }
  while (!infile.eof() )
    {    
      if (counter%lineLength == 0)
      {
	put_output(current_case, "\n");
	string pad = "    ";
    	if (counter > 9){ pad = "   "; }
	if (counter > 99){ pad = "  "; }
	if (counter > 999){ pad = " "; }
	if (counter > 9999){ pad = ""; }
        stringstream ss;
	ss << dec << counter << pad << "|";
        put_output(current_case, ss.str().c_str());
      }  
      counter++;
      stringstream ss;
      ss << hex << uppercase << setfill('0') << setw(2) << (int)c << " ";
      put_output(current_case, ss.str().c_str());
      infile >> noskipws >> c;
    }  
  put_output(current_case, "\n\n");
  infile.close();
}

void replace(Case current_case, string filename){
  fstream infile;
  int pos;
  string replacement;
  put_output(current_case, "character at position: - e.g. 5\n");
  string posString = get_input_string(current_case);
  pos = atoi(posString.c_str());
  put_output(current_case, "value: - e.g. 7D\n");
  replacement = get_input_string(current_case);
  

  //Convert it to a char for simplicity
  char h = FromHex(replacement);
  //Open the file - must have input, output and binary flags.
  infile.open(filename.c_str(), ios_base::in | ios_base::out | ios_base::binary );
  //Move the put cursor to pos-1 (pos was offset by the way we displayed the 
  //file on the screen.
  infile.seekp(pos-1);
  char c = infile.peek();
  stringstream ss;
  //Prepare printout
  ss << "Replacing char at pos " << posString << ": " << hex << c << " --> " << replacement;

  //Put the character at that position
  infile.put(h);
  //Close the file
  infile.close();
  put_output(current_case, "Done\n");

  log_text(current_case, ss.str().c_str());
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

