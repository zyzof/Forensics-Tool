#include <wx/wx.h>
#include <wx/imaglist.h>
#include <wx/event.h>
#include <wx/toolbar.h>
#include <iostream>
#include <dirent.h>
#include <vector>
#include <string>
#include <algorithm>
#include <deque>

using namespace std;

class MyFrame : public wxFrame {
  private:
  
    const static int ID_PATHTEXTCTRL = 6000;
    const static int ID_ERRORDIALOG = 6001;
    
    wxListCtrl* iconList;
    wxPanel* panel;
    wxBoxSizer* sizer;
    wxImageList* imageList;
    wxToolBar* toolbar;
    wxTextCtrl* pathTextCtrl;
    deque<string> history;
    int historyPos;
    int historyMaxSize;
  
  public:
  
    MyFrame() : wxFrame(NULL, wxID_ANY, wxT("File Navigator"), wxPoint(50, 50), wxSize(800, 600)) {
        history = deque<string>();
        historyPos = -1;
        historyMaxSize = 30;
        
        panel = new wxPanel(this);
        sizer = new wxBoxSizer(wxVERTICAL);
        
        this->CreateToolBar(wxTB_TEXT|wxTB_HORIZONTAL|wxTB_NOICONS);
        toolbar = this->GetToolBar();
        
        wxBitmap* bitmap = new wxBitmap(wxT("folder.gif"), wxBITMAP_TYPE_GIF);
        toolbar->AddTool(wxID_UP, wxT("Up"), *bitmap);
        toolbar->AddTool(wxID_BACKWARD, wxT("Back"), *bitmap);
        toolbar->AddTool(wxID_FORWARD, wxT("Forward"), *bitmap);
        toolbar->AddSeparator();
        
        pathTextCtrl = new wxTextCtrl(toolbar, ID_PATHTEXTCTRL, wxT(""), wxDefaultPosition, wxSize(400, wxDefaultSize.GetHeight()), wxTE_PROCESS_ENTER);
        pathTextCtrl->Connect(wxEVT_COMMAND_TEXT_ENTER, wxCommandEventHandler(MyFrame::OnTextEnter), NULL, this);
        toolbar->AddControl(pathTextCtrl);
        
        toolbar->AddTool(wxID_OPEN, wxT("Go"), *bitmap);
        
        toolbar->Connect(wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(MyFrame::OnButtonSelect), NULL, this);
        toolbar->Realize();
        
        imageList = new wxImageList(16, 16);
        
        wxIcon* folderIcon  = new wxIcon();
        wxBitmap* folderBitmap = new wxBitmap(wxT("folder.gif"), wxBITMAP_TYPE_GIF);
        folderIcon->CopyFromBitmap(*folderBitmap);
        imageList->Add(*folderIcon); 
               
        wxIcon* fileIcon  = new wxIcon();
        wxBitmap* fileBitmap = new wxBitmap(wxT("file.png"), wxBITMAP_TYPE_PNG);
        fileIcon->CopyFromBitmap(*fileBitmap);
        imageList->Add(*fileIcon);
        
        
        iconList = new wxListCtrl(panel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_LIST);
        iconList->AssignImageList(imageList, wxIMAGE_LIST_SMALL);
        iconList->Connect(wxEVT_COMMAND_LIST_ITEM_ACTIVATED, wxListEventHandler(MyFrame::OnIconActivate), NULL, this);
        
        goToDir();
        populateIconList();
        
        sizer->Add(iconList, 1, wxEXPAND | wxALL, 0);
        panel->SetSizer(sizer);
    }
    
  private:
  
    void populateIconList() {
        pathTextCtrl->ChangeValue(wxString(getCurrentDir().c_str(), wxConvUTF8));
        vector<struct dirent>* dirs = getDirVector(getCurrentDir(), true);
        if (dirs == NULL) {
            cout << "err" << endl;
            while (history.size() > historyPos) {
                history.pop_back();
            }
            historyPos--;
            populateIconList();
            return;
        }
        vector<struct dirent>* files = getDirVector(getCurrentDir(), false);
        toolbar->EnableTool(wxID_UP, getCurrentDir().compare("/") != 0);
        toolbar->EnableTool(wxID_BACKWARD, historyPos > 0);
        toolbar->EnableTool(wxID_FORWARD, historyPos < history.size() - 1);
        vector<struct dirent>* vec = dirs;
        iconList->ClearAll();
        int id = 0;
        for (int i = 0; i < 2; i++) {
            sort(vec->begin(), vec->end(), MyFrame::direntCompare);
            vector<struct dirent>::iterator it;
            for (it = vec->begin(); it != vec->end(); it++) {
                string name = string(it->d_name);
                if (name.compare(".") != 0 && name.compare("..") != 0) {
                    wxListItem item;
                    item.SetId(id++);
                    item.SetText(wxString::FromAscii(name.c_str()));
                    item.SetImage(i);
                    item.SetData(it->d_type);
                    iconList->InsertItem(item);
                }
            }
            vec = files;
        }
    }
    
    void OnButtonSelect(wxCommandEvent &event) {
        int id = event.GetId();
        if (id == wxID_UP) {
            goUpDir();
            populateIconList();
        }
        else if (id == wxID_BACKWARD && historyPos > 0) {
            historyPos--;
            populateIconList();
        }
        else if (id == wxID_FORWARD && historyPos < history.size() - 1) {
            historyPos++;
            populateIconList();
        }
        else if (id == wxID_OPEN) {
            goToTextCtrlDir();
        }
    }
    
    void OnIconActivate(wxListEvent &event) {
        wxListItem item = (const wxListItem) event.GetItem();
        string str = (const char*) item.GetText().mb_str(*wxConvCurrent);
        if (item.GetImage() == 0) {
            if (getCurrentDir().compare("/") == 0) {
                goToDir("/" + str);
            }
            else {
                goToDir(getCurrentDir() + "/" + str);
            }
            populateIconList();
        }
        else if (item.GetData() == DT_REG) {
            string openthis = "xdg-open \'" + getCurrentDir() + "/" + str + "\'";
            system(openthis.c_str());
        }
    }
    
    void OnTextEnter(wxCommandEvent &event) {
        if (event.GetId() == ID_PATHTEXTCTRL) {
            goToTextCtrlDir();
        }
    }
    
    vector<struct dirent>* getDirVector(string path, bool dirs=true) {
        struct dirent* entry;
        DIR* dp = opendir(path.c_str());
        if (dp == NULL) {
            return NULL;
        }
        vector<struct dirent>* vect = new vector<struct dirent>();
        
        struct dirent entry2;
        while((entry = readdir(dp))) {
            memcpy(&entry2, entry, sizeof(struct dirent));
            if (entry2.d_type == DT_DIR && dirs) {
                vect->push_back(entry2);
            }
            else if (entry2.d_type != DT_DIR && !dirs) {
                vect->push_back(entry2);
            }
        }
        
        closedir(dp);
        return vect;
    }
    
    void goUpDir() {
        string dir = getCurrentDir();
        int pos = dir.find_last_of("/");
        if (pos == string::npos || pos == 0) {
            goToDir("/");
        }
        else {
            goToDir(dir.substr(0, pos));
        }
    }
    
    void goToDir(string dir="/") {
        int loc;
        while ((loc = dir.find("//")) != string::npos) {
            dir.erase(loc, 1);
        }
        if (dir.compare(getCurrentDir()) != 0) {
            if (historyPos == historyMaxSize - 1) {
                history.pop_front();
                historyPos--;
            }
            int numToPop = history.size() - historyPos - 1;
            for (int i = 0; i < numToPop; i++) {
                history.pop_back();
            }
            history.push_back(dir);
            historyPos++;
        }
    }
    
    string getCurrentDir() {
        if (history.size() == 0) {
            history.push_back("/");
            historyPos = 0;
        }
        return history[historyPos];
    }
    
    void showError(string msg) {
        /*const char* caption = (string("Error: ")+msg).c_str();
        wxDialog* dialog = new wxDialog(this, ID_ERRORDIALOG, wxString::FromAscii(caption), wxDefaultPosition, wxDefaultSize, wxCAPTION | wxCLOSE_BOX | wxSTAY_ON_TOP);
        wxSizer* s = new wxBoxSizer(wxVERTICAL);
        wxStaticText* text = new wxStaticText(dialog, 0, wxT("Asrfgfh"));
        sizer->Add(text);
        dialog->SetSizer(sizer);
        wxSizer* buttonSizer = dialog->CreateSeparatedButtonSizer(wxOK);
        dialog->Centre();
        dialog->Show();*/
    }
    
    void goToTextCtrlDir() {
        string newDir = (const char*) pathTextCtrl->GetValue().mb_str(*wxConvCurrent);
        if (getDirVector(newDir, true) == NULL) {
            showError("Cannot find \"" + newDir + "\".");
            pathTextCtrl->SetValue(wxString::FromAscii(getCurrentDir().c_str()));
        }
        else {
            goToDir(newDir);
        }
        populateIconList();
        string cur = (const char*) pathTextCtrl->GetValue().mb_str(*wxConvCurrent);
        pathTextCtrl->SetInsertionPoint(cur.size());
    }
    
    static bool direntCompare(struct dirent a, struct dirent b) {
        return string(a.d_name).compare(string(b.d_name)) <= 0;
    }
};

class MyApp : public wxApp {
    wxFrame* myFrame;
  
  public:
    bool OnInit() {
        /*char c;
        cin >> c;
        while (c != 's') {cin >> c;}*/
        wxInitAllImageHandlers();
        myFrame = new MyFrame();
        myFrame->Show();
        return true;
    }
    
};
 
IMPLEMENT_APP(MyApp);