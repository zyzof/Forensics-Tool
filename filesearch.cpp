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
#include "filesearch.h"
#include "hexedit.h"

using namespace std;

int searchFile(string path, vector<string> keywords, bool hex);

int search(Case current_case, bool hexInput);

void displayResults(Case current_case, vector<string> results);

string getPath(vector<string> results, int selection);
 
int traverse(string path, vector<string> keywords, vector<string> &results, bool hex) {
  struct dirent *entry;
  DIR *dp;
  string p;
  string name;


  dp = opendir(path.c_str());
  if (dp == NULL) {
    perror("opendir");
    return -1;
  }

  while((entry = readdir(dp)))
    {
	name = entry->d_name;
	/*This line stops loopback to directories and only 
	looks in directories and files (ignoring pipes and sockets, 
	because these break the search)*/
	if (name.compare("..") != 0 && name.compare(".") != 0 && (entry->d_type == DT_DIR || entry->d_type == DT_REG))
	{
		p = path + "/" + entry->d_name;
		//If the file or directory name contains a keyword, add it to the results list.
		//Don't bother checking for a hex string.
		for(std::vector<int>::size_type i = 0; i < keywords.size(); i++)
		{	
		    if (!hex && name.find(keywords[i]) == 0)
		    {
			results.push_back(p);
		    }
		}
		//If it's a directory, we'll want to traverse it too. So go down into it recursively.
		if (entry->d_type == DT_DIR)
		{	
			traverse(p, keywords, results, hex);
		}
		//Otherwise it must be a regular file, so we'll try to search it.
		else
		{
			if (searchFile(p, keywords, hex) == 0)
			{
				results.push_back(p);
			}	
		}
	}
    }
  closedir(dp);
  return 0;
}

int searchFile(string path, vector<string> keywords, bool hex){
    string line;
    ifstream infile;
    infile.open(path.c_str());
    size_t found;
    while(getline(infile,line)) 
    {
	
	//If we're doing a hex search, convert the line to hex
	if (hex)
	{
	  line = ToHex(line);
	}
	else
	{
		int length = line.length();
  		for(int i=0; i!=length; ++i)
 		{
   	 		line[i] = tolower(line[i]);
  		}
	}
	//String search
	for(std::vector<string>::iterator it = keywords.begin(); it != keywords.end(); ++it) 
	{	
	    found=line.find(*it);
  	    if (found!=string::npos)
            {            
	        //Found, return 0.
	        return 0;
            }	   
	}
	
    }
    infile.close();
    //Not found, return 1.
    return 1;
}

int search(Case current_case, bool hexInput){
  vector<string> results;
  string dir;
  string findme;
  char buffer[256] = { '\0' }; 
  vector<string> keywords;
  
  cout << "Enter the directory to search" << endl;
  cin >> dir;
  //Hardcoded for testing 
  dir = "/home/cosc/student/jam296/Desktop";
  
  if (hexInput)
  {
      cout << "Enter Hex String (e.g. 66757A7A)" << endl;
      cin.ignore();
      gets(buffer);
      findme = buffer;
      cout << "Searching for \"" << findme << "\" in " << dir << endl;
  }
  else
  {
    	cout << "Enter keyword(s) [separated by spaces]" << endl;
        cin.ignore();
    	gets(buffer);
	
	int length = 256;
  	for(int i=0; i!=length; ++i)
 	{
   		buffer[i] = tolower(buffer[i]);
  	}
	findme = buffer;
	cout << "Searching for \"" << findme << "\" in " << dir << endl;
  }

  log_text(current_case, "Searching for");
  istringstream iss(findme);
  do
  {
    string sub;
    iss >> sub;
    if (!sub.empty()){
    keywords.push_back(sub);
    log_text(current_case, sub.c_str());
    }
  } while (iss);
  log_text(current_case, dir.c_str());
  
  cout << endl;

  traverse(dir, keywords, results, hexInput);
  displayResults(current_case, results);
  return 0;
}

void displayResults(Case current_case, vector<string> results){
  string command;
  string selectionString;
  int selection;
  vector<string>::iterator it;
  string openthis;
  string path;    
  int second;
  cout << "Finished searching" << endl;
  cout << results.size() << " result(s) :" << endl;
  if (results.size() > 0){
  while (true)
  {

    for (it=results.begin(); it!=results.end(); ++it)
    {
       cout << dec << distance(results.begin(), it) + 1 << ": " << *it << endl;
    }

    cout << "Usage: [open #|hexedit #|back] " << endl;
    cin >> command;
    if (command.compare("back") == 0){return;}
    cin >> selectionString;
    selection = atoi(selectionString.c_str());

    while (selection > results.size() || selection <= 0)
    {
      cout << "Selection out of bounds." << endl << "Try another number" << endl;
      cin >> selection;
    }

    if (command.compare("open") == 0)
    {
      path = getPath(results, selection);
      /*This creates a string instruction that will be run by the system.
      the command xdg-open opens the file in the default program. 
      The clumsy concatenation is to add apostrophes around the path,
      which prevents it from rejecting the path due to spaces and other
      characters.*/
      openthis = "xdg-open " + path;
      system(openthis.c_str()); 
      log_text(current_case, "Opening: ");
      log_text(current_case, path.c_str());
    }

    else if (command.compare("hexedit") == 0)
    {
      hexEdit(current_case, results.at(selection-1));
      log_text(current_case, "Hexediting: ");
      log_text(current_case, path.c_str());
    }
  }
}
  return;
}

string getPath(vector<string> results, int selection){
  //Adjust for the 0 index
  selection-=1;
  string path = "\'" + results.at(selection) + "\'";
  return path;
}



