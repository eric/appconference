
#include "iaxclient.h"

// For compilers that supports precompilation , includes  wx/wx.h  
#include  "wx/wxprec.h"  

#ifndef WX_PRECOMP 
#include  "wx/wx.h"
#endif 

#include  "wx.h"  


IMPLEMENT_APP(IAXClient) 


static int inputLevel = 0;
static int outputLevel = 0;

static char *buttonlabels[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9", "*", "0", "#" };



class LevelTimer : public wxTimer
{
  public:
    void LevelTimer::Notify(); 
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

    wxGauge *input; 
    wxGauge *output; 
    DialPad *dialPad;
    LevelTimer *timer;

private:

};

static IAXFrame *theFrame;

void LevelTimer::Notify()
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
	buttons[i] = new wxButton(this, wxID_HIGHEST+i, wxString(buttonlabels[i]),
	    wxPoint(x,y), wxSize(dX-3,dY-3),
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
	int buttonNo = evt.m_id - wxID_HIGHEST;	
	char *button = buttonlabels[buttonNo];
	fprintf(stderr, "got Button Event for button %s\n", button);
	iaxc_send_dtmf(*button);
}

BEGIN_EVENT_TABLE(DialPad, wxPanel)
	EVT_BUTTON(wxID_HIGHEST+0,DialPad::ButtonHandler)
	EVT_BUTTON(wxID_HIGHEST+1,DialPad::ButtonHandler)
	EVT_BUTTON(wxID_HIGHEST+2,DialPad::ButtonHandler)
	EVT_BUTTON(wxID_HIGHEST+3,DialPad::ButtonHandler)
	EVT_BUTTON(wxID_HIGHEST+4,DialPad::ButtonHandler)
	EVT_BUTTON(wxID_HIGHEST+5,DialPad::ButtonHandler)
	EVT_BUTTON(wxID_HIGHEST+6,DialPad::ButtonHandler)
	EVT_BUTTON(wxID_HIGHEST+7,DialPad::ButtonHandler)
	EVT_BUTTON(wxID_HIGHEST+8,DialPad::ButtonHandler)
	EVT_BUTTON(wxID_HIGHEST+9,DialPad::ButtonHandler)
	EVT_BUTTON(wxID_HIGHEST+10,DialPad::ButtonHandler)
	EVT_BUTTON(wxID_HIGHEST+11,DialPad::ButtonHandler)
END_EVENT_TABLE()


IAXFrame::IAXFrame(const wxChar *title, int xpos, int ypos, int width, int height)
  : wxFrame((wxFrame *) NULL, -1, title, wxPoint(xpos, ypos), wxSize(width, height))
{
    dialPad = new DialPad(this, wxPoint(0,0), wxSize(100,120));

    input  = new wxGauge(this, -1, 50, wxPoint(100,0), wxSize(10,200), 
	  wxGA_VERTICAL,  wxDefaultValidator, wxString("input level")); 
    output  = new wxGauge(this, -1, 50, wxPoint(110,0), wxSize(10,200), 
	  wxGA_VERTICAL,  wxDefaultValidator, wxString("output level")); 

    timer = new LevelTimer();
    timer->Start(10);
    //output = new wxGauge(this, -1, 100); 
    //text = new wxTextCtrl(this, -1, wxString("Type some text..."), wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE);
}

IAXFrame::~IAXFrame()
{
}

bool IAXClient::OnInit() 
{ 
	theFrame = new IAXFrame("IAXTest", 0,0,300,400);
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
      if (input < -50)
	inputLevel=0; 
      else
	inputLevel=(int)input+50; 

      if (output < -50)
	outputLevel=0; 
      else
	outputLevel=(int)output+50; 

      theFrame->input->SetValue(inputLevel); 
      theFrame->output->SetValue(outputLevel); 
   }
}
