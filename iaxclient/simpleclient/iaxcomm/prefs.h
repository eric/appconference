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

void         SetCallerID(wxString name, wxString number);

class PrefsDialog : public wxDialog
{
public: 
    PrefsDialog( wxWindow* parent );
        
private:

    wxTextCtrl  *RingBack;
    wxTextCtrl  *RingTone;
    wxTextCtrl  *Intercom;

    wxTextCtrl  *Name;
    wxTextCtrl  *Number;
    wxComboBox  *UseView;
    wxChoice    *DefaultAccount;
    wxTextCtrl  *IntercomPass;
    wxSpinCtrl  *nCalls;
    wxCheckBox  *AGC;
    wxCheckBox  *NoiseReduce;
    wxCheckBox  *EchoCancel;
    wxButton    *SaveButton;
    wxButton    *ApplyButton;

    void         OnBrowse(wxCommandEvent &event);
    void         OnSaveAudio(wxCommandEvent &event);
    void         OnSaveCallerID(wxCommandEvent &event);
    void         OnSaveMisc(wxCommandEvent &event);
    void         OnSaveFilters(wxCommandEvent &event);
    void         OnApplyAudio(wxCommandEvent &event);
    void         OnApplyCallerID(wxCommandEvent &event);
    void         OnApplyMisc(wxCommandEvent &event);
    void         OnApplyFilters(wxCommandEvent &event);
    void         ApplyFilters(void);

    DECLARE_EVENT_TABLE()

};

#endif  //_PREFS_H_
