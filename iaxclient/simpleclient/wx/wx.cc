

// For compilers that supports precompilation , includes  wx/wx.h  
#include  "wx/wxprec.h"  

#ifndef WX_PRECOMP 
#include  "wx/wx.h"
#endif 

#include  "wx.h"  

#include "iaxclient.h"

#define LEVEL_MAX -10
#define LEVEL_MIN -50


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

    wxGauge *input; 
    wxGauge *output; 
    IAXTimer *timer;
    wxTextCtrl *iaxDest;

protected:
    DECLARE_EVENT_TABLE()

};

static IAXFrame *theFrame;

void IAXTimer::Notify()
{
    iaxc_process_calls();
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




    /* add status bar first; otherwise, sizer doesn't take it into
     * account */
    CreateStatusBar();
    SetStatusText("Welcome to IAXClient!");


    /* Set up Menus */
    wxMenu *fileMenu = new wxMenu();
#ifndef WXMAC
    fileMenu->Append(ID_QUIT, _T("E&xit\tCtrl-X"));
#else
    fileMenu->Append(ID_QUIT, _T("&Quit\tCtrl-Q"));
#endif

    wxMenu *optionsMenu = new wxMenu();
    optionsMenu->AppendCheckItem(ID_PTT, _T("Enable &Push to Talk"));
    optionsMenu->AppendCheckItem(ID_MUTE, _T("&Mute"));
    optionsMenu->AppendCheckItem(ID_SUPPRESS, _T("&Disable Silence Suppression"));
   

    wxMenuBar *menuBar = new wxMenuBar();
    menuBar->Append(fileMenu, _T("&File"));
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


    aPanel->SetSizer(topsizer);
    topsizer->SetSizeHints(aPanel);

    panelSizer->Add(aPanel,1,wxEXPAND);
    SetSizer(panelSizer);	
    panelSizer->SetSizeHints(this);

    timer = new IAXTimer();
    timer->Start(10);
    //output = new wxGauge(this, -1, 100); 
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

END_EVENT_TABLE()

IAXFrame::~IAXFrame()
{
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
