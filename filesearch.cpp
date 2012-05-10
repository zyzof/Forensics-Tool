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
#include "constants.h"
#include "server.h"

using namespace std;

int searchFile(string path, vector<string> keywords, bool hex);

int search(Case current_case, bool hexInput);

void displayResults(Case current_case, vector<string> results);

string getPath(vector<string> results, int selection);

int getSelection(Case current_case, int size);
 
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
		infile.close();
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
  put_output(current_case, "Enter the directory to search\n");
  dir = get_input_string(current_case);
  
  if (hexInput)
  {
      put_output(current_case, "Enter Hex String (e.g. 66757A7A)\n");
      get_input(current_case, buffer);
      findme = string(buffer);
  }
  else
  {
    	put_output(current_case, "Enter keyword(s) [separated by spaces]\n");
        
    	get_input(current_case, buffer);
	
	int length = 256;
  	for(int i=0; i!=length; ++i)
 	{
   		buffer[i] = tolower(buffer[i]);
  	}
	findme = string(buffer);
  }

  string s = "Searching for " + findme + " in " + dir;
  put_output(current_case, s.c_str());
  
  istringstream iss(findme);
  stringstream words;
  do
  {
    string sub;
    iss >> sub;
    if (!sub.empty())
    {
        keywords.push_back(sub);
        words << sub;
    }
  } while (iss);
  string dsearch = "Searching : " + dir + " for:";
  log_text(current_case, dsearch.c_str());
  log_text(current_case, words.str().c_str());
  
  put_output(current_case, "\n");

  traverse(dir, keywords, results, hexInput);
  displayResults(current_case, results);
  return 0;
}

void displayResults(Case current_case, vector<string> results){
  string command;
  vector<string>::iterator it;
  string openthis;
  string path;    
  int second;
  char buff[BUFFER_SIZE] = { '\0' };
  put_output(current_case, "Finished searching\n");
  string s = results.size() + " result(s) :\n";
  put_output(current_case, s.c_str());
  if (results.size() > 0){
  while (true)
  {
    //Print out the results.
    for (it=results.begin(); it!=results.end(); ++it)
    {
       stringstream out;
       out << dec << distance(results.begin(), it) + 1 << ": " << *it << endl;
       string s = out.str();
       put_output(current_case, s.c_str());
    }

    put_output(current_case, "Usage: [open|hexedit|back]\n");
    
    command = get_input_string(current_case);
    if (command.compare("back") == 0){return;}
    
    if (command.compare("open") == 0)
    {
      int selection = getSelection(current_case, results.size());
      path = getPath(results, selection);

      if (current_case.local)
      {
        /*This creates a string instruction that will be run by the system.
        the command xdg-open opens the file in the default program. 
        The clumsy concatenation is to add apostrophes around the path,
        which prevents it from rejecting the path due to spaces and other
        characters.*/
        openthis = "xdg-open " + path;
        system(openthis.c_str()); 
        string out = "Opening: " + path;
        log_text(current_case, out.c_str());
      }
      else
      {
        put_output(current_case, "Opening a file remotely will cause it to stream to the console. \n Are you sure you wish to do this? y/N");
        char answer[2]  = {'\0'};
	get_input(current_case, answer);
	if (tolower(answer[0]) == 'y')
	{
          string line;
          fstream infile;
	  path = path.substr(1, path.length()-2);
          infile.open(path.c_str(), ios_base::in | ios_base::out | ios_base::binary );
          cout << path << endl;
          while (getline(infile,line))
          {
             put_output(current_case, line.c_str());
          }
          infile.close();
	put_output(current_case, "\n");
	}
      }
    }

    else if (command.compare("hexedit") == 0)
    {
	int selection = getSelection(current_case, results.size());
        path = getPath(results, selection);
        hexEdit(current_case, results.at(selection-1));
        string out = "Hexediting: " + path;
        log_text(current_case, out.c_str());
    }
  }
}
  return;
}

int getSelection(Case current_case, int size){
    string selectionString;
    int selection;
    put_output(current_case, "Enter the number you wish to open\n");
    selectionString = get_input_string(current_case);
    selection = atoi(selectionString.c_str());

    while (selection > size || selection <= 0)
    {
      put_output(current_case, "Selection out of bounds.\nTry another number\n");
      selectionString = get_input_string(current_case);;
      selection = atoi(selectionString.c_str());
    }
    return selection;
}

string getPath(vector<string> results, int selection){
  //Adjust to the 0 index
  selection-=1;
  string path = "\'" + results.at(selection) + "\'";
  return path;
}



