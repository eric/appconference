//----------------------------------------------------------------------------------------
// Name:        devices.cc
// Purpose:     devices dialog
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
    #pragma implementation "devices.h"
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
#include "devices.h"

//----------------------------------------------------------------------------------------
// Remaining headers
// ---------------------------------------------------------------------------------------

#include "app.h"
#include "frame.h"
#include "main.h"

//----------------------------------------------------------------------------------------
// Event table: connect the events to the handler functions to process them
//----------------------------------------------------------------------------------------
BEGIN_EVENT_TABLE(DevicesDialog, wxDialog)
    EVT_CHOICE(  XRCID("InputDevice"),        DevicesDialog::OnDirty)
    EVT_CHOICE(  XRCID("OutputDevice"),       DevicesDialog::OnDirty)
    EVT_CHOICE(  XRCID("RingDevice"),         DevicesDialog::OnDirty)
    EVT_CHECKBOX(XRCID("Speaker"),            DevicesDialog::OnDirty)
    EVT_BUTTON( wxID_SAVE,                    DevicesDialog::OnSave)
    EVT_BUTTON( wxID_APPLY,                   DevicesDialog::OnApply)
END_EVENT_TABLE()

//----------------------------------------------------------------------------------------
// Public methods
//----------------------------------------------------------------------------------------

DevicesDialog::DevicesDialog(wxWindow* parent)
{    
    wxXmlResource::Get()->LoadDialog(this, parent, wxT("Devices"));

    // Reach in for our controls

    InputDevice    = XRCCTRL(*this, "InputDevice",    wxChoice);
    OutputDevice   = XRCCTRL(*this, "OutputDevice",   wxChoice);
    RingDevice     = XRCCTRL(*this, "RingDevice",     wxChoice);
    Speaker        = XRCCTRL(*this, "Speaker",        wxCheckBox);

    SaveButton     = XRCCTRL(*this, "wxID_SAVE",      wxButton);
    ApplyButton    = XRCCTRL(*this, "wxID_APPLY",     wxButton);
    CancelButton   = XRCCTRL(*this, "wxID_CANCEL",    wxButton);

    GetAudioDevices();
    Speaker->SetValue(wxGetApp().theFrame->Speaker);
}

//----------------------------------------------------------------------------------------
// Private methods
//----------------------------------------------------------------------------------------

void DevicesDialog::GetAudioDevices()
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

void DevicesDialog::OnSave(wxCommandEvent &event)
{
    wxConfig *config = new wxConfig("iaxComm");

    config->SetPath("/");

    config->Write("Input Device",   InputDevice->GetStringSelection());
    config->Write("Output Device",  OutputDevice->GetStringSelection());
    config->Write("Ring Device",    RingDevice->GetStringSelection());
    config->Write("Speaker",        Speaker->GetValue());
    delete config;
    SaveButton->Disable();
    OnApply(event);
}

void DevicesDialog::OnApply(wxCommandEvent &event)
{
    SetAudioDevices(InputDevice->GetStringSelection(),
                    OutputDevice->GetStringSelection(),
                    RingDevice->GetStringSelection());
    wxGetApp().theFrame->Speaker = Speaker->GetValue();

    ApplyButton->Disable();
    CancelButton->SetLabel("Done");
}

void DevicesDialog::OnDirty(wxCommandEvent &event)
{
    SaveButton->Enable();
    ApplyButton->Enable();
    CancelButton->SetLabel("Cancel");
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
