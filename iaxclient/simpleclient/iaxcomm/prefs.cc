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
    EVT_BUTTON  ( XRCID("BrowseRingTone"),   PrefsDialog::OnBrowse)
    EVT_BUTTON  ( XRCID("BrowseIntercom"),   PrefsDialog::OnBrowse)
    EVT_BUTTON  ( XRCID("BrowseRingBack"),   PrefsDialog::OnBrowse)
    EVT_BUTTON  ( XRCID("PreviewRingTone"),  PrefsDialog::OnPreviewRingTone)
    EVT_BUTTON  ( XRCID("PreviewIntercom"),  PrefsDialog::OnPreviewIntercom)
    EVT_BUTTON  ( XRCID("PreviewRingBack"),  PrefsDialog::OnPreviewRingBack)
    EVT_TEXT    ( XRCID("RingBack"),         PrefsDialog::OnAudioDirty)
    EVT_TEXT    ( XRCID("RingTone"),         PrefsDialog::OnAudioDirty)
    EVT_TEXT    ( XRCID("Intercom"),         PrefsDialog::OnAudioDirty)
    EVT_BUTTON  ( XRCID("SaveAudio"),        PrefsDialog::OnSaveAudio)
    EVT_BUTTON  ( XRCID("ApplyAudio"),       PrefsDialog::OnApplyAudio)

    EVT_BUTTON  ( XRCID("SaveCallerID"),     PrefsDialog::OnSaveCallerID)
    EVT_BUTTON  ( XRCID("ApplyCallerID"),    PrefsDialog::OnApplyCallerID)
    EVT_TEXT    ( XRCID("Name"),             PrefsDialog::OnCallerIDDirty)
    EVT_TEXT    ( XRCID("Number"),           PrefsDialog::OnCallerIDDirty)

    EVT_BUTTON  ( XRCID("SaveMisc"),         PrefsDialog::OnSaveMisc)
    EVT_BUTTON  ( XRCID("ApplyMisc"),        PrefsDialog::OnApplyMisc)
    EVT_TEXT    ( XRCID("UseSkin"),          PrefsDialog::OnMiscDirty)
    EVT_CHOICE  ( XRCID("DefaultAccount"),   PrefsDialog::OnMiscDirty)
    EVT_TEXT    ( XRCID("IntercomPass"),     PrefsDialog::OnMiscDirty)
    EVT_SPINCTRL( XRCID("nCalls"),           PrefsDialog::OnMiscDirty)

    EVT_BUTTON  ( XRCID("SaveFilters"),      PrefsDialog::OnSaveFilters)
    EVT_BUTTON  ( XRCID("ApplyFilters"),     PrefsDialog::OnApplyFilters)
    EVT_CHECKBOX( XRCID("AGC"),              PrefsDialog::OnFiltersDirty)
    EVT_CHECKBOX( XRCID("NoiseReduce"),      PrefsDialog::OnFiltersDirty)
    EVT_CHECKBOX( XRCID("EchoCancel"),       PrefsDialog::OnFiltersDirty)

    EVT_BUTTON  ( XRCID("CancelAudio"),      PrefsDialog::OnCancel)
    EVT_BUTTON  ( XRCID("CancelCallerID"),   PrefsDialog::OnCancel)
    EVT_BUTTON  ( XRCID("CancelMisc"),       PrefsDialog::OnCancel)
    EVT_BUTTON  ( XRCID("CancelFilters"),    PrefsDialog::OnCancel)

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

    UseSkin        = XRCCTRL(*this, "UseSkin",        wxComboBox);
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

    RingTone->SetValue(wxGetApp().IncomingRingName);
    RingBack->SetValue(wxGetApp().RingBackToneName);
    Intercom->SetValue(wxGetApp().IntercomToneName);

    Name->SetValue(wxGetApp().Name);
    Number->SetValue(wxGetApp().Number);

    UseSkin->Append("default");
    UseSkin->Append("compact");
    UseSkin->Append("logo");
    UseSkin->Append("micro");
    UseSkin->SetValue(config->Read("UseSkin", "default"));

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
    nCalls->SetValue(wxGetApp().nCalls);

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

void PrefsDialog::OnPreviewRingTone(wxCommandEvent &event)
{
    // We only want to preview a single ring
    wxGetApp().IncomingRing.LoadTone(RingTone->GetValue(), 0);
    wxGetApp().IncomingRing.Start(1);
    wxGetApp().IncomingRing.LoadTone(RingTone->GetValue(), 10);
}

void PrefsDialog::OnPreviewRingBack(wxCommandEvent &event)
{
    wxGetApp().RingbackTone.LoadTone(RingBack->GetValue(), 0);
    wxGetApp().RingbackTone.Start(1);
    wxGetApp().RingbackTone.LoadTone(RingBack->GetValue(), 10);
}

void PrefsDialog::OnPreviewIntercom(wxCommandEvent &event)
{
    wxGetApp().IntercomTone.LoadTone(Intercom->GetValue(), 0);
    wxGetApp().IntercomTone.Start(1);
    wxGetApp().IntercomTone.LoadTone(Intercom->GetValue(), 1);
}

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
    wxGetApp().IncomingRingName = RingTone->GetValue();
    wxGetApp().RingBackToneName = RingBack->GetValue();
    wxGetApp().IntercomToneName = Intercom->GetValue();

    wxGetApp().IncomingRing.LoadTone(RingTone->GetValue(), 10);
    wxGetApp().RingbackTone.LoadTone(RingBack->GetValue(), 10);
    wxGetApp().IntercomTone.LoadTone(Intercom->GetValue(),  1);

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
    wxGetApp().Name   = Name->GetValue();
    wxGetApp().Number = Number->GetValue();
    SetCallerID(wxGetApp().Name, wxGetApp().Number);

    ApplyCallerID->Disable();
    CancelCallerID->SetLabel("Done");
}

void PrefsDialog::OnSaveMisc(wxCommandEvent &event)
{
    wxConfig *config = new wxConfig("iaxComm");
    config->SetPath("/Prefs");

    config->Write("UseSkin",        UseSkin->GetValue());
    config->Write("DefaultAccount", DefaultAccount->GetStringSelection());
    config->Write("IntercomPass",   IntercomPass->GetValue());
    config->Write("nCalls",         nCalls->GetValue());

    delete config;
    SaveMisc->Disable();
    OnApplyMisc(event);
}

void PrefsDialog::OnApplyMisc(wxCommandEvent &event)
{
    // Update the Default Account on the main panel, if there is one
    wxGetApp().DefaultAccount = DefaultAccount->GetStringSelection();
    wxGetApp().theFrame->ShowDirectoryControls();


    wxGetApp().theFrame->IntercomPass = IntercomPass->GetValue();
    wxGetApp().nCalls                 = nCalls->GetValue();

  #ifdef __WXMSW__
    // This segfaults on Linux, need to look into this
    wxGetApp().theFrame->RePanel(UseSkin->GetValue());
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
