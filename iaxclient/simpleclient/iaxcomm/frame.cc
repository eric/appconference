//----------------------------------------------------------------------------------------
// Name:        frame.cpp
// Purpose:     Main frame
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

#include "frame.h"  

//----------------------------------------------------------------------------------------
// Remaining headers
// ---------------------------------------------------------------------------------------

#include "app.h"
#include "main.h"
#include "prefs.h"
#include "directory.h"
#include "calls.h"
#include "dial.h"

static bool pttMode;  // are we in PTT mode?
static bool pttState; // is the PTT button pressed?
static bool silenceMode;  // are we in silence suppression mode?

//----------------------------------------------------------------------------------------
// Event table: connect the events to the handler functions to process them
//----------------------------------------------------------------------------------------
BEGIN_EVENT_TABLE(MyFrame, wxFrame)
    EVT_MENU    (IAXCLIENT_EVENT,      MyFrame::HandleEvent)

    EVT_MENU    (XRCID("PTT"),         MyFrame::OnPTTChange)
    EVT_MENU    (XRCID("Silence"),     MyFrame::OnSilenceChange)
    EVT_MENU    (XRCID("Prefs"),       MyFrame::OnPrefs)
    EVT_MENU    (XRCID("Directory"),   MyFrame::OnDirectory)
    EVT_MENU    (XRCID("Exit"),        MyFrame::OnExit)
    EVT_SIZE    (                      CallList::OnSize)  
#ifdef __WXMSW__
    EVT_ICONIZE (                      MyTaskBarIcon::OnHide)
#endif
    EVT_BUTTON  (XRCID("OT0"),         MyFrame::OnOneTouch)
    EVT_BUTTON  (XRCID("OT1"),         MyFrame::OnOneTouch)
    EVT_BUTTON  (XRCID("OT2"),         MyFrame::OnOneTouch)
    EVT_BUTTON  (XRCID("OT3"),         MyFrame::OnOneTouch)
    EVT_BUTTON  (XRCID("OT4"),         MyFrame::OnOneTouch)
    EVT_BUTTON  (XRCID("OT5"),         MyFrame::OnOneTouch)
    EVT_BUTTON  (XRCID("OT6"),         MyFrame::OnOneTouch)
    EVT_BUTTON  (XRCID("OT7"),         MyFrame::OnOneTouch)
    EVT_BUTTON  (XRCID("OT8"),         MyFrame::OnOneTouch)
    EVT_BUTTON  (XRCID("OT9"),         MyFrame::OnOneTouch)
    EVT_BUTTON  (XRCID("OT10"),        MyFrame::OnOneTouch)
    EVT_BUTTON  (XRCID("OT11"),        MyFrame::OnOneTouch)
    EVT_BUTTON  (XRCID("KP0"),         MyFrame::OnKeyPad)
    EVT_BUTTON  (XRCID("KP1"),         MyFrame::OnKeyPad)
    EVT_BUTTON  (XRCID("KP2"),         MyFrame::OnKeyPad)
    EVT_BUTTON  (XRCID("KP3"),         MyFrame::OnKeyPad)
    EVT_BUTTON  (XRCID("KP4"),         MyFrame::OnKeyPad)
    EVT_BUTTON  (XRCID("KP5"),         MyFrame::OnKeyPad)
    EVT_BUTTON  (XRCID("KP6"),         MyFrame::OnKeyPad)
    EVT_BUTTON  (XRCID("KP7"),         MyFrame::OnKeyPad)
    EVT_BUTTON  (XRCID("KP8"),         MyFrame::OnKeyPad)
    EVT_BUTTON  (XRCID("KP9"),         MyFrame::OnKeyPad)
    EVT_BUTTON  (XRCID("KPSTAR"),      MyFrame::OnKeyPad)
    EVT_BUTTON  (XRCID("KPPOUND"),     MyFrame::OnKeyPad)
    EVT_BUTTON  (XRCID("Dial"),        MyFrame::OnDialDirect)
    EVT_BUTTON  (XRCID("Hangup"),      MyFrame::OnHangup)
END_EVENT_TABLE()

//----------------------------------------------------------------------------------------
// Public methods
//----------------------------------------------------------------------------------------

MyFrame::MyFrame( wxWindow* parent )
{
    wxBoxSizer *panelSizer;
    wxPanel    *aPanel;
    wxConfig   *config = new wxConfig("iaxComm");
    wxButton   *ot;
    wxString    OTName;
    wxString    EntryName;
    wxString    ShortName;
    wxString    Label;
    long        dummy;
    bool        bCont;
    MyTimer    *timer;

    // Load up this frame from XRC. [Note, instead of making a class's
    // constructor take a wxWindow* parent with a default value of NULL,
    // we could have just had designed MyFrame class with an empty
    // constructor and then written here:
    // wxXmlResource::Get()->LoadFrame(this, (wxWindow* )NULL, "MyFrame");
    // since this frame will always be the top window, and thus parentless.
    // However, the current approach has source code that can be recycled
    // in case code to moves to having an invisible frame as the top level window.

    wxXmlResource::Get()->LoadFrame( this, parent, "MyFrame" );

    //----Set the icon------------------------------------------------------------------
#ifdef __WXMSW__   
    // XXX under linux, I got "`application_xpm' undeclared (first use this function)" here.
    SetIcon(wxICON(application));
#endif

    //----Add the menu------------------------------------------------------------------
    SetMenuBar( wxXmlResource::Get()->LoadMenuBar( "main_menubar" ) );

    //----Add the statusbar-------------------------------------------------------------
    const int widths[] = {-1, 60};
    CreateStatusBar( 2 );
    SetStatusWidths(2, widths);

    //----Add the panel-----------------------------------------------------------------
    aPanel = new wxPanel(this);
    if(config->Read("/ShowKeyPad",  0l) == 0) {
        aPanel = wxXmlResource::Get()->LoadPanel(this, wxT("Panel"));
    } else {
        aPanel = wxXmlResource::Get()->LoadPanel(this, wxT("FullPanel"));
    }

    //----Reach in for our controls-----------------------------------------------------
    Input      = XRCCTRL(*aPanel, "Input",      wxGauge);
    Output     = XRCCTRL(*aPanel, "Output",     wxGauge);
    Server     = XRCCTRL(*aPanel, "Server",     wxChoice);
    Extension  = XRCCTRL(*aPanel, "Extension",  wxTextCtrl);

    //----Insert the Calls listctrl into it's "unknown" placeholder---------------------
    Calls = new CallList(aPanel);
    wxXmlResource::Get()->AttachUnknownControl("Calls", Calls);

    //----Add Servers-------------------------------------------------------------------
    config->SetPath("/Servers");
    bCont = config->GetFirstGroup(EntryName, dummy);
    while ( bCont ) {
        Server->Append(EntryName);
        bCont = config->GetNextGroup(EntryName, dummy);
    }
    Server->SetSelection(0);

    //----Load up One Touch Keys--------------------------------------------------------
    config->SetPath("/OneTouch");
    bCont = config->GetFirstGroup(OTName, dummy);
    while ( bCont ) {
        ot = XRCCTRL(*aPanel, OTName, wxButton);
        if(ot != NULL) {
            ShortName = OTName + "/ShortName";
            Label = config->Read(ShortName, "");
            if(!Label.IsEmpty()) {
                ot->SetLabel(Label);
            } else {
                ot->SetLabel(OTName);
            }
            EntryName = OTName + "/EntryName";
            Label = config->Read(EntryName, "");
            if(!Label.IsEmpty()) {
                ot->SetToolTip(Label);
            }
        }
        bCont = config->GetNextGroup(OTName, dummy);
    }

    panelSizer = new wxBoxSizer(wxVERTICAL);
    panelSizer->Add(aPanel,1,wxEXPAND);

    SetSizer(panelSizer); 
    panelSizer->SetSizeHints(this);

    pttMode = false;

#ifdef __WXGTK__
    // window used for getting keyboard state
    GdkWindowAttr attr;
    keyStateWindow = gdk_window_new(NULL, &attr, 0);
#endif

    timer = new MyTimer();
    timer->Start(100);
}

MyFrame::~MyFrame()
{
    iaxc_dump_all_calls();
    for(int i=0;i<10;i++) {
        iaxc_millisleep(100);
    }
    iaxc_stop_processing_thread();
//    exit(0); 
    iaxc_shutdown();
}

void MyFrame::OnNotify()
{
    if(pttMode) CheckPTT(); 
}

void MyFrame::OnShow()
{
    Show(TRUE);
}

void MyFrame::OnHangup(wxEvent &event)
{
    iaxc_dump_call();
}

void MyFrame::OnQuit(wxEvent &event)
{
    Close(TRUE);
}

void MyFrame::OnPTTChange(wxCommandEvent &event)
{
    pttMode = event.IsChecked();
    
    if(pttMode) {
        SetPTT(GetPTTState()); 
    } else {
        SetPTT(true); 
        if(silenceMode) {
            iaxc_set_silence_threshold(DEFAULT_SILENCE_THRESHOLD);
            SetStatusText(_("VOX"),1);
        } else {
            iaxc_set_silence_threshold(-99);
            SetStatusText(_(""),1);
        }
        iaxc_set_audio_output(0);  // unmute output
    }
}

void MyFrame::OnSilenceChange(wxCommandEvent &event)
{
    // XXX get the actual state!
    silenceMode =  event.IsChecked();
    
    if(pttMode) return;

    if(silenceMode) {
        iaxc_set_silence_threshold(DEFAULT_SILENCE_THRESHOLD);
        SetStatusText(_("VOX"),1);
    } else {
        iaxc_set_silence_threshold(-99);
        SetStatusText(_(""),1);
    }
}

bool MyFrame::GetPTTState()
{
    bool pressed;
  #ifdef __WXMAC__
    KeyMap theKeys;
    GetKeys(theKeys);
    // that's the Control Key (by experimentation!)
    pressed = theKeys[1] & 0x08;
    //fprintf(stderr, "%p %p %p %p\n", theKeys[0], theKeys[1], theKeys[2], theKeys[3]);
  #else
  #ifdef __WXMSW__
    pressed = GetAsyncKeyState(VK_CONTROL)&0x8000;
  #else
    int x, y;
    GdkModifierType modifiers;
    gdk_window_get_pointer(keyStateWindow, &x, &y, &modifiers);
    pressed = modifiers & GDK_CONTROL_MASK;
  #endif
  #endif
    return pressed;
}

void MyFrame::CheckPTT()
{
    bool newState = GetPTTState();
    if(newState == pttState) return;
 
    SetPTT(newState);
}

void MyFrame::SetPTT(bool state)
{
    pttState = state;
    if(pttState) {
        iaxc_set_silence_threshold(-99); //unmute input
        iaxc_set_audio_output(1);  // unmute output
        SetStatusText(_("TALK"),1);
    } else {
        iaxc_set_silence_threshold(0);  // mute input
        iaxc_set_audio_output(0);  // mute output
        SetStatusText(_("MUTE"),1);
    }
}

void MyFrame::HandleEvent(wxCommandEvent &evt)
{
    iaxc_event *e = (iaxc_event *)evt.GetClientData();
    HandleIAXEvent(e);
    free (e);
}

int MyFrame:: HandleIAXEvent(iaxc_event *e)
{
    int ret = 0;

    switch(e->type) {
      case IAXC_EVENT_LEVELS:
           ret = HandleLevelEvent(e->ev.levels.input, e->ev.levels.output);
           break;
      case IAXC_EVENT_TEXT:
           ret = HandleStatusEvent(e->ev.text.message);
           break;
      case IAXC_EVENT_STATE:
           ret = wxGetApp().theFrame->Calls->HandleStateEvent(e->ev.call);
           break;
      default:
           break;  // not handled
    }
     return ret;
}

int MyFrame:: HandleStatusEvent(char *msg)
{
    wxGetApp().theFrame->SetStatusText(msg);
    return 1;
}

int MyFrame:: HandleLevelEvent(float input, float output)
{
    int inputLevel, outputLevel;

    if (input < LEVEL_MIN)
        input = LEVEL_MIN; 
    else if (input > LEVEL_MAX)
        input = LEVEL_MAX;
    inputLevel = (int)input - (LEVEL_MIN); 

    if (output < LEVEL_MIN)
        output = LEVEL_MIN; 
    else if (input > LEVEL_MAX)
        output = LEVEL_MAX;
    outputLevel = (int)output - (LEVEL_MIN); 

    static int lastInputLevel = 0;
    static int lastOutputLevel = 0;

    if(lastInputLevel != inputLevel) {
      wxGetApp().theFrame->Input->SetValue(inputLevel); 
      lastInputLevel = inputLevel;
    }

    if(lastOutputLevel != outputLevel) {
      wxGetApp().theFrame->Output->SetValue(outputLevel); 
      lastOutputLevel = outputLevel;
    }

    return 1;
}

//----------------------------------------------------------------------------------------
// Private methods
//----------------------------------------------------------------------------------------

void MyFrame::OnExit(wxCommandEvent& WXUNUSED(event))
{
    Close(TRUE);
}

void MyFrame::OnAbout(wxCommandEvent& WXUNUSED(event))
{
    wxString msg;
    msg.Printf( _T("CVS Version"));
    wxMessageBox(msg, _("iax Phone Application"), wxOK | wxICON_INFORMATION, this);
}

void MyFrame::OnPrefs(wxCommandEvent& WXUNUSED(event))
{
    PrefsDialog dialog(this);
    dialog.ShowModal();
}

void MyFrame::OnDirectory(wxCommandEvent& WXUNUSED(event))
{
    DirectoryDialog dialog(this);
    dialog.ShowModal();
}

void MyFrame::OnOneTouch(wxCommandEvent &event)
{
    wxConfig *config = new wxConfig("iaxComm");
    wxString  Message;
    int       OTNo;
    wxString  PathName;
    wxString  EntryName;

    OTNo = event.GetId() - XRCID("OT0");

    PathName.Printf("/OneTouch/OT%d", OTNo);
    config->SetPath(PathName);
    EntryName = config->Read("EntryName", "");

    if(!EntryName.IsEmpty()) {
        DialEntry(EntryName);
    }
}

void MyFrame::OnKeyPad(wxCommandEvent &event)
{
    wxString  Message;
    char      digit;
    int       OTNo;

    OTNo  = event.GetId() - XRCID("KP0");
    digit = '0' + (char)OTNo;

    if(OTNo == 10)
        digit = '*';

    if(OTNo == 11)
        digit = '#';

    iaxc_send_dtmf(digit);
}

void MyFrame::OnDialDirect(wxCommandEvent &event)
{
    if(IsEmpty(Server->GetStringSelection())) {
        // Shouldn't ever get here: we selected Server[0]
        SetStatusText(_T("Please select a server"));
        return;
    }

    wxString S = Server->GetStringSelection();
    wxString E = Extension->GetValue();
    DialDirect(S, E);

}

void MyTimer::Notify()
{
    wxGetApp().theFrame->OnNotify();    
}

