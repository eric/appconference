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
#include "accounts.h"
#include "calls.h"
#include "dial.h"
#include "wx/statusbr.h"

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

    EVT_MENU    (XRCID("Accounts"),    MyFrame::OnAccounts)
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
    EVT_BUTTON  (XRCID("Transfer"),    MyFrame::OnTransfer)
    EVT_BUTTON  (XRCID("Hold"),        MyFrame::OnHoldKey)
    EVT_BUTTON  (XRCID("Speaker"),     MyFrame::OnSpeakerKey)
    EVT_BUTTON  (XRCID("Hangup"),      MyFrame::OnHangup)

    EVT_CHOICE  (XRCID("Account"),     MyFrame::OnAccountChoice)

    EVT_COMMAND_SCROLL(XRCID("OutputSlider"), MyFrame::OnOutputSlider)
    EVT_COMMAND_SCROLL(XRCID("InputSlider"),  MyFrame::OnInputSlider)
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

    //----Set some preferences ---------------------------------------------------------
    config->SetPath("/Prefs");

    RingOnSpeaker = config->Read("RingOnSpeaker", 0l);
    AGC           = config->Read("AGC", 0l);
    AAGC          = config->Read("AAGC", 1l);
    CN            = config->Read("CN", 1l);
    NoiseReduce   = config->Read("NoiseReduce", 1l);
    EchoCancel    = config->Read("EchoCancel", 0l);

    config->SetPath("/Codecs");

    AllowuLawVal    = config->Read("AllowuLaw",  0l);
    AllowaLawVal    = config->Read("AllowaLaw",  0l);
    AllowGSMVal     = config->Read("AllowGSM",   0l);
    AllowSpeexVal   = config->Read("AllowSpeex", 1l);
    AllowiLBCVal    = config->Read("AllowiLBC",  0l);
    PreferredBitmap = config->Read("Preferred",  IAXC_FORMAT_SPEEX);

    config->SetPath("/Codecs/SpeexTune");

    SPXEnhanceVal   = config->Read("SPXEnhance",     1l);
    SPXQualityVal   = config->Read("SPXQuality",     4l);
    SPXBitrateVal   = config->Read("SPXBitrate",     9l);
    SPXABRVal       = config->Read("SPXABR",         9l);
    SPXVBRVal       = config->Read("SPXVBR",         1l);
    SPXComplexityVal= config->Read("SPXComplexity",  4l);

    //----Add the panel-----------------------------------------------------------------
    Name = config->Read("UseSkin", "default");
    AddPanel(this, Name);

    pttMode = false;

    wxGetApp().InputDevice     = config->Read("Input Device", "");
    wxGetApp().OutputDevice    = config->Read("Output Device", "");
    wxGetApp().SpkInputDevice  = config->Read("Speaker Input Device",  
                                              wxGetApp().InputDevice);
    wxGetApp().SpkOutputDevice = config->Read("Speaker Output Device", 
                                              wxGetApp().OutputDevice);
    wxGetApp().RingDevice      = config->Read("Ring Device", "");

    ApplyFilters();
    ApplyCodecs();
    UsingSpeaker = false;

    if(OutputSlider != NULL)
        OutputSlider->SetValue(config->Read("OutputLevel", 70));

    if(InputSlider != NULL)
        InputSlider->SetValue(config->Read("InputLevel", 70));

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
    InputSlider  = XRCCTRL(*aPanel, "InputSlider",  wxSlider);
    Account      = XRCCTRL(*aPanel, "Account",      wxChoice);
    Extension    = XRCCTRL(*aPanel, "Extension",    wxComboBox);

    //----Insert the Calls listctrl into it's "unknown" placeholder---------------------
    Calls = new CallList(aPanel, wxGetApp().nCalls);

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
    int flag = ~(IAXC_FILTER_AGC | IAXC_FILTER_AAGC | IAXC_FILTER_CN |
                 IAXC_FILTER_DENOISE | IAXC_FILTER_ECHO);
    iaxc_set_filters(iaxc_get_filters() & flag);

    flag = 0;
    if(AGC)
       flag = IAXC_FILTER_AGC;

    if(AAGC)
       flag = IAXC_FILTER_AAGC;

    if(CN)
       flag = IAXC_FILTER_CN;

    if(NoiseReduce)
       flag |= IAXC_FILTER_DENOISE;

    if(EchoCancel)
       flag |= IAXC_FILTER_ECHO;

    iaxc_set_filters(iaxc_get_filters() | flag);
}

void MyFrame::ApplyCodecs()
{
    wxConfig   *config = new wxConfig("iaxComm");

    int  Allowed = 0;

    if(AllowiLBCVal)
        Allowed |= IAXC_FORMAT_ILBC;
    
    if(AllowGSMVal)
        Allowed |= IAXC_FORMAT_GSM;
     
    if(AllowSpeexVal)   
        Allowed |= IAXC_FORMAT_SPEEX;
    
    if(AllowuLawVal)
        Allowed |= IAXC_FORMAT_ULAW;
    
    if(AllowaLawVal)
        Allowed |= IAXC_FORMAT_ALAW;

    iaxc_set_formats(PreferredBitmap, Allowed);

    iaxc_set_speex_settings(   (int)SPXEnhanceVal,
                             (float)SPXQualityVal,
                               (int)SPXBitrateVal,
                               (int)SPXVBRVal,
                               (int)SPXABRVal,
                               (int)SPXComplexityVal);
}

MyFrame::~MyFrame()
{
    iaxc_dump_all_calls();
    for(int i=0;i<10;i++) {
        iaxc_millisleep(100);
    }
    iaxc_stop_processing_thread();
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
    if(Account != NULL) {
        Account->Clear();
        config->SetPath("/Accounts");
        bCont = config->GetFirstGroup(Name, dummy);
        while ( bCont ) {
            Account->Append(Name);
            bCont = config->GetNextGroup(Name, dummy);
        }

        Account->SetSelection(Account->FindString(wxGetApp().DefaultAccount));
    }

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

    //----Load up Extension ComboBox----------------------------------------------------
    config->SetPath("/PhoneBook");
    bCont = config->GetFirstGroup(Name, dummy);
    while ( bCont ) {
        Extension->Append(Name);
        bCont = config->GetNextGroup(Name, dummy);
    }
}

void MyFrame::OnNotify()
{
    MessageTicks++;

    if(MessageTicks > 30) {
        MessageTicks = 0;
        wxGetApp().theFrame->SetStatusText(_T(""));
    }

    if(pttMode) CheckPTT(); 
}

void MyFrame::OnShow()
{
    Show(TRUE);
}

void MyFrame::OnSpeakerKey(wxEvent &event)
{
    if(UsingSpeaker != true) {
        UsingSpeaker = true;
        SetAudioDevices(wxGetApp().SpkInputDevice,
                        wxGetApp().SpkOutputDevice,
                        wxGetApp().RingDevice);
    } else {
        UsingSpeaker = false;
        SetAudioDevices(wxGetApp().InputDevice,
                        wxGetApp().OutputDevice,
                        wxGetApp().RingDevice);
    }
}

void MyFrame::OnHoldKey(wxEvent &event)
{
    int selected = iaxc_selected_call();

    if(selected < 0)
        return;

    iaxc_quelch(selected, 1);
    iaxc_select_call(-1);
}

void MyFrame::OnHangup(wxEvent &event)
{
    iaxc_dump_call();
}

void MyFrame::OnQuit(wxEvent &event)
{
    Close(TRUE);
}

void MyFrame::OnOutputSlider(wxScrollEvent &event)
{
    int      pos = event.GetPosition();

    iaxc_output_level_set((double)pos/100.0);
}

void MyFrame::OnInputSlider(wxScrollEvent &event)
{
    int      pos = event.GetPosition();

    iaxc_input_level_set((double)pos/100.0);
}

void MyFrame::OnPTTChange(wxCommandEvent &event) {
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
    MessageTicks = 0;
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

    if(wxGetApp().theFrame->Input != NULL) {
      if(lastInputLevel != inputLevel) {
        wxGetApp().theFrame->Input->SetValue(inputLevel); 
        lastInputLevel = inputLevel;
      }
    }

    if(wxGetApp().theFrame->Output != NULL) {
      if(lastOutputLevel != outputLevel) {
        wxGetApp().theFrame->Output->SetValue(outputLevel); 
        lastOutputLevel = outputLevel;
      }
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
    msg.Printf("Version: %s", VERSION);
    wxMessageBox(msg, _("iaxComm"), wxOK | wxICON_INFORMATION, this);
}

void MyFrame::OnAccounts(wxCommandEvent& WXUNUSED(event))
{
    AccountsDialog dialog(this);
    dialog.ShowModal();
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
    Extension->SetValue(Extension->GetValue()+digit);
}

void MyFrame::OnDialDirect(wxCommandEvent &event)
{
    wxString  DialString;
    wxString  Value       = Extension->GetValue();
    wxConfig *config      = new wxConfig("iaxComm");

    DialString = config->Read("/PhoneBook/" + Value + "/Extension", "");

    if(DialString.IsEmpty()) {
        Dial(Value);
    } else {
        Dial(DialString);
    }
    Extension->Clear();
}

void MyFrame::OnTransfer(wxCommandEvent &event)
{
    //Transfer
    int      selected = iaxc_selected_call();
    char     ext[256];
    wxString Title;

    Title.Printf("Transfer Call %d", selected);
    wxTextEntryDialog dialog(this,
                             _T("Target Extension:"),
                             Title,
                             _T(""),
                             wxOK | wxCANCEL);

    if(dialog.ShowModal() == wxID_OK) {
        strncpy(ext, dialog.GetValue().c_str(), 256);
        iaxc_blind_transfer_call(selected, ext);
    }
}

void MyFrame::OnAccountChoice(wxCommandEvent &event)
{
    wxGetApp().DefaultAccount = Account->GetStringSelection();
}

void MyTimer::Notify()
{
    wxGetApp().theFrame->OnNotify();    
}

