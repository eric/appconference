//----------------------------------------------------------------------------------------
// Name:        directory.h
// Purpose:     Describes directory dialog
// Author:      Michael Van Donselaar
// Modified by:
// Created:     2003
// Copyright:   (c) Michael Van Donselaar ( michael@vandonselaar.org )
// Licence:     GPL
//----------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------
// Begin single inclusion of this .h file condition
//----------------------------------------------------------------------------------------

#ifndef _DIRECTORY_H_
#define _DIRECTORY_H_

//----------------------------------------------------------------------------------------
// GCC interface
//----------------------------------------------------------------------------------------

#if defined(__GNUG__) && ! defined(__APPLE__)
    #pragma interface "directory.h"
#endif

//----------------------------------------------------------------------------------------
// Headers
//----------------------------------------------------------------------------------------

#include "app.h"

class DirectoryDialog : public wxDialog
{
public: 
    DirectoryDialog( wxWindow* parent );

    wxComboBox  *ServerName;
    wxTextCtrl  *HostName;
    wxTextCtrl  *UserName;
    wxTextCtrl  *Password;
    wxTextCtrl  *Confirm;
    wxCheckBox  *Default;
    wxComboBox  *EntryName;
    wxChoice    *ChooseServer;
    wxTextCtrl  *Extension;
    wxSpinCtrl  *OTNo;
    wxTextCtrl  *ShortName;
    wxChoice    *ChooseEntry;
    wxButton    *SaveButton;
    wxButton    *ApplyButton;
        
private:

    wxNotebook  *DirectoryNotebook;
    void         OnServerName(wxCommandEvent &event);
    void         OnEntryName(wxCommandEvent &event);
    void         OnOTNo(wxCommandEvent &event);
    void         OnDone(wxCommandEvent &event);
    void         OnSave(wxCommandEvent &event);
    void         OnRemove(wxCommandEvent &event);
    void         OnDial(wxCommandEvent &event);

    DECLARE_EVENT_TABLE()

};

#endif  //_DIRECTORY_H_

