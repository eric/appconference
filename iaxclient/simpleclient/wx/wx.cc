// For compilers that supports precompilation , includes  wx/wx.h  
#include  "wx/wxprec.h"  

#ifndef WX_PRECOMP 
#include  "wx/wx.h"
#endif 

#include  "wx.h"  

#include "iaxclient.h"


/* for the silly key state stuff :( */
#ifdef __WXGTK__
#include <gdk/gdk.h>
#endif

#ifdef __WXMAC__
#include <Carbon/Carbon.h>
#endif

#define LEVEL_MAX -10
#define LEVEL_MIN -50
#define DEFAULT_SILENCE_THRESHOLD -40

IMPLEMENT_APP(IAXClient) 

// event constants
enum
{
    ID_DTMF_1 = 0,
    ID_DTMF_2,
    ID_DTMF_3,
    ID_DTMF_4,
    ID_DTMF_5,
    ID_DTMF_6,
    ID_DTMF_7,
    ID_DTMF_8,
    ID_DTMF_9,
    ID_DTMF_STAR,
    ID_DTMF_O,
    ID_DTMF_POUND,

    ID_DIAL = 100,
    ID_HANG,

    ID_QUIT = 200,

    ID_PTT = 300,
    ID_MUTE,
    ID_SUPPRESS,

    ID_MAX
};

static int inputLevel = 0;
static int outputLevel = 0;

static char *buttonlabels[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9", "*", "0", "#" };



class IAXTimer : public wxTimer
{
  public:
    void IAXTimer::Notify(); 
};



class IAXFrame : public wxFrame
{
public: 
    IAXFrame(const wxChar *title, int xpos, int ypos, int width, int height);

    ~IAXFrame();

    void IAXFrame::OnDTMF(wxEvent &evt);
    void IAXFrame::OnDial(wxEvent &evt);
    void IAXFrame::OnHangup(wxEvent &evt);
    void IAXFrame::OnQuit(wxEvent &evt);
    void IAXFrame::OnPTTChange(wxEvent &evt);

    bool IAXFrame::GetPTTState();
    void IAXFrame::CheckPTT();
    void IAXFrame::SetPTT(bool state);

    wxGauge *input; 
    wxGauge *output; 
    IAXTimer *timer;
    wxTextCtrl *iaxDest;
    wxStaticText *muteState;

    bool pttMode;  // are we in PTT mode?
    bool pttState; // is the PTT button pressed?

protected:
    DECLARE_EVENT_TABLE()

#ifdef __WXGTK__
    GdkWindow *keyStateWindow;
#endif
};

static IAXFrame *theFrame;

void IAXTimer::Notify()
{
    iaxc_process_calls();
    // really shouldn't do this so often..
    if(theFrame->pttMode) theFrame->CheckPTT(); 
}


IAXFrame::IAXFrame(const wxChar *title, int xpos, int ypos, int width, int height)
  : wxFrame((wxFrame *) NULL, -1, title, wxPoint(xpos, ypos), wxSize(width, height))
{
    wxBoxSizer *panelSizer = new wxBoxSizer(wxVERTICAL);
    wxPanel *aPanel = new wxPanel(this);
    wxButton *dialButton;

    wxBoxSizer *topsizer = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer *row1sizer = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer *row3sizer = new wxBoxSizer(wxHORIZONTAL);
    pttMode = false;



    /* add status bar first; otherwise, sizer doesn't take it into
     * account */
    CreateStatusBar();
    SetStatusText("Welcome to IAXClient!");


    /* Set up Menus */
    /* NOTE: Mac doesn't use a File/Exit item, and Application/Quit is
	automatically added for us */
 
#ifndef __WXMAC__
    wxMenu *fileMenu = new wxMenu();
    fileMenu->Append(ID_QUIT, _T("E&xit\tCtrl-X"));
#endif

    wxMenu *optionsMenu = new wxMenu();
    optionsMenu->AppendCheckItem(ID_PTT, _T("Enable &Push to Talk\tCtrl-P"));
   

    wxMenuBar *menuBar = new wxMenuBar();

#ifndef __WXMAC__
    menuBar->Append(fileMenu, _T("&File"));
#endif
    menuBar->Append(optionsMenu, _T("&Options"));

    SetMenuBar(menuBar);

   
    /* DialPad Buttons */
    wxGridSizer *dialpadsizer = new wxGridSizer(3);
    for(int i=0; i<12;i++)
    {
	dialpadsizer->Add(
	  new wxButton(aPanel, i, wxString(buttonlabels[i]),
		  wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT), 
	    1, wxEXPAND|wxALL, 3);
    }
    row1sizer->Add(dialpadsizer,1, wxEXPAND);

    /* volume meters */
    row1sizer->Add(input  = new wxGauge(aPanel, -1, LEVEL_MAX-LEVEL_MIN, wxDefaultPosition, wxSize(15,50), 
	  wxGA_VERTICAL,  wxDefaultValidator, wxString("input level")),0,wxEXPAND); 

    row1sizer->Add(output  = new wxGauge(aPanel, -1, LEVEL_MAX-LEVEL_MIN, wxDefaultPosition, wxSize(15,50), 
	  wxGA_VERTICAL,  wxDefaultValidator, wxString("output level")),0, wxEXPAND); 

    topsizer->Add(row1sizer,1,wxEXPAND);

    /* Destination */
    topsizer->Add(iaxDest = new wxTextCtrl(aPanel, -1, wxString("guest@ast1/8068"), 
	wxDefaultPosition, wxDefaultSize),0,wxEXPAND);

    /* main control buttons */    
    row3sizer->Add(dialButton = new wxButton(aPanel, ID_DIAL, wxString("Dial"),
	    wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT),1, wxEXPAND|wxALL, 3);

    row3sizer->Add(new wxButton(aPanel, ID_HANG, wxString("Hang Up"),
	    wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT),1, wxEXPAND|wxALL, 3);

    /* make dial the default button */
    dialButton->SetDefault();
#if 0
    row3sizer->Add(quitButton = new wxButton(aPanel, 100, wxString("Quit"),
	    wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT),1, wxEXPAND|wxALL, 3);
#endif

    topsizer->Add(row3sizer,0,wxEXPAND);

    topsizer->Add(muteState = new wxStaticText(aPanel,-1,"PTT Disabled",
	    wxDefaultPosition, wxDefaultSize),0,wxEXPAND);


    aPanel->SetSizer(topsizer);
    topsizer->SetSizeHints(aPanel);

    panelSizer->Add(aPanel,1,wxEXPAND);
    SetSizer(panelSizer);	
    panelSizer->SetSizeHints(this);

#ifdef __WXGTK__
    // window used for getting keyboard state
    GdkWindowAttr attr;
    keyStateWindow = gdk_window_new(NULL, &attr, 0);
#endif

    timer = new IAXTimer();
    timer->Start(10);
    //output = new wxGauge(this, -1, 100); 
}

bool IAXFrame::GetPTTState()
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
    pressed = GetAsyncKeyState(VK_CONTROL)|0x8000;
#else
    int x, y;
    GdkModifierType modifiers;
    gdk_window_get_pointer(keyStateWindow, &x, &y, &modifiers);
    pressed = modifiers & GDK_CONTROL_MASK;
#endif
#endif
    return pressed;
}

void IAXFrame::SetPTT(bool state)
{
	pttState = state;
	if(pttState) {
		iaxc_set_silence_threshold(-99); //unmute input
		iaxc_set_audio_output(1);  // mute output
	} else {
		iaxc_set_silence_threshold(0);  // mute input
		iaxc_set_audio_output(0);  // unmute output
	}

        muteState->SetLabel( pttState ? "Talk" : "Mute");
}

void IAXFrame::CheckPTT()
{
    bool newState = GetPTTState();
    if(newState == pttState) return;
 
    SetPTT(newState);
}

void IAXFrame::OnDTMF(wxEvent &evt)
{
	iaxc_send_dtmf(evt.m_id);
}

void IAXFrame::OnDial(wxEvent &evt)
{
	iaxc_call((char *)(theFrame->iaxDest->GetValue().c_str()));
}

void IAXFrame::OnHangup(wxEvent &evt)
{
	iaxc_dump_call();
}

void IAXFrame::OnQuit(wxEvent &evt)
{
	iaxc_dump_call();
	iaxc_process_calls();
	for(int i=0;i<10;i++) {
	  iaxc_millisleep(100);
	  iaxc_process_calls();
	}
	exit(0);	
}

void IAXFrame::OnPTTChange(wxEvent &evt)
{
	// XXX get the actual state!
	pttMode = !pttMode;
	
	if(pttMode) {
		SetPTT(GetPTTState());	
	} else {
		SetPTT(true);	
		iaxc_set_silence_threshold(DEFAULT_SILENCE_THRESHOLD);
		muteState->SetLabel("PTT Disabled");
	}
}


BEGIN_EVENT_TABLE(IAXFrame, wxFrame)
	EVT_BUTTON(0,IAXFrame::OnDTMF)
	EVT_BUTTON(1,IAXFrame::OnDTMF)
	EVT_BUTTON(2,IAXFrame::OnDTMF)
	EVT_BUTTON(3,IAXFrame::OnDTMF)
	EVT_BUTTON(4,IAXFrame::OnDTMF)
	EVT_BUTTON(5,IAXFrame::OnDTMF)
	EVT_BUTTON(6,IAXFrame::OnDTMF)
	EVT_BUTTON(7,IAXFrame::OnDTMF)
	EVT_BUTTON(8,IAXFrame::OnDTMF)
	EVT_BUTTON(9,IAXFrame::OnDTMF)
	EVT_BUTTON(10,IAXFrame::OnDTMF)
	EVT_BUTTON(11,IAXFrame::OnDTMF)
	EVT_BUTTON(ID_DIAL,IAXFrame::OnDial)
	EVT_BUTTON(ID_HANG,IAXFrame::OnHangup)

	EVT_MENU(ID_QUIT,IAXFrame::OnQuit)
	EVT_MENU(ID_PTT,IAXFrame::OnPTTChange)

END_EVENT_TABLE()

IAXFrame::~IAXFrame()
{
#ifdef __WXGTK__
    gdk_window_destroy(keyStateWindow);
#endif
}

bool IAXClient::OnInit() 
{ 
	theFrame = new IAXFrame("IAXTest", 0,0,130,220);

	theFrame->Show(TRUE); 
	SetTopWindow(theFrame); 
    
	doTestCall(0,NULL);
	iaxc_set_error_callback(status_callback);
	iaxc_set_status_callback(status_callback);

	return true; 

}



extern "C" {
   void status_callback(char *msg)
   {
      theFrame->SetStatusText(msg);
   }

   int levels_callback(float input, float output)
   {
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

      theFrame->input->SetValue(inputLevel); 
      theFrame->output->SetValue(outputLevel); 
   }
}
