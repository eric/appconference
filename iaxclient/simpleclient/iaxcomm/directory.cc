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
    #pragma implementation "MyFrame.h"
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
    EVT_BUTTON(  wxID_OK,                DirectoryDialog::OnDone)
    EVT_BUTTON(  wxID_SAVE,              DirectoryDialog::OnSave)
    EVT_BUTTON(  wxID_RESET,             DirectoryDialog::OnRemove)
    EVT_BUTTON(XRCID("Dial"),            DirectoryDialog::OnDial)
    EVT_COMBOBOX(XRCID("ServerName"),    DirectoryDialog::OnServerName)
    EVT_COMBOBOX(XRCID("EntryName"),     DirectoryDialog::OnEntryName)
    EVT_SPINCTRL(XRCID("OTNo"),          DirectoryDialog::OnOTNo)
END_EVENT_TABLE()


//----------------------------------------------------------------------------------------
// Public methods
//----------------------------------------------------------------------------------------

DirectoryDialog::DirectoryDialog( wxWindow* parent )
{
    wxConfig *config = new wxConfig("iaxComm");
    wxString  str;
    long      dummy;
    bool      bCont;

    wxXmlResource::Get()->LoadDialog(this, parent, wxT("Directory"));

    //----Reach in for our controls-----------------------------------------------------

    DirectoryNotebook  = XRCCTRL(*this, "DirectoryNotebook", wxNotebook);

    // Server tab
    ServerName   = XRCCTRL(*this, "ServerName",   wxComboBox);
    HostName     = XRCCTRL(*this, "HostName",     wxTextCtrl);
    UserName     = XRCCTRL(*this, "UserName",     wxTextCtrl);
    Password     = XRCCTRL(*this, "Password",     wxTextCtrl);
    Confirm      = XRCCTRL(*this, "Confirm",      wxTextCtrl);

    // Entries tab
    EntryName    = XRCCTRL(*this, "EntryName",    wxComboBox);
    ChooseServer = XRCCTRL(*this, "ChooseServer", wxChoice);
    Extension    = XRCCTRL(*this, "Extension",    wxTextCtrl);

    // One Touch tab
    OTNo         = XRCCTRL(*this, "OTNo",         wxSpinCtrl);
    ShortName    = XRCCTRL(*this, "ShortName",    wxTextCtrl);
    ChooseEntry  = XRCCTRL(*this, "ChooseEntry",  wxChoice);

    SaveButton   = XRCCTRL(*this, "wxID_SAVE",    wxButton);
    ApplyButton  = XRCCTRL(*this, "wxID_APPLY",   wxButton);

    //----Populate Servers choice controls----------------------------------------------
    config->SetPath("/Servers");
    bCont = config->GetFirstGroup(str, dummy);
    while ( bCont ) {
        ServerName->Append(str);
        ChooseServer->Append(str);
        bCont = config->GetNextGroup(str, dummy);
    }

    //----Populate Entry choice controls------------------------------------------------
    config->SetPath("/Entries");
    bCont = config->GetFirstGroup(str, dummy);
    while ( bCont ) {
        EntryName->Append(str);
        ChooseEntry->Append(str);
        bCont = config->GetNextGroup(str, dummy);
    }

    // Needed so we can appear to clear it
    ChooseServer->Append(" ");
    ChooseEntry->Append(" ");
}

//----------------------------------------------------------------------------------------
// Private methods
//----------------------------------------------------------------------------------------

void DirectoryDialog::OnServerName(wxCommandEvent &event)
{
    wxConfig *config = new wxConfig("iaxComm");
    wxString  val;
    wxString  KeyPath;

    // Update the Host/Username/Password boxes
    KeyPath = "/Servers/" + ServerName->GetStringSelection();
    config->SetPath(KeyPath);

    if(config->Exists(KeyPath)) {
        HostName->SetValue(config->Read("Host", ""));
        UserName->SetValue(config->Read("Username", ""));
        Password->SetValue(config->Read("Password", ""));
        Confirm->SetValue(Password->GetValue());
    }
}

void DirectoryDialog::OnEntryName(wxCommandEvent &event)
{
    wxConfig *config = new wxConfig("iaxComm");
    wxString  val;
    wxString  KeyPath;

    KeyPath = "/Entries/" + EntryName->GetStringSelection();
    config->SetPath(KeyPath);

    if(config->Exists(KeyPath)) {
        ChooseServer->SetStringSelection(config->Read("Server", ""));
        Extension->SetValue(config->Read("Extension", ""));
    }
}

void DirectoryDialog::OnOTNo(wxCommandEvent &event)
{
    wxConfig *config = new wxConfig("iaxComm");
    wxString  val;
    wxString  KeyPath;

    KeyPath.Printf("/OneTouch/OT%d", OTNo->GetValue());
    config->SetPath(KeyPath);

    if(config->Exists(KeyPath)) {
        ChooseEntry->SetStringSelection(config->Read("EntryName", ""));
        ShortName->SetValue(config->Read("ShortName", ""));
    } else {
        // *looks* empty
        ChooseEntry->SetStringSelection(" ");
        ShortName->SetValue("");
    }
}

void DirectoryDialog::OnDone(wxCommandEvent &event)
{
    Close();
}

void DirectoryDialog::OnSave(wxCommandEvent &event)
{
    wxConfig *config = new wxConfig("iaxComm");
    wxString  val;
    wxString  KeyPath;

    switch(DirectoryNotebook->GetSelection())
    {
      case 0:
           //--Server Tab ----------------------------------------------------------------
           if(!IsEmpty(ServerName->GetValue())) {
               KeyPath = "/Servers/" + ServerName->GetValue();
               config->SetPath(KeyPath);

               if(!Password->GetValue().IsSameAs(Confirm->GetValue())) {
                   wxMessageBox(_("Try Again"),
                                _("Password Mismatch"),
                                wxICON_INFORMATION);
               } else {
                  if(IsEmpty(HostName->GetValue())) {
                       wxMessageBox(_("Please Specify a Hostname"),
                                    _("Empty Hostname"),
                                    wxICON_INFORMATION);
                  } else {
                       config->Write("Host",     HostName->GetValue());
                       config->Write("Username", UserName->GetValue());
                       config->Write("Password", Password->GetValue());
                       wxGetApp().theFrame->Server->Append(ServerName->GetValue());
                  }
               }
           }
           break;

      case 1:
           //--Entries Tab ---------------------------------------------------------------
           if(!IsEmpty(EntryName->GetValue())) {
               KeyPath = "/Entries/" + EntryName->GetValue();
               config->SetPath(KeyPath);

               if(IsEmpty(ChooseServer->GetStringSelection())) {
                    wxMessageBox(_("Please Specify a server"),
                                 _("Empty Server Name"),
                                 wxICON_INFORMATION);
               } else {
                    // Empty Extension is OK. (Dialler will substitute s)
                    config->Write("Server",    ChooseServer->GetStringSelection());
                    config->Write("Extension", Extension->GetValue());
               }
           }
           break;

      case 2:
           //--One Touch Tab -------------------------------------------------------------
           if((OTNo->GetValue() >= OTNo->GetMin()) &&
              (OTNo->GetValue() <= OTNo->GetMax())) {
               KeyPath.Printf("/OneTouch/OT%d", OTNo->GetValue());
               config->SetPath(KeyPath);

               if(IsEmpty(ShortName->GetValue())) {
                    wxMessageBox(_("Please Specify a short name"),
                                 _("Empty Short Name"),
                                 wxICON_INFORMATION);
               } else {
                   if(IsEmpty(ChooseEntry->GetStringSelection())) {
                        wxMessageBox(_("Please Specify an entry"),
                                     _("Empty Entry Name"),
                                     wxICON_INFORMATION);
                   } else {
                        config->Write("EntryName", ChooseEntry->GetStringSelection());
                        config->Write("ShortName", ShortName->GetValue());

                        wxButton *OT;
                        wxString OTName;
                        OTName.Printf("OT%d", OTNo->GetValue());
                        OT = XRCCTRL(*wxGetApp().theFrame, OTName, wxButton);
                        OT->SetLabel(ShortName->GetValue());
                   }
               }

           }
           break;
    }
    delete config;
}

void DirectoryDialog::OnRemove(wxCommandEvent &event)
{
    wxConfig *config = new wxConfig("iaxComm");
    wxString  val;
    wxString  KeyPath;

    switch(DirectoryNotebook->GetSelection())
    {
      case 0:
           //--Server Tab ----------------------------------------------------------------
           if(!IsEmpty(ServerName->GetValue())) {
               KeyPath = "/Servers/" + ServerName->GetValue();

               if(IsEmpty(HostName->GetValue())) {
                    wxMessageBox(_("Please Specify a Hostname"),
                                 _("Empty Hostname"),
                                 wxICON_INFORMATION);
               } else {
                    wxGetApp().theFrame->Server->Delete(wxGetApp().theFrame->Server->
                                         FindString(ServerName->GetStringSelection()));
                    config->DeleteGroup(KeyPath);
                    ServerName->Delete(ServerName->GetSelection());
                    HostName->SetValue("");
                    UserName->SetValue("");
                    Password->SetValue("");
                    Confirm->SetValue("");
               }
           }
           break;

      case 1:
           //--Entries Tab ---------------------------------------------------------------
           if(!IsEmpty(EntryName->GetValue())) {
               KeyPath = "/Entries/" + EntryName->GetValue();

               if(IsEmpty(ChooseServer->GetStringSelection())) {
                    wxMessageBox(_("Please Specify a server"),
                                 _("Empty Server Name"),
                                 wxICON_INFORMATION);
               } else {
                    config->DeleteGroup(KeyPath);
                    EntryName->Delete(EntryName->GetSelection());
                    ChooseServer->SetStringSelection(" ");
                    Extension->SetValue("");
               }
           }
           break;

      case 2:
           //--One Touch Tab -------------------------------------------------------------
           if((OTNo->GetValue() >= OTNo->GetMin()) &&
              (OTNo->GetValue() <= OTNo->GetMax())) {
               KeyPath.Printf("/OneTouch/OT%d", OTNo->GetValue());

               if(IsEmpty(ChooseEntry->GetStringSelection())) {
                    wxMessageBox(_("Please Specify an entry"),
                                 _("Empty Entry Name"),
                                 wxICON_INFORMATION);
               } else {
                    config->DeleteGroup(KeyPath);
                    ChooseEntry->SetStringSelection(" ");
                    ShortName->SetValue("");

                    wxButton *OT;
                    wxString OTName;
                    OTName.Printf("OT%d", OTNo->GetValue());
                    OT = XRCCTRL(*wxGetApp().theFrame, OTName, wxButton);
                    if(OT != NULL) {
                        OT->SetLabel("");
                      //OT->Destroy();
                    }
               }
           }
           break;
    }
    delete config;
}

void DirectoryDialog::OnDial(wxCommandEvent &event)
{
    wxString  Name;

    Name = EntryName->GetValue();
    DialEntry(Name);
}

