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
    EVT_BUTTON( XRCID("BrowseRingTone"),     PrefsDialog::OnBrowse)
    EVT_BUTTON( XRCID("BrowseIntercom"),     PrefsDialog::OnBrowse)
    EVT_BUTTON( XRCID("BrowseRingBack"),     PrefsDialog::OnBrowse)

    EVT_BUTTON( XRCID("SaveAudio"),          PrefsDialog::OnSaveAudio)
    EVT_BUTTON( XRCID("ApplyAudio"),         PrefsDialog::OnApplyAudio)

    EVT_BUTTON( XRCID("SaveCallerID"),       PrefsDialog::OnSaveCallerID)
    EVT_BUTTON( XRCID("ApplyCallerID"),      PrefsDialog::OnApplyCallerID)

    EVT_BUTTON( XRCID("SaveMisc"),           PrefsDialog::OnSaveMisc)
    EVT_BUTTON( XRCID("ApplyMisc"),          PrefsDialog::OnApplyMisc)

    EVT_BUTTON( XRCID("SaveFilters"),        PrefsDialog::OnSaveFilters)
    EVT_BUTTON( XRCID("ApplyFilters"),       PrefsDialog::OnApplyFilters)

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

    Name           = XRCCTRL(*this, "Name",           wxTextCtrl);
    Number         = XRCCTRL(*this, "Number",         wxTextCtrl);

    UseView        = XRCCTRL(*this, "UseView",        wxComboBox);
    DefaultAccount = XRCCTRL(*this, "DefaultAccount", wxChoice);
    IntercomPass   = XRCCTRL(*this, "IntercomPass",   wxTextCtrl);
    nCalls         = XRCCTRL(*this, "nCalls",         wxSpinCtrl);

    AGC            = XRCCTRL(*this, "AGC",            wxCheckBox);
    NoiseReduce    = XRCCTRL(*this, "NoiseReduce",    wxCheckBox);
    EchoCancel     = XRCCTRL(*this, "EchoCancel",     wxCheckBox);

    config->SetPath("/");

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
    dummy = DefaultAccount->FindString(config->Read("/DefaultAccount", ""));
    if(dummy <= 0)
        dummy = 0;
    DefaultAccount->SetSelection(dummy);

    IntercomPass->SetValue(config->Read("/IntercomPass", ""));
    nCalls->SetValue(config->Read("/nCalls", 2));

    AGC->SetValue(wxGetApp().theFrame->AGC);
    NoiseReduce->SetValue(wxGetApp().theFrame->NoiseReduce);
    EchoCancel->SetValue(wxGetApp().theFrame->EchoCancel);

    delete config;
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
    config->SetPath("/");

    config->Write("RingTone",   RingTone->GetValue());
    config->Write("RingBack",   RingBack->GetValue());
    config->Write("Intercom",   Intercom->GetValue());

    delete config;
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
}

void PrefsDialog::OnSaveCallerID(wxCommandEvent &event)
{
    wxConfig *config = new wxConfig("iaxComm");
    config->SetPath("/");

    config->Write("Name",           Name->GetValue());
    config->Write("Number",         Number->GetValue());

    delete config;
    OnApplyCallerID(event);
}

void PrefsDialog::OnApplyCallerID(wxCommandEvent &event)
{
    wxGetApp().theFrame->Name   = Name->GetValue();
    wxGetApp().theFrame->Number = Number->GetValue();
    SetCallerID(wxGetApp().theFrame->Name, wxGetApp().theFrame->Number);
}

void PrefsDialog::OnSaveMisc(wxCommandEvent &event)
{
    wxConfig *config = new wxConfig("iaxComm");
    config->SetPath("/");

    config->Write("UseView",        UseView->GetValue());
    config->Write("DefaultAccount", DefaultAccount->GetStringSelection());
    config->Write("IntercomPass",   IntercomPass->GetValue());
    config->Write("nCalls",         nCalls->GetValue());

    delete config;
    OnApplyMisc(event);
}

void PrefsDialog::OnApplyMisc(wxCommandEvent &event)
{
    // Update the Default Account
    wxGetApp().theFrame->DefaultAccount = DefaultAccount->GetStringSelection();
    wxGetApp().theFrame->ShowDirectoryControls();

    wxGetApp().theFrame->IntercomPass = IntercomPass->GetValue();

    wxGetApp().theFrame->RePanel(UseView->GetValue());
}

void PrefsDialog::OnSaveFilters(wxCommandEvent &event)
{
    wxConfig *config = new wxConfig("iaxComm");
    config->SetPath("/");

    config->Write("AGC",            AGC->GetValue());
    config->Write("NoiseReduce",    NoiseReduce->GetValue());
    config->Write("EchoCancel",     EchoCancel->GetValue());

    delete config;
    OnApplyFilters(event);
}

void PrefsDialog::OnApplyFilters(wxCommandEvent &event)
{
    wxGetApp().theFrame->AGC         = AGC->GetValue();
    wxGetApp().theFrame->NoiseReduce = NoiseReduce->GetValue();
    wxGetApp().theFrame->EchoCancel  = EchoCancel->GetValue();

    ApplyFilters();
}

void PrefsDialog::ApplyFilters()
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
