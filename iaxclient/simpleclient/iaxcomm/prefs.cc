//----------------------------------------------------------------------------------------
// Name:        prefs.cpp
// Purpose:     prefernces dialog
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

//----------------------------------------------------------------------------------------
// Event table: connect the events to the handler functions to process them
//----------------------------------------------------------------------------------------
BEGIN_EVENT_TABLE(PrefsDialog, wxDialog)
    EVT_BUTTON(  wxID_SAVE,              PrefsDialog::OnSave)
    EVT_BUTTON(  wxID_APPLY,             PrefsDialog::OnApply)
    EVT_CHOICE(  XRCID("InputDevice"),   PrefsDialog::OnDirty)
    EVT_CHOICE(  XRCID("OutputDevice"),  PrefsDialog::OnDirty)
    EVT_CHOICE(  XRCID("RingDevice"),    PrefsDialog::OnDirty)
    EVT_TEXT(    XRCID("Name"),          PrefsDialog::OnDirty)
    EVT_TEXT(    XRCID("Number"),        PrefsDialog::OnDirty)
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

    InputDevice   = XRCCTRL(*this, "InputDevice",   wxChoice);
    OutputDevice  = XRCCTRL(*this, "OutputDevice",  wxChoice);
    RingDevice    = XRCCTRL(*this, "RingDevice",    wxChoice);

    Name          = XRCCTRL(*this, "Name",          wxTextCtrl);
    Number        = XRCCTRL(*this, "Number",        wxTextCtrl);

    UseView       = XRCCTRL(*this, "UseView",       wxChoice);
    DefaultServer = XRCCTRL(*this, "DefaultServer", wxChoice);
    Intercom      = XRCCTRL(*this, "Intercom",      wxTextCtrl);
    nCalls        = XRCCTRL(*this, "nCalls",        wxSpinCtrl);

    AGC           = XRCCTRL(*this, "AGC",           wxCheckBox);
    NoiseReduce   = XRCCTRL(*this, "NoiseReduce",   wxCheckBox);
    EchoCancel    = XRCCTRL(*this, "EchoCancel",    wxCheckBox);

    SaveButton    = XRCCTRL(*this, "wxID_SAVE",     wxButton);
    ApplyButton   = XRCCTRL(*this, "wxID_APPLY",    wxButton);
    CancelButton  = XRCCTRL(*this, "wxID_CANCEL",   wxButton);

    GetAudioDevices();

    config->SetPath("/");

    SetAudioDevices(config->Read("Input Device",  ""),
                    config->Read("Output Device", ""),
                    config->Read("Ring Device",   ""));

    Name->SetValue(config->Read("Name", "Caller Name"));
    Number->SetValue(config->Read("Number", "700000000"));

    UseView->Append("default");
    UseView->Append("compact");
    UseView->Append("expanded");
    UseView->SetStringSelection(config->Read("UseView", "default"));

    config->SetPath("/Servers");
    bCont = config->GetFirstGroup(str, dummy);
    while ( bCont ) {
        DefaultServer->Append(str);
        bCont = config->GetNextGroup(str, dummy);
    }
    dummy = DefaultServer->FindString(config->Read("/DefaultServer", ""));
    if(dummy <= 0)
        dummy = 0;
    DefaultServer->SetSelection(dummy);

    Intercom->SetValue(config->Read("/Intercom", ""));
    nCalls->SetValue(config->Read("/nCalls", 2));

    AGC->SetValue(config->Read("/AGC", 0l));
    NoiseReduce->SetValue(config->Read("/NoiseReduce", 0l));
    EchoCancel->SetValue(config->Read("/EchoCancel", 0l));

    delete config;
}

//----------------------------------------------------------------------------------------
// Private methods
//----------------------------------------------------------------------------------------

void PrefsDialog::GetAudioDevices()
{
    struct iaxc_audio_device *devices;
    int               nDevs;
    int               input, output, ring;
    int               i;
    long              caps;
    wxString          devname;

    iaxc_audio_devices_get(&devices, &nDevs, &input, &output, &ring);

    for(i=0; i<nDevs; i++) {
        caps =    devices->capabilities;
        devname = devices->name;

        if(caps & IAXC_AD_INPUT)
            InputDevice->Append(devname);

        if(caps & IAXC_AD_OUTPUT)
            OutputDevice->Append(devname);

        if(caps & IAXC_AD_RING)
            RingDevice->Append(devname);

        if(i == input)
            InputDevice->SetStringSelection(devname);

        if(i == output)
            OutputDevice->SetStringSelection(devname);

        if(i == ring)
            RingDevice->SetStringSelection(devname);

        devices++;
    }
}

void PrefsDialog::OnSave(wxCommandEvent &event)
{
    wxConfig *config = new wxConfig("iaxComm");

    config->SetPath("/");

    config->Write("Input Device",   InputDevice->GetStringSelection());
    config->Write("Output Device",  OutputDevice->GetStringSelection());
    config->Write("Ring Device",    RingDevice->GetStringSelection());

    config->Write("Name",           Name->GetValue());
    config->Write("Number",         Number->GetValue());

    config->Write("UseView",        UseView->GetStringSelection());
    config->Write("DefaultServer",  DefaultServer->GetStringSelection());
    config->Write("Intercom",       Intercom->GetValue());
    config->Write("nCalls",         nCalls->GetValue());

    config->Write("AGC",            AGC->GetValue());
    config->Write("NoiseReduce",    NoiseReduce->GetValue());
    config->Write("EchoCancel",     EchoCancel->GetValue());

    delete config;
    SaveButton->Disable();
}

void PrefsDialog::OnApply(wxCommandEvent &event)
{
    SetAudioDevices(InputDevice->GetStringSelection(),
                    OutputDevice->GetStringSelection(),
                    RingDevice->GetStringSelection());

    iaxc_set_callerid((char *)Name->GetValue().c_str(), (char *)Number->GetValue().c_str());

    // Update the main frame
    int which = wxGetApp().theFrame->Server->FindString(DefaultServer->GetStringSelection());
    wxGetApp().theFrame->Server->SetSelection(which);

    // Clear these filters
    int flag = ~(IAXC_FILTER_AGC | IAXC_FILTER_DENOISE | IAXC_FILTER_ECHO);
    iaxc_set_filters(iaxc_get_filters() & flag);

    flag = 0;
    if(AGC->GetValue())
       flag = IAXC_FILTER_AGC;

    if(NoiseReduce->GetValue())
       flag |= IAXC_FILTER_DENOISE;

    if(EchoCancel->GetValue())
       flag |= IAXC_FILTER_ECHO;

    iaxc_set_filters(iaxc_get_filters() | flag);


    Close();
}

void PrefsDialog::OnDirty(wxCommandEvent &event)
{
    SaveButton->Enable();
    ApplyButton->Enable();
}

void SetAudioDevices(wxString inname, wxString outname, 
                                  wxString ringname)
{
    struct iaxc_audio_device *devices;
    int                      nDevs;
    int                      i;
    int                      input  = 0;
    int                      output = 0;
    int                      ring   = 0;

    // Note that if we're called with an invalid devicename, the deviceID
    // stays 0, which equals default.

    iaxc_audio_devices_get(&devices, &nDevs, &input, &output, &ring);

    for(i=0; i<nDevs; i++) {
        if(devices->capabilities & IAXC_AD_INPUT) {
            if(inname.Cmp(devices->name) == 0)
                input = devices->devID;
        }

        if(devices->capabilities & IAXC_AD_OUTPUT) {
            if(outname.Cmp(devices->name) == 0)
                output = devices->devID;
        }

        if(devices->capabilities & IAXC_AD_RING) {
            if(ringname.Cmp(devices->name) == 0)
                ring = devices->devID;
        }
        devices++;
    }
    iaxc_audio_devices_set(input,output,ring);
}
