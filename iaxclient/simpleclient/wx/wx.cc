
#include "iaxclient.h"

// For compilers that supports precompilation , includes  wx/wx.h  
#include  "wx/wxprec.h"  

#ifndef WX_PRECOMP 
#include  "wx/wx.h"
#endif 

#include  "wx.h"  

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
    wxButton *dialButton, *quitButton;

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
    int x=0 ,y=0;  

    int dX = size.GetWidth()/3;
    int dY = size.GetHeight()/4;


#if 0
    wxGauge *w = new wxGauge(this, -1, 50, wxPoint(0,0), wxSize(10,200), 
	  wxGA_VERTICAL,  wxDefaultValidator, wxString("test level")); 
#endif
    for(int i=0; i<12;)
    {
	fprintf(stderr, "creating button at %d,%d, %d by %d\n", x,y,dX,dY);
	buttons[i] = new wxButton(this, i, wxString(buttonlabels[i]),
	    wxPoint(x,y), wxSize(dX-5,dY-5),
	    0, wxDefaultValidator, wxString(buttonlabels[i]));	      

	i++;
	if(i%3 == 0) {
	    x=0; y+=dY;
	} else {
	    x+=dX;
	}
    }
}

void DialPad::ButtonHandler(wxEvent &evt)
{
	int buttonNo = evt.m_id;	
	char *button = buttonlabels[buttonNo];
	fprintf(stderr, "got Button Event for button %s\n", button);
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
    dialPad = new DialPad(this, wxPoint(0,0), wxSize(100,120));

    input  = new wxGauge(this, -1, LEVEL_MAX-LEVEL_MIN, wxPoint(100,0), wxSize(10,120), 
	  wxGA_VERTICAL,  wxDefaultValidator, wxString("input level")); 
    output  = new wxGauge(this, -1, LEVEL_MAX-LEVEL_MIN, wxPoint(110,0), wxSize(10,120), 
	  wxGA_VERTICAL,  wxDefaultValidator, wxString("output level")); 

    iaxDest = new wxTextCtrl(this, -1, wxString("guest@ast1/8068"), 
	wxPoint(0,120), wxSize(110,20) /*, wxTE_MULTILINE */);
	
    dialButton = new wxButton(this, 100, wxString("Dial"),
	    wxPoint(0,150), wxSize(55,25));

    quitButton = new wxButton(this, 101, wxString("Quit"),
	    wxPoint(60,150), wxSize(55,25));

    timer = new IAXTimer();
    timer->Start(10);
    //output = new wxGauge(this, -1, 100); 
}

void IAXFrame::ButtonHandler(wxEvent &evt)
{
	int buttonNo = evt.m_id;	

	fprintf(stderr, "got Button Event for button %d\n", buttonNo);

	switch(buttonNo) {
		case 100:
			// dial the number
			iaxc_call(stderr,
			   (char *)(theFrame->iaxDest->GetValue().c_str()));
			break;
		case 101:
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
END_EVENT_TABLE()

IAXFrame::~IAXFrame()
{
}

bool IAXClient::OnInit() 
{ 
	theFrame = new IAXFrame("IAXTest", 0,0,130,220);
	theFrame->CreateStatusBar(); 
	theFrame->SetStatusText("Hello World"); 

	theFrame->Show(TRUE); 
	SetTopWindow(theFrame); 
    
	doTestCall(0,NULL);

	return true; 

}

extern "C" {
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
