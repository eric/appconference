//----------------------------------------------------------------------------------------
// Name:        frame.cc
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
    #pragma implementation "frame.h"
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
#include "devices.h"
#include "directory.h"
#include "calls.h"
#include "dial.h"

static bool pttMode;      // are we in PTT mode?
static bool pttState;     // is the PTT button pressed?
static bool silenceMode;  // are we in silence suppression mode?

//----------------------------------------------------------------------------------------
// Event table: connect the events to the handler functions to process them
//----------------------------------------------------------------------------------------
BEGIN_EVENT_TABLE(MyFrame, wxFrame)
    EVT_MENU    (IAXCLIENT_EVENT,      MyFrame::HandleEvent)

    EVT_MENU    (XRCID("PTT"),         MyFrame::OnPTTChange)
    EVT_MENU    (XRCID("Silence"),     MyFrame::OnSilenceChange)

    EVT_MENU    (XRCID("Prefs"),       MyFrame::OnPrefs)
    EVT_MENU    (XRCID("Devices"),     MyFrame::OnDevices)
    EVT_MENU    (XRCID("Directory"),   MyFrame::OnDirectory)
    EVT_MENU    (XRCID("Exit"),        MyFrame::OnExit)
    EVT_MENU    (XRCID("About"),       MyFrame::OnAbout)
    EVT_SIZE    (                      CallList::OnSize)  
#ifdef __WXMSW__
    EVT_ICONIZE (                      MyTaskBarIcon::OnHide)
#endif
    EVT_BUTTON  (XRCID("0"),           MyFrame::OnOneTouch)
    EVT_BUTTON  (XRCID("1"),           MyFrame::OnOneTouch)
    EVT_BUTTON  (XRCID("2"),           MyFrame::OnOneTouch)
    EVT_BUTTON  (XRCID("3"),           MyFrame::OnOneTouch)
    EVT_BUTTON  (XRCID("4"),           MyFrame::OnOneTouch)
    EVT_BUTTON  (XRCID("5"),           MyFrame::OnOneTouch)
    EVT_BUTTON  (XRCID("6"),           MyFrame::OnOneTouch)
    EVT_BUTTON  (XRCID("7"),           MyFrame::OnOneTouch)
    EVT_BUTTON  (XRCID("8"),           MyFrame::OnOneTouch)
    EVT_BUTTON  (XRCID("9"),           MyFrame::OnOneTouch)
    EVT_BUTTON  (XRCID("10"),          MyFrame::OnOneTouch)
    EVT_BUTTON  (XRCID("11"),          MyFrame::OnOneTouch)
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
    EVT_BUTTON  (XRCID("Next"),        MyFrame::OnNextKey)
    EVT_BUTTON  (XRCID("Hangup"),      MyFrame::OnHangup)

    EVT_BUTTON  (XRCID("AddAccount"),         MyFrame::OnAddAccountList)
    EVT_BUTTON  (XRCID("DeleteAccount"),      MyFrame::OnRemoveAccountList)

    EVT_COMMAND_SCROLL(XRCID("OutputSlider"), MyFrame::OnSlider)
    EVT_TEXT_ENTER    (XRCID("Extension"),    MyFrame::OnDialDirect)
END_EVENT_TABLE()

//----------------------------------------------------------------------------------------
// Public methods
//----------------------------------------------------------------------------------------

MyFrame::MyFrame( wxWindow *parent )
{
    wxConfig   *config = new wxConfig("iaxComm");
    wxMenuBar  *aMenuBar;
    wxString    Name;
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
    SetIcon(wxICON(application));
#endif

    //----Add the menu------------------------------------------------------------------
    aMenuBar =  wxXmlResource::Get()->LoadMenuBar( "main_menubar" );
    SetMenuBar( aMenuBar);

    //----Add the statusbar-------------------------------------------------------------
    const int widths[] = {-1, 60};
    CreateStatusBar( 2 );
    SetStatusWidths(2, widths);

    //----Add the panel-----------------------------------------------------------------
    Name = config->Read("/UseView", "default");
    DefaultAccount = config->Read("/DefaultAccount", "");
    AddPanel(this, Name);

    pttMode = false;

    Speaker     = config->Read("Speaker", 0l);
    AGC         = config->Read("/AGC", 0l);
    NoiseReduce = config->Read("/NoiseReduce", 0l);
    EchoCancel  = config->Read("/EchoCancel", 0l);

    ApplyFilters();

    if(OutputSlider != NULL)
        OutputSlider->SetValue(config->Read("/OutputLevel", 70));

#ifdef __WXGTK__
    // window used for getting keyboard state
    GdkWindowAttr attr;
    keyStateWindow = gdk_window_new(NULL, &attr, 0);
#endif

    timer = new MyTimer();
    timer->Start(100);
}

void MyFrame::RePanel(wxString Name)
{
    aPanel->Destroy();
    AddPanel(this, Name);
    Layout();
}

void MyFrame::AddPanel(wxWindow *parent, wxString Name)
{
    wxBoxSizer *panelSizer;

    aPanel = new wxPanel(parent);
    aPanel = wxXmlResource::Get()->LoadPanel(parent, Name);
    if(aPanel == NULL)
        aPanel = wxXmlResource::Get()->LoadPanel(parent, wxT("default"));

    if(aPanel == NULL)
        wxFatalError(_("Can't Load Panel in frame.cc"));

    //----Reach in for our controls-----------------------------------------------------
    Input        = XRCCTRL(*aPanel, "Input",        wxGauge);
    Output       = XRCCTRL(*aPanel, "Output",       wxGauge);
    OutputSlider = XRCCTRL(*aPanel, "OutputSlider", wxSlider);
    Account      = XRCCTRL(*aPanel, "Account",      wxChoice);
    Extension    = XRCCTRL(*aPanel, "Extension",    wxTextCtrl);

    //----Insert the Calls listctrl into it's "unknown" placeholder---------------------
    Calls = new CallList(aPanel);

    if(Calls == NULL)
        wxFatalError(_("Can't Load CallList in frame.cc"));

    wxXmlResource::Get()->AttachUnknownControl("Calls", Calls);

    ShowDirectoryControls();

    panelSizer = new wxBoxSizer(wxVERTICAL);
    panelSizer->Add(aPanel,1,wxEXPAND);

    SetSizer(panelSizer); 
    panelSizer->SetSizeHints(parent);
}

void MyFrame::ApplyFilters()
{
    // Clear these filters
    int flag = ~(IAXC_FILTER_AGC | IAXC_FILTER_DENOISE | IAXC_FILTER_ECHO);
    iaxc_set_filters(iaxc_get_filters() & flag);

    flag = 0;
    if(AGC)
       flag = IAXC_FILTER_AGC;

    if(NoiseReduce)
       flag |= IAXC_FILTER_DENOISE;

    if(EchoCancel)
       flag |= IAXC_FILTER_ECHO;

    iaxc_set_filters(iaxc_get_filters() | flag);
}

MyFrame::~MyFrame()
{
    iaxc_dump_all_calls();
    for(int i=0;i<10;i++) {
        iaxc_millisleep(100);
    }
    iaxc_stop_processing_thread();

//  This hangs under linux; appears unnecessary under Win32
//  iaxc_shutdown();
}

void MyFrame::OnAddAccountList(wxCommandEvent &event)
{
    wxString val;

    AddAccountDialog dialog(this, val);
    dialog.ShowModal();
    ShowDirectoryControls();
}

void MyFrame::OnRemoveAccountList(wxCommandEvent &event)
{
    wxConfig  *config = new wxConfig("iaxComm");
    long       sel = -1;

    sel = Account->GetSelection();

    if(sel >= 0) {
        config->DeleteGroup("/Accounts/"+ Account->GetStringSelection());
        Account->Delete(sel);
    }

    delete config;
    ShowDirectoryControls();
}

void MyFrame::ShowDirectoryControls()
{
    wxConfig   *config = new wxConfig("iaxComm");
    wxButton   *ot;
    wxString    OTName;
    wxString    DialString;
    wxString    Name;
    wxString    Label;
    long        dummy;
    bool        bCont;

    //----Add Accounts-------------------------------------------------------------------
    Account->Clear();
    config->SetPath("/Accounts");
    bCont = config->GetFirstGroup(Name, dummy);
    while ( bCont ) {
        Account->Append(Name);
        bCont = config->GetNextGroup(Name, dummy);
    }

    Account->SetSelection(Account->FindString(DefaultAccount));

    //----Load up One Touch Keys--------------------------------------------------------
    config->SetPath("/OT");
    bCont = config->GetFirstGroup(OTName, dummy);
    while ( bCont ) {
        ot = XRCCTRL(*aPanel, OTName, wxButton);
        if(ot != NULL) {
            Name = OTName + "/Name";
            Label = config->Read(Name, "");
            if(!Label.IsEmpty()) {
                ot->SetLabel(Label);
            } else {
                ot->SetLabel("OT" + OTName);
            }
            DialString = OTName + "/Extension";
            Label = config->Read(DialString, "");
            if(!Label.IsEmpty()) {
                ot->SetToolTip(Label);
            }
        }
        bCont = config->GetNextGroup(OTName, dummy);
    }
}

void MyFrame::OnNotify()
{
    if(pttMode) CheckPTT(); 
}

void MyFrame::OnShow()
{
    Show(TRUE);
}

void MyFrame::OnNextKey(wxEvent &event)
{
    int n = iaxc_first_free_call();

    if(n < 0)
        wxMessageBox(_("Sorry, there are no free calls"), "Oops");
    else
        iaxc_select_call(n);
}

void MyFrame::OnHangup(wxEvent &event)
{
    iaxc_dump_call();
}

void MyFrame::OnQuit(wxEvent &event)
{
    Close(TRUE);
}

void MyFrame::OnSlider(wxScrollEvent &event)
{
    int      pos = event.GetPosition();

    iaxc_output_level_set((double)pos/100.0);
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

int MyFrame::HandleIAXEvent(iaxc_event *e)
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

int MyFrame::HandleStatusEvent(char *msg)
{
    wxGetApp().theFrame->SetStatusText(msg);
    return 1;
}

int MyFrame::HandleLevelEvent(float input, float output)
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

void MyFrame::OnDevices(wxCommandEvent& WXUNUSED(event))
{
    DevicesDialog dialog(this);
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
    wxString  DialString;

    OTNo = event.GetId() - XRCID("0");

    PathName.Printf("/OT/%d", OTNo);
    config->SetPath(PathName);
    DialString = config->Read("Extension", "");

    if(DialString.IsEmpty())
        return;

    // A DialString in quotes means look up name in phone book
    if(DialString.StartsWith("\"")) {
        DialString = DialString.Mid(1, DialString.Len() -2);
        DialString = config->Read("/PhoneBook/" + DialString + "/Extension", "");
    }
    Dial(DialString);
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
    Extension->WriteText(digit);
}

void MyFrame::OnDialDirect(wxCommandEvent &event)
{
    Dial(Extension->GetValue());
}

void MyTimer::Notify()
{
    wxGetApp().theFrame->OnNotify();    
}

