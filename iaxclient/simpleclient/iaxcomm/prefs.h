//----------------------------------------------------------------------------------------
// Name:        prefs,h
// Purpose:     Describes main dialog
// Author:      Michael Van Donselaar
// Modified by:
// Created:     2003
// Copyright:   (c) Michael Van Donselaar ( michael@vandonselaar.org )
// Licence:     GPL
//----------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------
// Begin single inclusion of this .h file condition
//----------------------------------------------------------------------------------------

#ifndef _PREFS_H_
#define _PREFS_H_

//----------------------------------------------------------------------------------------
// GCC interface
//----------------------------------------------------------------------------------------

#if defined(__GNUG__) && ! defined(__APPLE__)
    #pragma interface "prefs.h"
#endif

//----------------------------------------------------------------------------------------
// Headers
//----------------------------------------------------------------------------------------

#include "app.h"

void         SetAudioDevices(wxString in, wxString out, wxString ring);

class PrefsDialog : public wxDialog
{
public: 
    PrefsDialog( wxWindow* parent );
        
private:

    wxChoice    *InputDevice;
    wxChoice    *OutputDevice;
    wxChoice    *RingDevice;
    wxCheckBox  *ShowKeyPad;
    wxButton    *SaveButton;
    wxButton    *ApplyButton;
    wxButton    *CancelButton;

    void         GetAudioDevices();
    void         OnSave(wxCommandEvent &event);
    void         OnApply(wxCommandEvent &event);
    void         OnDirty(wxCommandEvent &event);

    DECLARE_EVENT_TABLE()

};

#endif  //_PREFS_H_
