// For compilers that supports precompilation , includes  wx/wx.h  
#include  "wx/wxprec.h"  

#ifndef WX_PRECOMP 
#include  "wx/wx.h"
#endif 

#include "wx/cmdline.h"
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
#define DEFAULT_SILENCE_THRESHOLD 1 // positive is "auto"

#define TRY_GUILOCK

class IAXClient : public wxApp
{
          public:
	          virtual bool OnInit();
		  virtual bool OnCmdLineParsed(wxCmdLineParser& p);
		  virtual void OnInitCmdLine(wxCmdLineParser& p);

		  bool optNoDialPad;
		  wxString optDestination;
};

DECLARE_APP(IAXClient)

IMPLEMENT_APP(IAXClient) 

// forward decls for callbacks
extern "C" {
     void status_callback(char *msg);
     int levels_callback(float input, float output);
     int doTestCall(int ac, char **av);
}



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
    ID_SILENCE,

    CALLBACK_STATUS = 1000,
    CALLBACK_LEVELS,

    ID_MAX
};


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
    void IAXFrame::OnPTTChange(wxCommandEvent &evt);
    void IAXFrame::OnSilenceChange(wxCommandEvent &evt);
    void IAXFrame::OnNotify(void);

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
    bool silenceMode;  // are we in silence suppression mode?

#ifndef TRY_GUILOCK
    // values set by callbacks
    int	      inputLevel;
    int	      outputLevel;
    wxString  statusString;
#endif

protected:
    DECLARE_EVENT_TABLE()

#ifdef __WXGTK__
    GdkWindow *keyStateWindow;
#endif
};

static IAXFrame *theFrame;

void IAXFrame::OnNotify()
{
#ifndef TRY_GUILOCK
    static wxString lastStatus;
    static int lastInputLevel = 0;
    static int lastOutputLevel = 0;

    if(statusString != lastStatus) {
      SetStatusText(statusString);
      lastStatus = statusString;
    }

    if(lastInputLevel != inputLevel) {
      input->SetValue(inputLevel); 
      lastInputLevel = inputLevel;
    }

    if(lastOutputLevel != outputLevel) {
      output->SetValue(outputLevel); 
      lastOutputLevel = outputLevel;
    }
#endif

    if(pttMode) CheckPTT(); 
}

void IAXTimer::Notify()
{
    theFrame->OnNotify();
}


IAXFrame::IAXFrame(const wxChar *title, int xpos, int ypos, int width, int height)
  : wxFrame((wxFrame *) NULL, -1, title, wxPoint(xpos, ypos), wxSize(width, height))
{
    wxBoxSizer *panelSizer = new wxBoxSizer(wxVERTICAL);
    wxPanel *aPanel = new wxPanel(this);
    wxButton *dialButton;

    wxBoxSizer *topsizer = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer *row3sizer = new wxBoxSizer(wxHORIZONTAL);
    pttMode = false;
#ifndef TRY_GUILOCK
    inputLevel = 0;
    outputLevel = 0;
    statusString = _T("Welcome to IAXClient");
#endif



    /* add status bar first; otherwise, sizer doesn't take it into
     * account */
    CreateStatusBar();


    /* Set up Menus */
    /* NOTE: Mac doesn't use a File/Exit item, and Application/Quit is
	automatically added for us */
 
#ifndef __WXMAC__
    wxMenu *fileMenu = new wxMenu();
    fileMenu->Append(ID_QUIT, _T("E&xit\tCtrl-X"));
#endif

    wxMenu *optionsMenu = new wxMenu();
    optionsMenu->AppendCheckItem(ID_PTT, _T("Enable &Push to Talk\tCtrl-P"));
    optionsMenu->AppendCheckItem(ID_SILENCE, _T("Enable &Silence Suppression\tCtrl-S"));
   

    wxMenuBar *menuBar = new wxMenuBar();

#ifndef __WXMAC__
    menuBar->Append(fileMenu, _T("&File"));
#endif
    menuBar->Append(optionsMenu, _T("&Options"));

    SetMenuBar(menuBar);

    wxBoxSizer *row1sizer; 
    if(wxGetApp().optNoDialPad) {
	row1sizer = new wxBoxSizer(wxVERTICAL);

	/* volume meters */
	row1sizer->Add(input  = new wxGauge(aPanel, -1, LEVEL_MAX-LEVEL_MIN, wxDefaultPosition, wxSize(15,15), 
	      wxGA_HORIZONTAL,  wxDefaultValidator, _T("input level")),1, wxEXPAND); 

	row1sizer->Add(output  = new wxGauge(aPanel, -1, LEVEL_MAX-LEVEL_MIN, wxDefaultPosition, wxSize(15,15), 
	      wxGA_HORIZONTAL,  wxDefaultValidator, _T("output level")),1, wxEXPAND); 
    } else {
	row1sizer = new wxBoxSizer(wxHORIZONTAL);
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
	      wxGA_VERTICAL,  wxDefaultValidator, _T("input level")),0, wxEXPAND); 

	row1sizer->Add(output  = new wxGauge(aPanel, -1, LEVEL_MAX-LEVEL_MIN, wxDefaultPosition, wxSize(15,50), 
	      wxGA_VERTICAL,  wxDefaultValidator, _T("output level")),0, wxEXPAND); 
    }

    topsizer->Add(row1sizer,1,wxEXPAND);

    /* Destination */
    topsizer->Add(iaxDest = new wxTextCtrl(aPanel, -1, _T("guest@ast1/8068"), 
	wxDefaultPosition, wxDefaultSize),0,wxEXPAND);

    /* main control buttons */    
    row3sizer->Add(dialButton = new wxButton(aPanel, ID_DIAL, _T("Dial"),
	    wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT),1, wxEXPAND|wxALL, 3);

    row3sizer->Add(new wxButton(aPanel, ID_HANG, _T("Hang Up"),
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
    timer->Start(100);
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
	iaxc_send_dtmf(*buttonlabels[evt.m_id]);
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
	for(int i=0;i<10;i++) {
	  iaxc_millisleep(100);
	}
	iaxc_stop_processing_thread();
	exit(0);	
}

void IAXFrame::OnPTTChange(wxCommandEvent &evt)
{
	pttMode = evt.IsChecked();
	
	if(pttMode) {
		SetPTT(GetPTTState());	
	} else {
		SetPTT(true);	
		if(silenceMode) {
			iaxc_set_silence_threshold(DEFAULT_SILENCE_THRESHOLD);
		} else {
			iaxc_set_silence_threshold(-99);
		}
		iaxc_set_audio_output(0);  // unmute output
		muteState->SetLabel("PTT Disabled");
	}
}

void IAXFrame::OnSilenceChange(wxCommandEvent &evt)
{
	// XXX get the actual state!
	silenceMode =  evt.IsChecked();
	
	if(pttMode) return;

	if(silenceMode) {
		iaxc_set_silence_threshold(DEFAULT_SILENCE_THRESHOLD);
	} else {
		iaxc_set_silence_threshold(-99);
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
	EVT_MENU(ID_SILENCE,IAXFrame::OnSilenceChange)

END_EVENT_TABLE()

IAXFrame::~IAXFrame()
{
#ifdef __WXGTK__
#if 0
    // apparently, I'm not supposed to destroy this, cause it's a "root" window
    gdk_window_destroy(keyStateWindow);
#endif
#endif
    iaxc_stop_processing_thread();
    iaxc_shutdown();
}


void IAXClient::OnInitCmdLine(wxCmdLineParser& p)
{
     // declare our CmdLine options and stuff.
     p.AddSwitch(_T("d"),_T("disable-dialpad"),_T("Disable Dial Pad"));
     p.AddParam(_T("destination"),wxCMD_LINE_VAL_STRING,wxCMD_LINE_PARAM_OPTIONAL);

}

bool IAXClient::OnCmdLineParsed(wxCmdLineParser& p)
{
    if(p.Found(_T("d"))) { 
	optNoDialPad = true;
	//fprintf(stderr, "-d option found\n");
    }
    if(p.GetParamCount() >= 1) {
	optDestination=p.GetParam(0);
	//fprintf(stderr, "dest is %s\n", optDestination.c_str());
    }

    return true;
}

bool IAXClient::OnInit() 
{ 
	optNoDialPad = false;

	if(!wxApp::OnInit())
	  return false;

	theFrame = new IAXFrame("IAXTest", 0,0,130,220);


	theFrame->Show(TRUE); 
	SetTopWindow(theFrame); 
    
        iaxc_initialize(AUDIO_INTERNAL_PA);

        iaxc_set_encode_format(IAXC_FORMAT_GSM);
        iaxc_set_silence_threshold(-99);
	iaxc_set_levels_callback(levels_callback);
	iaxc_set_error_callback(status_callback);
	iaxc_set_status_callback(status_callback);

        iaxc_start_processing_thread();

	if(!optDestination.IsEmpty()) 
	    iaxc_call((char *)optDestination.c_str());
    

	return true; 
}



extern "C" {

   /* yes, using member variables, and polling for changes
    * isn't really a nice way to do callbacks.  BUT, I've tried the
    * "right" ways, unsuccessfully:
    *
    * 1) I've tried using wxGuiMutexEnter/Leave, but that deadlocks (and it should, I guess),
    * because sometimes these _are_ called in response to GUI events (i.e. the status callback eventually
    * gets called when you press hang up.
    *
    * 2) I've tried using events and ::wxPostEvent, but that also deadlocks (at least on GTK),
    * because it calls wxWakeUpIdle, which calls wxGuiMutexEnter, which deadlocks also, for some reason.
    *
    * So, this isn't ideal, but it works, until I can figure out a better way
    */
   void status_callback(char *msg)
   {
#ifdef TRY_GUILOCK
      static wxString lastStatus;
      if(lastStatus == msg) return;

      if (!wxThread::IsMain()) wxMutexGuiEnter();
      theFrame->SetStatusText(msg);
      lastStatus = msg;
      if (!wxThread::IsMain()) wxMutexGuiLeave();
#else
      theFrame->statusString = wxString(msg);
#endif

   }

   int levels_callback(float input, float output)
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

#ifdef TRY_GUILOCK
      static int lastInputLevel = 0;
      static int lastOutputLevel = 0;

      if (!wxThread::IsMain()) wxMutexGuiEnter();

	if(lastInputLevel != inputLevel) {
	  theFrame->input->SetValue(inputLevel); 
	  lastInputLevel = inputLevel;
	}

	if(lastOutputLevel != outputLevel) {
	  theFrame->output->SetValue(outputLevel); 
	  lastOutputLevel = outputLevel;
	}

      if (!wxThread::IsMain()) wxMutexGuiLeave();
#else
      theFrame->inputLevel = (int)input - (LEVEL_MIN); 
      theFrame->outputLevel = (int)output - (LEVEL_MIN); 
#endif
     return 0;
   }
}
