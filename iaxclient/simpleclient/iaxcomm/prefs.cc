//----------------------------------------------------------------------------------------
// Name:        prefs.cc
// Purpose:     preferences dialog
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
    #pragma implementation "prefs.h"
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
#include "prefs.h"

//----------------------------------------------------------------------------------------
// Remaining headers
// ---------------------------------------------------------------------------------------

#include "app.h"
#include "frame.h"
#include "main.h"
#include "calls.h"

//----------------------------------------------------------------------------------------
// Event table: connect the events to the handler functions to process them
//----------------------------------------------------------------------------------------
BEGIN_EVENT_TABLE(PrefsDialog, wxDialog)
    EVT_BUTTON  ( XRCID("BrowseRingTone"),     PrefsDialog::OnBrowse)
    EVT_BUTTON  ( XRCID("BrowseIntercom"),     PrefsDialog::OnBrowse)
    EVT_BUTTON  ( XRCID("BrowseRingBack"),     PrefsDialog::OnBrowse)
    EVT_TEXT    ( XRCID("RingBack"),           PrefsDialog::OnAudioDirty)
    EVT_TEXT    ( XRCID("RingTone"),           PrefsDialog::OnAudioDirty)
    EVT_TEXT    ( XRCID("Intercom"),           PrefsDialog::OnAudioDirty)
    EVT_BUTTON  ( XRCID("SaveAudio"),          PrefsDialog::OnSaveAudio)
    EVT_BUTTON  ( XRCID("ApplyAudio"),         PrefsDialog::OnApplyAudio)

    EVT_BUTTON  ( XRCID("SaveCallerID"),       PrefsDialog::OnSaveCallerID)
    EVT_BUTTON  ( XRCID("ApplyCallerID"),      PrefsDialog::OnApplyCallerID)
    EVT_TEXT    ( XRCID("Name"),               PrefsDialog::OnCallerIDDirty)
    EVT_TEXT    ( XRCID("Number"),             PrefsDialog::OnCallerIDDirty)

    EVT_BUTTON  ( XRCID("SaveMisc"),           PrefsDialog::OnSaveMisc)
    EVT_BUTTON  ( XRCID("ApplyMisc"),          PrefsDialog::OnApplyMisc)
    EVT_TEXT    ( XRCID("UseView"),            PrefsDialog::OnMiscDirty)
    EVT_CHOICE  ( XRCID("DefaultAccount"),     PrefsDialog::OnMiscDirty)
    EVT_TEXT    ( XRCID("IntercomPass"),       PrefsDialog::OnMiscDirty)
    EVT_SPINCTRL( XRCID("nCalls"),             PrefsDialog::OnMiscDirty)

    EVT_BUTTON  ( XRCID("SaveFilters"),        PrefsDialog::OnSaveFilters)
    EVT_BUTTON  ( XRCID("ApplyFilters"),       PrefsDialog::OnApplyFilters)
    EVT_CHECKBOX( XRCID("AGC"),                PrefsDialog::OnFiltersDirty)
    EVT_CHECKBOX( XRCID("NoiseReduce"),        PrefsDialog::OnFiltersDirty)
    EVT_CHECKBOX( XRCID("EchoCancel"),         PrefsDialog::OnFiltersDirty)

    EVT_BUTTON  ( XRCID("CancelAudio"),        PrefsDialog::OnCancel)
    EVT_BUTTON  ( XRCID("CancelCallerID"),     PrefsDialog::OnCancel)
    EVT_BUTTON  ( XRCID("CancelMisc"),         PrefsDialog::OnCancel)
    EVT_BUTTON  ( XRCID("CancelFilters"),      PrefsDialog::OnCancel)

END_EVENT_TABLE()

//----------------------------------------------------------------------------------------
// Public methods
//----------------------------------------------------------------------------------------

PrefsDialog::PrefsDialog(wxWindow* parent)
{    
    wxConfig *config = new wxConfig("iaxComm");
    long      dummy;
    bool      bCont;
    wxString  str;

    wxXmlResource::Get()->LoadDialog(this, parent, wxT("Prefs"));

    // Reach in for our controls

    RingBack       = XRCCTRL(*this, "RingBack",       wxTextCtrl);
    RingTone       = XRCCTRL(*this, "RingTone",       wxTextCtrl);
    Intercom       = XRCCTRL(*this, "Intercom",       wxTextCtrl);
    SaveAudio      = XRCCTRL(*this, "SaveAudio",      wxButton);
    ApplyAudio     = XRCCTRL(*this, "ApplyAudio",     wxButton);
    CancelAudio    = XRCCTRL(*this, "CancelAudio",    wxButton);

    Name           = XRCCTRL(*this, "Name",           wxTextCtrl);
    Number         = XRCCTRL(*this, "Number",         wxTextCtrl);
    SaveCallerID   = XRCCTRL(*this, "SaveCallerID",   wxButton);
    ApplyCallerID  = XRCCTRL(*this, "ApplyCallerID",  wxButton);
    CancelCallerID = XRCCTRL(*this, "CancelCallerID", wxButton);

    UseView        = XRCCTRL(*this, "UseView",        wxComboBox);
    DefaultAccount = XRCCTRL(*this, "DefaultAccount", wxChoice);
    IntercomPass   = XRCCTRL(*this, "IntercomPass",   wxTextCtrl);
    nCalls         = XRCCTRL(*this, "nCalls",         wxSpinCtrl);
    SaveMisc       = XRCCTRL(*this, "SaveMisc",       wxButton);
    ApplyMisc      = XRCCTRL(*this, "ApplyMisc",      wxButton);
    CancelMisc     = XRCCTRL(*this, "CancelMisc",     wxButton);

    AGC            = XRCCTRL(*this, "AGC",            wxCheckBox);
    NoiseReduce    = XRCCTRL(*this, "NoiseReduce",    wxCheckBox);
    EchoCancel     = XRCCTRL(*this, "EchoCancel",     wxCheckBox);
    SaveFilters    = XRCCTRL(*this, "SaveFilters",    wxButton);
    ApplyFilters   = XRCCTRL(*this, "ApplyFilters",   wxButton);
    CancelFilters  = XRCCTRL(*this, "CancelFilters",  wxButton);

    config->SetPath("/Prefs");

    RingTone->SetValue(wxGetApp().theFrame->Calls->RingToneName);
    RingBack->SetValue(wxGetApp().theFrame->Calls->RingBackName);
    Intercom->SetValue(wxGetApp().theFrame->Calls->IntercomName);

    Name->SetValue(config->Read("Name", "Caller Name"));
    Number->SetValue(config->Read("Number", "700000000"));

    UseView->Append("default");
    UseView->Append("compact");
    UseView->SetValue(config->Read("UseView", "default"));

    config->SetPath("/Accounts");
    bCont = config->GetFirstGroup(str, dummy);
    while ( bCont ) {
        DefaultAccount->Append(str);
        bCont = config->GetNextGroup(str, dummy);
    }
    dummy = DefaultAccount->FindString(config->Read("/Prefs/DefaultAccount", ""));
    if(dummy <= 0)
        dummy = 0;
    DefaultAccount->SetSelection(dummy);

    IntercomPass->SetValue(config->Read("/Prefs/IntercomPass", ""));
    nCalls->SetValue(wxGetApp().theFrame->nCalls);

    AGC->SetValue(wxGetApp().theFrame->AGC);
    NoiseReduce->SetValue(wxGetApp().theFrame->NoiseReduce);
    EchoCancel->SetValue(wxGetApp().theFrame->EchoCancel);

    delete config;

    //Populating wxTextCtrls makes EVT_TEXT, just as if user did it

    SaveAudio->Disable();
    ApplyAudio->Disable();
    CancelAudio->SetLabel("Done");

    SaveCallerID->Disable();
    ApplyCallerID->Disable();
    CancelCallerID->SetLabel("Done");

    SaveMisc->Disable();
    ApplyMisc->Disable();
    CancelMisc->SetLabel("Done");
}

//----------------------------------------------------------------------------------------
// Private methods
//----------------------------------------------------------------------------------------

void PrefsDialog::OnBrowse(wxCommandEvent &event)
{
    wxString dirHome;
    wxGetHomeDir(&dirHome);

    wxFileDialog where(NULL, _("Raw sound file"), dirHome, "", "*.*", wxOPEN );
    where.ShowModal();

    if(event.GetId() == XRCID("BrowseRingBack"))
        RingBack->SetValue(where.GetPath());

    if(event.GetId() == XRCID("BrowseRingTone"))
        RingTone->SetValue(where.GetPath());

    if(event.GetId() == XRCID("BrowseIntercom"))
        Intercom->SetValue(where.GetPath());

}

void SetCallerID(wxString name, wxString number)
{
    iaxc_set_callerid((char *)name.c_str(),
                      (char *)number.c_str());
}

void PrefsDialog::OnSaveAudio(wxCommandEvent &event)
{
    wxConfig *config = new wxConfig("iaxComm");
    config->SetPath("/Prefs");

    config->Write("RingTone",   RingTone->GetValue());
    config->Write("RingBack",   RingBack->GetValue());
    config->Write("Intercom",   Intercom->GetValue());

    delete config;
    SaveAudio->Disable();
    OnApplyAudio(event);
}

void PrefsDialog::OnApplyAudio(wxCommandEvent &event)
{
    if(!RingTone->GetValue().IsEmpty())
        LoadTone(&wxGetApp().theFrame->Calls->ringtone, RingTone->GetValue(),10);
    else
        CalcTone(&wxGetApp().theFrame->Calls->ringtone, 880, 960, 16000, 48000, 10);

    if(!RingBack->GetValue().IsEmpty())
        LoadTone(&wxGetApp().theFrame->Calls->ringback, RingBack->GetValue(),10);
    else
        CalcTone(&wxGetApp().theFrame->Calls->ringback, 440, 480, 16000, 48000, 10);

    if(!Intercom->GetValue().IsEmpty())
        LoadTone(&wxGetApp().theFrame->Calls->icomtone, Intercom->GetValue(),10);
    else
        CalcTone(&wxGetApp().theFrame->Calls->icomtone, 440, 960,  6000,  6000,  1);

    wxGetApp().theFrame->Calls->RingToneName = RingTone->GetValue();
    wxGetApp().theFrame->Calls->RingBackName = RingBack->GetValue();
    wxGetApp().theFrame->Calls->IntercomName = Intercom->GetValue();

    ApplyAudio->Disable();
    CancelAudio->SetLabel("Done");
}

void PrefsDialog::OnSaveCallerID(wxCommandEvent &event)
{
    wxConfig *config = new wxConfig("iaxComm");
    config->SetPath("/Prefs");

    config->Write("Name",           Name->GetValue());
    config->Write("Number",         Number->GetValue());

    delete config;
    SaveCallerID->Disable();
    OnApplyCallerID(event);
}

void PrefsDialog::OnApplyCallerID(wxCommandEvent &event)
{
    wxGetApp().theFrame->Name   = Name->GetValue();
    wxGetApp().theFrame->Number = Number->GetValue();
    SetCallerID(wxGetApp().theFrame->Name, wxGetApp().theFrame->Number);

    ApplyCallerID->Disable();
    CancelCallerID->SetLabel("Done");
}

void PrefsDialog::OnSaveMisc(wxCommandEvent &event)
{
    wxConfig *config = new wxConfig("iaxComm");
    config->SetPath("/Prefs");

    config->Write("UseView",        UseView->GetValue());
    config->Write("DefaultAccount", DefaultAccount->GetStringSelection());
    config->Write("IntercomPass",   IntercomPass->GetValue());
    config->Write("nCalls",         nCalls->GetValue());

    delete config;
    SaveMisc->Disable();
    OnApplyMisc(event);
}

void PrefsDialog::OnApplyMisc(wxCommandEvent &event)
{
    // Update the Default Account
    wxGetApp().theFrame->DefaultAccount = DefaultAccount->GetStringSelection();
    wxGetApp().theFrame->ShowDirectoryControls();

    wxGetApp().theFrame->IntercomPass = IntercomPass->GetValue();
//  wxGetApp().theFrame->nCalls       = nCalls->GetValue();

  #ifdef __WXMSW__
    // This segfaults on Linux, need to look into this
    wxGetApp().theFrame->RePanel(UseView->GetValue());
  #endif

    ApplyMisc->Disable();
    CancelMisc->SetLabel("Done");
}

void PrefsDialog::OnSaveFilters(wxCommandEvent &event)
{
    wxConfig *config = new wxConfig("iaxComm");
    config->SetPath("/Prefs");

    config->Write("AGC",            AGC->GetValue());
    config->Write("NoiseReduce",    NoiseReduce->GetValue());
    config->Write("EchoCancel",     EchoCancel->GetValue());

    delete config;
    SaveFilters->Disable();
    OnApplyFilters(event);
}

void PrefsDialog::OnApplyFilters(wxCommandEvent &event)
{
    wxGetApp().theFrame->AGC         = AGC->GetValue();
    wxGetApp().theFrame->NoiseReduce = NoiseReduce->GetValue();
    wxGetApp().theFrame->EchoCancel  = EchoCancel->GetValue();

    DoApplyFilters();

    ApplyFilters->Disable();
    CancelFilters->SetLabel("Done");
}

void PrefsDialog::DoApplyFilters()
{
    // Clear these filters
    int flag = ~(IAXC_FILTER_AGC | IAXC_FILTER_DENOISE | IAXC_FILTER_ECHO);
    iaxc_set_filters(iaxc_get_filters() & flag);

    flag = 0;
    if(wxGetApp().theFrame->AGC)
       flag = IAXC_FILTER_AGC;

    if(wxGetApp().theFrame->NoiseReduce)
       flag |= IAXC_FILTER_DENOISE;

    if(wxGetApp().theFrame->EchoCancel)
       flag |= IAXC_FILTER_ECHO;

    iaxc_set_filters(iaxc_get_filters() | flag);
}

void PrefsDialog::OnAudioDirty(wxCommandEvent &event)
{
    ApplyAudio->Enable();
    SaveAudio->Enable();
    CancelAudio->SetLabel("Cancel");
}

void PrefsDialog::OnCallerIDDirty(wxCommandEvent &event)
{
    ApplyCallerID->Enable();
    SaveCallerID->Enable();
    CancelCallerID->SetLabel("Cancel");
}

void PrefsDialog::OnMiscDirty(wxCommandEvent &event)
{
    ApplyMisc->Enable();
    SaveMisc->Enable();
    CancelMisc->SetLabel("Cancel");
}

void PrefsDialog::OnFiltersDirty(wxCommandEvent &event)
{
    ApplyFilters->Enable();
    SaveFilters->Enable();
    CancelFilters->SetLabel("Cancel");
}
