#ifndef _WX_H 
#define _WX_H 
/** * The HelloWorldApp class 
 * * This class shows a window containing a statusbar with the text  Hello World  */ 

class IAXClient : public wxApp 
{ 
	public: 
		virtual bool OnInit(); 
}; 

DECLARE_APP(IAXClient) 

extern "C" {
	void status_callback(char *msg);
	int levels_callback(float input, float output);
	int doTestCall(int ac, char **av);
}



#endif /* _WX_H */
