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
    EVT_BUTTON(XRCID("AddAccountList"),          DirectoryDialog::OnAddAccountList)
    EVT_BUTTON(XRCID("EditOTList"),              DirectoryDialog::OnAddOTList)
    EVT_BUTTON(XRCID("EditPhoneList"),           DirectoryDialog::OnAddPhoneList)
    EVT_BUTTON(XRCID("EditAccountList"),         DirectoryDialog::OnAddAccountList)
    EVT_BUTTON(XRCID("RemoveOTList"),            DirectoryDialog::OnRemoveOTList)
    EVT_BUTTON(XRCID("RemovePhoneList"),         DirectoryDialog::OnRemovePhoneList)
    EVT_BUTTON(XRCID("RemoveAccountList"),       DirectoryDialog::OnRemoveAccountList)
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
    wxXmlResource::Get()->LoadDialog(this, parent, wxT("Directory"));

    //----Reach in for our controls-----------------------------------------------------
    DirectoryNotebook  = XRCCTRL(*this, "DirectoryNotebook", wxNotebook);

    OTList       = XRCCTRL(*this, "OTList",      wxListCtrl);
    PhoneList    = XRCCTRL(*this, "PhoneList",   wxListCtrl);
    AccountList  = XRCCTRL(*this, "AccountList", wxListCtrl);

    OTList->InsertColumn(     0, _("No"),        wxLIST_FORMAT_LEFT,  40);
    OTList->InsertColumn(     1, _("Name"),      wxLIST_FORMAT_LEFT, 100);
    OTList->InsertColumn(     2, _("Extension"), wxLIST_FORMAT_LEFT, 100);

    PhoneList->InsertColumn(  0, _("Name"),      wxLIST_FORMAT_LEFT, 100);
    PhoneList->InsertColumn(  1, _("Extension"), wxLIST_FORMAT_LEFT, 100);

    AccountList->InsertColumn(0, _("Name"),      wxLIST_FORMAT_LEFT, 100);
    AccountList->InsertColumn(1, _("Host"),      wxLIST_FORMAT_LEFT, 100);
    AccountList->InsertColumn(2, _("Username"),  wxLIST_FORMAT_LEFT, 100);

    Show();
}

void DirectoryDialog::Show( void )
{
    wxConfig  *config = new wxConfig("iaxComm");
    wxString   str;
    long       dummy;
    bool       bCont;
    wxListItem item;
    long       i;

    //----Populate OTList listctrl--------------------------------------------------
    config->SetPath("/OT");
    i = 0;
    OTList->DeleteAllItems();
    bCont = config->GetFirstGroup(str, dummy);
    while ( bCont ) {
        OTList->InsertItem(i, str);
        OTList->SetItem(i, 1, config->Read(OTList->GetItemText(i) + "/Name" ,""));
        OTList->SetItem(i, 2, config->Read(OTList->GetItemText(i) + "/Extension" ,""));
        bCont = config->GetNextGroup(str, dummy);
        i++;
    }

    OTList->SetColumnWidth(0, -1);
    OTList->SetColumnWidth(1, -1);
    OTList->SetColumnWidth(2, -1);
    if(OTList->GetColumnWidth(0) <  40)        OTList->SetColumnWidth(0,   40);
    if(OTList->GetColumnWidth(1) < 100)        OTList->SetColumnWidth(1,  100);
    if(OTList->GetColumnWidth(2) < 100)        OTList->SetColumnWidth(2,  100);

    //----Populate PhoneList listctrl--------------------------------------------------
    config->SetPath("/PhoneBook");
    PhoneList->DeleteAllItems();
    i=0;
    bCont = config->GetFirstGroup(str, dummy);
    while ( bCont ) {
        PhoneList->InsertItem(i, str);
        PhoneList->SetItem(i, 1, config->Read(PhoneList->GetItemText(i) + "/Extension" ,""));
        bCont = config->GetNextGroup(str, dummy);
        i++;
    }

    PhoneList->SetColumnWidth(0, -1);
    PhoneList->SetColumnWidth(1, -1);
    if(PhoneList->GetColumnWidth(0) < 100)        PhoneList->SetColumnWidth(0,  100);
    if(PhoneList->GetColumnWidth(1) < 100)        PhoneList->SetColumnWidth(1,  100);

    //----Populate AccountList listctrl--------------------------------------------------
    config->SetPath("/Accounts");
    AccountList->DeleteAllItems();
    i = 0;
    bCont = config->GetFirstGroup(str, dummy);
    while ( bCont ) {
        AccountList->InsertItem(i, str);
        AccountList->SetItem(i, 1, config->Read(AccountList->GetItemText(i) +
                                                "/Host" ,""));
        AccountList->SetItem(i, 2, config->Read(AccountList->GetItemText(i) +
                                                "/Username" ,""));
        bCont = config->GetNextGroup(str, dummy);
        i++;
    }

    AccountList->SetColumnWidth(0, -1);
    AccountList->SetColumnWidth(1, -1);
    AccountList->SetColumnWidth(2, -1);
    if(AccountList->GetColumnWidth(0) < 100)        AccountList->SetColumnWidth(0,  100);
    if(AccountList->GetColumnWidth(1) < 100)        AccountList->SetColumnWidth(1,  100);
    if(AccountList->GetColumnWidth(2) < 100)        AccountList->SetColumnWidth(2,  100);

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

BEGIN_EVENT_TABLE(AddAccountDialog, wxDialog)
    EVT_BUTTON(XRCID("Add"),            AddAccountDialog::OnAdd)
END_EVENT_TABLE()

AddAccountDialog::AddAccountDialog( wxWindow* parent, wxString Selection )
{
    wxConfig  *config = new wxConfig("iaxComm");
    wxXmlResource::Get()->LoadDialog(this, parent, wxT("AddAccount"));

    //----Reach in for our controls-----------------------------------------------------
    AccountName  = XRCCTRL(*this, "AccountName",  wxTextCtrl);
    HostName     = XRCCTRL(*this, "HostName",     wxTextCtrl);
    UserName     = XRCCTRL(*this, "UserName",     wxTextCtrl);
    Password     = XRCCTRL(*this, "Password",     wxTextCtrl);
    Confirm      = XRCCTRL(*this, "Confirm",      wxTextCtrl);

    if(!Selection.IsEmpty()) {
        SetTitle(_("Edit " + Selection));
        AccountName->SetValue(Selection);
        config->SetPath("/Accounts/" + Selection);
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

    Show();
    wxGetApp().theFrame->ShowDirectoryControls();
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

    Show();
}

void DirectoryDialog::OnAddAccountList(wxCommandEvent &event)
{
    long     sel = -1;
    wxString val;

    if(event.GetId() == XRCID("EditAccountList")) {
        if((sel = AccountList->GetNextItem(sel,wxLIST_NEXT_ALL,wxLIST_STATE_SELECTED)) >= 0)
            val = AccountList->GetItemText(sel);
    }
    AddAccountDialog dialog(this, val);
    dialog.ShowModal();

    Show();
    wxGetApp().theFrame->ShowDirectoryControls();
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
    wxGetApp().theFrame->ShowDirectoryControls();
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

void DirectoryDialog::OnRemoveAccountList(wxCommandEvent &event)
{
    wxConfig  *config = new wxConfig("iaxComm");
    long       sel = -1;
    int        isOK;

    if((sel=AccountList->GetNextItem(sel,wxLIST_NEXT_ALL,wxLIST_STATE_SELECTED)) >= 0) {
        isOK = wxMessageBox("Really remove " + AccountList->GetItemText(sel) + "?",
                            "Remove from Account List", wxOK|wxCANCEL|wxCENTRE);
        if(isOK == wxOK) {
            config->DeleteGroup("/Accounts/"+AccountList->GetItemText(sel));
            AccountList->DeleteItem(sel);
        }
    }
    delete config;
    wxGetApp().theFrame->ShowDirectoryControls();
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

void AddAccountDialog::OnAdd(wxCommandEvent &event)
{
    wxConfig  *config = new wxConfig("iaxComm");

    if(!Password->GetValue().IsSameAs(Confirm->GetValue())) {
        wxMessageBox(_("Try Again"),
                     _("Password Mismatch"),
                       wxICON_INFORMATION);
        return;
    }
    config->SetPath("/Accounts/" + AccountName->GetValue());
    config->Write("Host",     HostName->GetValue());
    config->Write("Username", UserName->GetValue());
    config->Write("Password", Password->GetValue());
    delete config;

    // Well we wouldn't have added it if we didn't want to regiser
    // Thanks, AJ
    char user[256], pass[256], host[256];
    wxStrcpy(user, UserName->GetValue());
    wxStrcpy(pass, Password->GetValue());
    wxStrcpy(host, HostName->GetValue());
    iaxc_register(user, pass, host);

    AccountName->SetValue("");
    HostName->SetValue("");
    UserName->SetValue("");
    HostName->SetValue("");
    Confirm->SetValue("");
}

