//----------------------------------------------------------------------------------------
// Name:        frame,h
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

#ifndef _FRAME_H_
#define _FRAME_H_

//----------------------------------------------------------------------------------------
// GCC interface
//----------------------------------------------------------------------------------------

#if defined(__GNUG__) && ! defined(__APPLE__)
    #pragma interface "frame.h"
#endif

//----------------------------------------------------------------------------------------
// Headers
//----------------------------------------------------------------------------------------

#include "app.h"
#include "calls.h"
#include "directory.h"
#include "wx/menu.h"

#define LEVEL_MAX -40
#define LEVEL_MIN -50
#define DEFAULT_SILENCE_THRESHOLD 1 // positive is "auto"

//----------------------------------------------------------------------------------------
// Class definition: MyTimer
//----------------------------------------------------------------------------------------

class MyTimer : public wxTimer
{

public:

    void        Notify();
};

//----------------------------------------------------------------------------------------
// Class definition: MyFrame
//----------------------------------------------------------------------------------------

class MyFrame : public wxFrame
{

public:
    
    MyFrame( wxWindow* parent=(wxWindow *)NULL);
   ~MyFrame();
   
    void        ShowDirectoryControls();
    void        OnShow();
    void        OnNotify();
    void        OnHangup(wxEvent &event);
    void        OnNextKey(wxEvent &event);
    void        OnQuit(wxEvent &event);
    void        OnPTTChange(wxCommandEvent &event);
    void        OnSilenceChange(wxCommandEvent &event);
    void        OnSlider(wxScrollEvent &event);
    bool        GetPTTState();
    void        CheckPTT();
    void        SetPTT(bool state);
    void        OnAddAccountList(wxCommandEvent &event);
    void        OnRemoveAccountList(wxCommandEvent &event);
    void        RePanel(wxString Name);

    // Handlers for library-initiated events
    void        HandleEvent(wxCommandEvent &evt);
    int         HandleIAXEvent(iaxc_event *e);
    int         HandleStatusEvent(char *msg);
    int         HandleLevelEvent(float input, float output);

    wxGauge    *Input;
    wxGauge    *Output;
    wxSlider   *OutputSlider;
    wxChoice   *Account;
    wxTextCtrl *Extension;
    CallList   *Calls;

    // From wxConfig
    int         nCalls;
    wxString    Name;
    wxString    Number;
    wxString    RingToneName;
    wxString    RingBackName;
    wxString    IntercomName;
    wxString    DefaultAccount;
    wxString    IntercomPass;

    bool        Speaker;
    bool        AGC;
    bool        NoiseReduce;
    bool        EchoCancel;
    
private:

    // An icon for the corner of dialog and application's taskbar button
    wxIcon      m_icon;
    wxPanel    *aPanel;
    void        OnPrefs(wxCommandEvent& event);
    void        OnDevices(wxCommandEvent& event);
    void        OnDirectory(wxCommandEvent& event);
    void        OnExit(wxCommandEvent& event);
    void        OnAbout(wxCommandEvent& event);
    void        OnOneTouch(wxCommandEvent& event);
    void        OnKeyPad(wxCommandEvent& event);
    void        OnDialDirect(wxCommandEvent& event);
    void        AddPanel(wxWindow *parent, wxString Name);
    void        ApplyFilters(void);

#ifdef __WXGTK__
    GdkWindow *keyStateWindow;
#endif

    DECLARE_EVENT_TABLE()
};

//----------------------------------------------------------------------------------------
// End single inclusion of this .h file condition
//----------------------------------------------------------------------------------------

#endif  // _FRAME_H_
