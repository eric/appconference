//----------------------------------------------------------------------------------------
// Name:        directory.cc
// Purpose:     dialog box to manage directory
// Author:      Michael Van Donselaar
// Modified by:
// Created:     2003
// Copyright:   (c) Michael Van Donselaar ( michael@vandonselaar.org )
// Licence:     GPL
//----------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------
// GCC implementation
//----------------------------------------------------------------------------------------

#if defined(__GNUG__) && ! defined(__APPLE__)
    #pragma implementation "directory.h"
#endif

//----------------------------------------------------------------------------------------
// Standard wxWindows headers
//----------------------------------------------------------------------------------------

// For compilers that support precompilation, includes "wx/wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

// For all others, include the necessary headers (this file is usually all you
// need because it includes almost all "standard" wxWindows headers)
#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

//----------------------------------------------------------------------------------------
// Header of this .cpp file
//----------------------------------------------------------------------------------------

#include "directory.h"  

//----------------------------------------------------------------------------------------
// Remaining headers
// ---------------------------------------------------------------------------------------

#include "app.h"
#include "frame.h"
#include "main.h"
#include "dial.h"

//----------------------------------------------------------------------------------------
// Event table: connect the events to the handler functions to process them
//----------------------------------------------------------------------------------------
BEGIN_EVENT_TABLE(DirectoryDialog, wxDialog)
    EVT_BUTTON(XRCID("AddOTList"),               DirectoryDialog::OnAddOTList)
    EVT_BUTTON(XRCID("AddPhoneList"),            DirectoryDialog::OnAddPhoneList)
    EVT_BUTTON(XRCID("AddServerList"),           DirectoryDialog::OnAddServerList)
    EVT_BUTTON(XRCID("EditOTList"),              DirectoryDialog::OnAddOTList)
    EVT_BUTTON(XRCID("EditPhoneList"),           DirectoryDialog::OnAddPhoneList)
    EVT_BUTTON(XRCID("EditServerList"),          DirectoryDialog::OnAddServerList)
    EVT_BUTTON(XRCID("RemoveOTList"),            DirectoryDialog::OnRemoveOTList)
    EVT_BUTTON(XRCID("RemovePhoneList"),         DirectoryDialog::OnRemovePhoneList)
    EVT_BUTTON(XRCID("RemoveServerList"),        DirectoryDialog::OnRemoveServerList)
    EVT_BUTTON(XRCID("DialOTList"),              DirectoryDialog::OnDialOTList)
    EVT_BUTTON(XRCID("DialPhoneList"),           DirectoryDialog::OnDialPhoneList)

    EVT_LIST_ITEM_ACTIVATED(XRCID("OTList"),     DirectoryDialog::OnDialOTList)
    EVT_LIST_ITEM_ACTIVATED(XRCID("PhoneList"),  DirectoryDialog::OnDialPhoneList)
END_EVENT_TABLE()


//----------------------------------------------------------------------------------------
// Public methods
//----------------------------------------------------------------------------------------

DirectoryDialog::DirectoryDialog( wxWindow* parent )
{
    wxConfig  *config = new wxConfig("iaxComm");
    wxString   str;
    long       dummy;
    bool       bCont;
    wxListItem item;
    long       i;

    wxXmlResource::Get()->LoadDialog(this, parent, wxT("Directory"));

    //----Reach in for our controls-----------------------------------------------------
    DirectoryNotebook  = XRCCTRL(*this, "DirectoryNotebook", wxNotebook);

    // One Touch tab
    OTList       = XRCCTRL(*this, "OTList",       wxListCtrl);

    // Phone Book tab
    PhoneList    = XRCCTRL(*this, "PhoneList",    wxListCtrl);

    // Server tab
    ServerList   = XRCCTRL(*this, "ServerList",   wxListCtrl);

    //----Populate OTList listctrl--------------------------------------------------
    OTList->InsertColumn( 0, _("No"),         wxLIST_FORMAT_LEFT,  40);
    OTList->InsertColumn( 1, _("Name"),       wxLIST_FORMAT_LEFT, 100);
    OTList->InsertColumn( 2, _("Extension"),  wxLIST_FORMAT_LEFT, 100);

    config->SetPath("/OT");
    bCont = config->GetFirstGroup(str, dummy);
    while ( bCont ) {
        OTList->InsertItem(i, str);
        bCont = config->GetNextGroup(str, dummy);
        i++;
    }

    // I don't know why, but I can't seem to add these during the first pass
    for(i=0;i<OTList->GetItemCount();i++) {
        OTList->SetItem(i, 1, config->Read(OTList->GetItemText(i) +
                                              "/Name" ,""));
        OTList->SetItem(i, 2, config->Read(OTList->GetItemText(i) +
                                              "/Extension" ,""));
    }

    OTList->SetColumnWidth(0, -1);
    OTList->SetColumnWidth(1, -1);
    OTList->SetColumnWidth(2, -1);

    if(OTList->GetColumnWidth(0) < 40)
        OTList->SetColumnWidth(0,  40);
    if(OTList->GetColumnWidth(1) < 100)
        OTList->SetColumnWidth(1,  100);
    if(OTList->GetColumnWidth(2) < 100)
        OTList->SetColumnWidth(2,  100);


    //----Populate PhoneList listctrl--------------------------------------------------
    PhoneList->InsertColumn( 0, _("Name"),       wxLIST_FORMAT_LEFT, 100);
    PhoneList->InsertColumn( 1, _("Extension"),  wxLIST_FORMAT_LEFT, 100);

    config->SetPath("/PhoneBook");
    bCont = config->GetFirstGroup(str, dummy);
    while ( bCont ) {
        PhoneList->InsertItem(i, str);
        bCont = config->GetNextGroup(str, dummy);
        i++;
    }

    // I don't know why, but I can't seem to add these during the first pass
    for(i=0;i<PhoneList->GetItemCount();i++) {
        PhoneList->SetItem(i, 1, config->Read(PhoneList->GetItemText(i) +
                                              "/Extension" ,""));
    }

    PhoneList->SetColumnWidth(0, -1);
    PhoneList->SetColumnWidth(1, -1);

    if(PhoneList->GetColumnWidth(0) < 100)
        PhoneList->SetColumnWidth(0,  100);
    if(PhoneList->GetColumnWidth(1) < 100)
        PhoneList->SetColumnWidth(1,  100);



    //----Populate ServerList listctrl--------------------------------------------------
    ServerList->InsertColumn( 0, _("Name"),     wxLIST_FORMAT_LEFT, 100);
    ServerList->InsertColumn( 1, _("Host"),     wxLIST_FORMAT_LEFT, 100);
    ServerList->InsertColumn( 2, _("Username"), wxLIST_FORMAT_LEFT, 100);

    config->SetPath("/Servers");
    bCont = config->GetFirstGroup(str, dummy);
    while ( bCont ) {
        ServerList->InsertItem(i, str);
        bCont = config->GetNextGroup(str, dummy);
        i++;
    }

    // I don't know why, but I can't seem to add these during the first pass
    for(i=0;i<ServerList->GetItemCount();i++) {
        ServerList->SetItem(i, 1, config->Read(ServerList->GetItemText(i) +
                                              "/Host" ,""));
        ServerList->SetItem(i, 2, config->Read(ServerList->GetItemText(i) +
                                              "/Username" ,""));
    }

    ServerList->SetColumnWidth(0, -1);
    ServerList->SetColumnWidth(1, -1);
    ServerList->SetColumnWidth(2, -1);

    if(ServerList->GetColumnWidth(0) < 100)
        ServerList->SetColumnWidth(0,  100);
    if(ServerList->GetColumnWidth(1) < 100)
        ServerList->SetColumnWidth(1,  100);
    if(ServerList->GetColumnWidth(2) < 100)
        ServerList->SetColumnWidth(2,  100);
}


//----------------------------------------------------------------------------------------


BEGIN_EVENT_TABLE(AddOTListDialog, wxDialog)
    EVT_BUTTON(XRCID("Add"),            AddOTListDialog::OnAdd)
END_EVENT_TABLE()

AddOTListDialog::AddOTListDialog( wxWindow* parent, wxString Selection )
{
    wxConfig  *config = new wxConfig("iaxComm");
    wxXmlResource::Get()->LoadDialog(this, parent, wxT("AddOT"));

    //----Reach in for our controls-----------------------------------------------------
    OTNo         = XRCCTRL(*this, "OTNo",         wxSpinCtrl);
    Name         = XRCCTRL(*this, "Name",         wxTextCtrl);
    Extension    = XRCCTRL(*this, "Extension",    wxTextCtrl);

    if(!Selection.IsEmpty()) {
        SetTitle(_("Edit " + Selection));
        OTNo->SetValue(Selection);
        config->SetPath("/OT/" + Selection);
        Name->SetValue(config->Read("Name" ,""));
        Extension->SetValue(config->Read("Extension" ,""));
    }
}

//----------------------------------------------------------------------------------------

BEGIN_EVENT_TABLE(AddPhoneListDialog, wxDialog)
    EVT_BUTTON(XRCID("Add"),            AddPhoneListDialog::OnAdd)
END_EVENT_TABLE()

AddPhoneListDialog::AddPhoneListDialog( wxWindow* parent, wxString Selection )
{
    wxConfig  *config = new wxConfig("iaxComm");
    wxXmlResource::Get()->LoadDialog(this, parent, wxT("AddPhoneList"));

    //----Reach in for our controls-----------------------------------------------------
    Name         = XRCCTRL(*this, "Name",         wxTextCtrl);
    Extension    = XRCCTRL(*this, "Extension",    wxTextCtrl);

    if(!Selection.IsEmpty()) {
        SetTitle(_("Edit " + Selection));
        Name->SetValue(Selection);
        config->SetPath("/PhoneBook/" + Selection);
        Extension->SetValue(config->Read("Extension" ,""));
    }
}

//----------------------------------------------------------------------------------------

BEGIN_EVENT_TABLE(AddServerDialog, wxDialog)
    EVT_BUTTON(XRCID("Add"),            AddServerDialog::OnAdd)
END_EVENT_TABLE()

AddServerDialog::AddServerDialog( wxWindow* parent, wxString Selection )
{
    wxConfig  *config = new wxConfig("iaxComm");
    wxXmlResource::Get()->LoadDialog(this, parent, wxT("AddServer"));

    //----Reach in for our controls-----------------------------------------------------
    ServerName   = XRCCTRL(*this, "ServerName",   wxTextCtrl);
    HostName     = XRCCTRL(*this, "HostName",     wxTextCtrl);
    UserName     = XRCCTRL(*this, "UserName",     wxTextCtrl);
    Password     = XRCCTRL(*this, "Password",     wxTextCtrl);
    Confirm      = XRCCTRL(*this, "Confirm",      wxTextCtrl);

    if(!Selection.IsEmpty()) {
        SetTitle(_("Edit " + Selection));
        ServerName->SetValue(Selection);
        config->SetPath("/Servers/" + Selection);
        HostName->SetValue(config->Read("Host" ,""));
        UserName->SetValue(config->Read("Username" ,""));
        Password->SetValue(config->Read("Password" ,""));
        Confirm->SetValue(config->Read("Password" ,""));
    }
}

//----------------------------------------------------------------------------------------
// Private methods
//----------------------------------------------------------------------------------------

void DirectoryDialog::OnAddOTList(wxCommandEvent &event)
{
    long     sel = -1;
    wxString val;

    if(event.GetId() == XRCID("EditOTList")) {
        if((sel = OTList->GetNextItem(sel,wxLIST_NEXT_ALL,wxLIST_STATE_SELECTED)) >= 0)
            val = OTList->GetItemText(sel);
    }
    AddOTListDialog dialog(this, val);
    dialog.ShowModal();
}

void DirectoryDialog::OnAddPhoneList(wxCommandEvent &event)
{
    long     sel = -1;
    wxString val;

    if(event.GetId() == XRCID("EditPhoneList")) {
        if((sel = PhoneList->GetNextItem(sel,wxLIST_NEXT_ALL,wxLIST_STATE_SELECTED)) >= 0)
            val = PhoneList->GetItemText(sel);
    }
    AddPhoneListDialog dialog(this, val);
    dialog.ShowModal();
}

void DirectoryDialog::OnAddServerList(wxCommandEvent &event)
{
    long     sel = -1;
    wxString val;

    if(event.GetId() == XRCID("EditServerList")) {
        if((sel = ServerList->GetNextItem(sel,wxLIST_NEXT_ALL,wxLIST_STATE_SELECTED)) >= 0)
            val = ServerList->GetItemText(sel);
    }
    AddServerDialog dialog(this, val);
    dialog.ShowModal();
}

//----------------------------------------------------------------------------------------

void DirectoryDialog::OnDialOTList(wxCommandEvent &event)
{
    wxConfig  *config = new wxConfig("iaxComm");
    long       sel = -1;
    wxString   DialString;

    sel=OTList->GetNextItem(sel,wxLIST_NEXT_ALL,wxLIST_STATE_SELECTED);
    if(sel >= 0) {
        DialString = config->Read("/OT/" + OTList->GetItemText(sel) +
                                  "/Extension", "");

        // A DialString in quotes means look up name in phone book
        if(DialString.StartsWith("\"")) {
            DialString = DialString.Mid(1, DialString.Len() -2);
            DialString = config->Read("/PhoneBook/" + DialString + "/Extension", "");
        }
        Dial(DialString);
    }
}

void DirectoryDialog::OnDialPhoneList(wxCommandEvent &event)
{
    wxConfig  *config = new wxConfig("iaxComm");
    long       sel = -1;
    wxString   DialString;

    sel=PhoneList->GetNextItem(sel,wxLIST_NEXT_ALL,wxLIST_STATE_SELECTED);
    if(sel >= 0) {
        DialString = config->Read("/PhoneBook/" + PhoneList->GetItemText(sel) +
                                  "/Extension", "");
        Dial(DialString);
    }
}

//----------------------------------------------------------------------------------------

void DirectoryDialog::OnRemoveOTList(wxCommandEvent &event)
{
    wxConfig  *config = new wxConfig("iaxComm");
    long       sel = -1;
    int        isOK;

    if((sel=OTList->GetNextItem(sel,wxLIST_NEXT_ALL,wxLIST_STATE_SELECTED)) >= 0) {
        isOK = wxMessageBox("Really remove One Touch " + OTList->GetItemText(sel) + "?",
                            "Remove from One Touch List", wxOK|wxCANCEL|wxCENTRE);
        if(isOK == wxOK) {
            config->DeleteGroup("/OT/"+OTList->GetItemText(sel));
            OTList->DeleteItem(sel);
        }
    }
    delete config;
}

void DirectoryDialog::OnRemovePhoneList(wxCommandEvent &event)
{
    wxConfig  *config = new wxConfig("iaxComm");
    long       sel = -1;
    int        isOK;

    if((sel=PhoneList->GetNextItem(sel,wxLIST_NEXT_ALL,wxLIST_STATE_SELECTED)) >= 0) {
        isOK = wxMessageBox("Really remove " + PhoneList->GetItemText(sel) + "?",
                            "Remove from Phone Book", wxOK|wxCANCEL|wxCENTRE);
        if(isOK == wxOK) {
            config->DeleteGroup("/PhoneBook/"+PhoneList->GetItemText(sel));
            PhoneList->DeleteItem(sel);
        }
    }
    delete config;
}

void DirectoryDialog::OnRemoveServerList(wxCommandEvent &event)
{
    wxConfig  *config = new wxConfig("iaxComm");
    long       sel = -1;
    int        isOK;

    if((sel=ServerList->GetNextItem(sel,wxLIST_NEXT_ALL,wxLIST_STATE_SELECTED)) >= 0) {
        isOK = wxMessageBox("Really remove " + ServerList->GetItemText(sel) + "?",
                            "Remove from Server List", wxOK|wxCANCEL|wxCENTRE);
        if(isOK == wxOK) {
            config->DeleteGroup("/Servers/"+ServerList->GetItemText(sel));
            ServerList->DeleteItem(sel);
        }
    }
    delete config;
}

//----------------------------------------------------------------------------------------

void AddOTListDialog::OnAdd(wxCommandEvent &event)
{
    wxConfig  *config = new wxConfig("iaxComm");
    wxString   Path;

    Path.Printf("/OT/%d", OTNo->GetValue());
    config->SetPath(Path);
    config->Write("Name",      Name->GetValue());
    config->Write("Extension", Extension->GetValue());
    delete config;

    Name->SetValue("");
    Extension->SetValue("");
}

//----------------------------------------------------------------------------------------

void AddPhoneListDialog::OnAdd(wxCommandEvent &event)
{
    wxConfig  *config = new wxConfig("iaxComm");

    config->SetPath("/PhoneBook/" + Name->GetValue());
    config->Write("Extension", Extension->GetValue());
    delete config;

    Name->SetValue("");
    Extension->SetValue("");
}

//----------------------------------------------------------------------------------------

void AddServerDialog::OnAdd(wxCommandEvent &event)
{
    wxConfig  *config = new wxConfig("iaxComm");

    if(!Password->GetValue().IsSameAs(Confirm->GetValue())) {
        wxMessageBox(_("Try Again"),
                     _("Password Mismatch"),
                       wxICON_INFORMATION);
        return;
    }
    config->SetPath("/Servers/" + ServerName->GetValue());
    config->Write("Host",     HostName->GetValue());
    config->Write("Username", UserName->GetValue());
    config->Write("Password", Password->GetValue());
    delete config;

    ServerName->SetValue("");
    HostName->SetValue("");
    UserName->SetValue("");
    HostName->SetValue("");
    Confirm->SetValue("");
}

