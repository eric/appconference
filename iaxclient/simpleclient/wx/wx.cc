

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


static int inputLevel = 0;
static int outputLevel = 0;

static char *buttonlabels[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9", "*", "0", "#" };



class IAXTimer : public wxTimer
{
  public:
    void IAXTimer::Notify(); 
};


class DialPad : public wxPanel
{
public:
    DialPad(wxFrame *parent, wxPoint pos, wxSize size);

    void ButtonHandler(wxEvent &evt);

    wxButton *buttons[12];

protected:
    DECLARE_EVENT_TABLE()
};


class IAXFrame : public wxFrame
{
public: 
    IAXFrame(const wxChar *title, int xpos, int ypos, int width, int height);

    ~IAXFrame();

    void IAXFrame::ButtonHandler(wxEvent &evt);

    wxGauge *input; 
    wxGauge *output; 
    DialPad *dialPad;
    IAXTimer *timer;
    wxTextCtrl *iaxDest;
    wxButton *dialButton, *hangButton, *quitButton;

protected:
    DECLARE_EVENT_TABLE()

};

static IAXFrame *theFrame;

void IAXTimer::Notify()
{
    iaxc_process_calls();
}

DialPad::DialPad(wxFrame *parent, wxPoint pos, wxSize size)
  : wxPanel(parent, -1, pos, size, wxTAB_TRAVERSAL, wxString("dial pad"))
{
    wxGridSizer *sizer = new wxGridSizer(3);

    for(int i=0; i<12;i++)
    {
	sizer->Add(
	  new wxButton(this, i, wxString(buttonlabels[i])), 1, wxEXPAND);
    }

    SetSizer(sizer);
    sizer->SetSizeHints(this);
}

void DialPad::ButtonHandler(wxEvent &evt)
{
	int buttonNo = evt.m_id;	
	char *button = buttonlabels[buttonNo];
	//fprintf(stderr, "got Button Event for button %s\n", button);
	iaxc_send_dtmf(*button);
}

BEGIN_EVENT_TABLE(DialPad, wxPanel)
	EVT_BUTTON(0,DialPad::ButtonHandler)
	EVT_BUTTON(1,DialPad::ButtonHandler)
	EVT_BUTTON(2,DialPad::ButtonHandler)
	EVT_BUTTON(3,DialPad::ButtonHandler)
	EVT_BUTTON(4,DialPad::ButtonHandler)
	EVT_BUTTON(5,DialPad::ButtonHandler)
	EVT_BUTTON(6,DialPad::ButtonHandler)
	EVT_BUTTON(7,DialPad::ButtonHandler)
	EVT_BUTTON(8,DialPad::ButtonHandler)
	EVT_BUTTON(9,DialPad::ButtonHandler)
	EVT_BUTTON(10,DialPad::ButtonHandler)
	EVT_BUTTON(11,DialPad::ButtonHandler)
END_EVENT_TABLE()


IAXFrame::IAXFrame(const wxChar *title, int xpos, int ypos, int width, int height)
  : wxFrame((wxFrame *) NULL, -1, title, wxPoint(xpos, ypos), wxSize(width, height))
{
    wxBoxSizer *topsizer = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer *row1sizer = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer *row3sizer = new wxBoxSizer(wxHORIZONTAL);

    CreateStatusBar();
    SetStatusText("Welcome to IAXClient!");
    
    row1sizer->Add(dialPad = new DialPad(this, wxDefaultPosition, wxDefaultSize),1, wxEXPAND);

    row1sizer->Add(input  = new wxGauge(this, -1, LEVEL_MAX-LEVEL_MIN, wxDefaultPosition, wxSize(10,120), 
	  wxGA_VERTICAL,  wxDefaultValidator, wxString("input level")),0,wxEXPAND); 

    row1sizer->Add(output  = new wxGauge(this, -1, LEVEL_MAX-LEVEL_MIN, wxDefaultPosition, wxSize(10,120), 
	  wxGA_VERTICAL,  wxDefaultValidator, wxString("output level")),0, wxEXPAND); 

    topsizer->Add(row1sizer,1,wxEXPAND);

    topsizer->Add(iaxDest = new wxTextCtrl(this, -1, wxString("guest@ast1/8068"), 
	wxDefaultPosition, wxDefaultSize),0,wxEXPAND);
	
    row3sizer->Add(dialButton = new wxButton(this, 101, wxString("Dial"),
	    wxDefaultPosition, wxDefaultSize),1);

    row3sizer->Add(hangButton = new wxButton(this, 102, wxString("Hang Up"),
	    wxDefaultPosition, wxDefaultSize),1);

    row3sizer->Add(quitButton = new wxButton(this, 100, wxString("Quit"),
	    wxDefaultPosition, wxDefaultSize),1);

    topsizer->Add(row3sizer,0,wxEXPAND);

    SetSizer(topsizer);
    topsizer->SetSizeHints(this);

    timer = new IAXTimer();
    timer->Start(10);
    //output = new wxGauge(this, -1, 100); 
}

void IAXFrame::ButtonHandler(wxEvent &evt)
{
	int buttonNo = evt.m_id;	

	//fprintf(stderr, "got Button Event for button %d\n", buttonNo);

	switch(buttonNo) {
		case 101:
			// dial the number
			iaxc_call(stderr,
			   (char *)(theFrame->iaxDest->GetValue().c_str()));
			break;
		case 102:
		        iaxc_dump_call();
			break;
		case 100:
		        iaxc_dump_call();
			iaxc_process_calls();
			for(int i=0;i<10;i++) {
			  iaxc_millisleep(100);
			  iaxc_process_calls();
			}
			exit(0);	
			break;
		default:
			break;
	}
}

BEGIN_EVENT_TABLE(IAXFrame, wxFrame)
	EVT_BUTTON(100,IAXFrame::ButtonHandler)
	EVT_BUTTON(101,IAXFrame::ButtonHandler)
	EVT_BUTTON(102,IAXFrame::ButtonHandler)
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
