//----------------------------------------------------------------------------------------
// Name:        main.cc
// Purpose:     Core application
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
    #pragma implementation "main.h"
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

#include "main.h"
#include "devices.h"
#include "prefs.h"
#include "wx/tokenzr.h"

//----------------------------------------------------------------------------------------
// Remaining headers
//----------------------------------------------------------------------------------------

#include "app.h"
#include "frame.h"
#include "wx/dirdlg.h"

//----------------------------------------------------------------------------------------
// forward decls for callbacks
//----------------------------------------------------------------------------------------
extern "C" {
     static int iaxc_callback(iaxc_event e);
     int        doTestCall(int ac, char **av);
}

//----------------------------------------------------------------------------------------
// Static variables
//----------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------
// wxWindows macro: Declare the application.
//----------------------------------------------------------------------------------------

// Create a new application object: this macro will allow wxWindows to create
// the application object during program execution (it's better than using a
// static object for many reasons) and also declares the accessor function
// wxGetApp() which will return the reference of the right type (i.e. theApp and
// not wxApp).

IMPLEMENT_APP( theApp )

//----------------------------------------------------------------------------------------
// Public methods
//----------------------------------------------------------------------------------------

bool theApp::OnInit()
{
    wxConfig *config = new wxConfig("iaxComm");

    wxString str;
    wxString reginfo;
    long     dummy;
    bool     bCont;

#ifndef __WXMAC__
// XXX this seems broken on mac with wx-2.4.2
    m_single_instance_checker = new wxSingleInstanceChecker( GetAppName() );


    // And if a copy is alreay running, then abort...
    if ( m_single_instance_checker->IsAnotherRunning() ) {
        wxMessageDialog second_instance_messagedialog( (wxWindow*)NULL,
                  _( "Another instance is already running." ),
                  _( "Startup Error" ),
                  wxOK | wxICON_INFORMATION );
        second_instance_messagedialog.ShowModal();
        // Returning FALSE from within wxApp::OnInit() will terminate the application.
        return FALSE;
    }
#endif

    // Load up the XML Resource handler, to be able to load XML resources..

    wxXmlResource::Get()->InitAllHandlers();

    // Load up enough XML resources to construct a main frame

    load_xrc_resource( "frame.xrc" );
    load_xrc_resource( "menubar.xrc" );
    load_xrc_resource( "panel.xrc" );
    load_xrc_resource( "prefs.xrc" );
    load_xrc_resource( "directory.xrc" );
    load_xrc_resource( "devices.xrc" );

    extern void InitXmlResource();
    InitXmlResource();

    // Create an instance of the main frame.
    // Using a pointer since behaviour will be when close the frame, it will
    // Destroy() itself, which will safely call "delete" when the time is right.
    // [The constructor is empty, since using the default value for parent, which is
    // NULL].

    theFrame = new MyFrame();

    // This is of dubious usefulness unless an app is a dialog-only app. This
    // is the first created frame, so it is the top window already.

    SetTopWindow( theFrame );

    // Show the main frame. Unlike most controls, frames don't show immediately upon
    // construction, but instead wait for a specific Show() function.

    theFrame->Show( TRUE );

  #ifdef __WXMSW__
    theTaskBarIcon.SetIcon(wxICON(application));
  #endif

    if(iaxc_initialize(AUDIO_INTERNAL_PA, theFrame->nCalls)) {
        wxFatalError(_("Couldn't Initialize IAX Client "));
    }

    SetAudioDevices(config->Read("/Input Device", ""),
                    config->Read("/Output Device", ""),
                    config->Read("/Ring Device", ""));

    iaxc_set_encode_format(IAXC_FORMAT_GSM);
    iaxc_set_silence_threshold(-99);
    iaxc_set_event_callback(iaxc_callback);

    iaxc_start_processing_thread();

    // Callerid from wxConfig
    theFrame->Name   = config->Read("Name", "IaxComm User");
    theFrame->Number = config->Read("Number",  "700000000");
    SetCallerID(theFrame->Name, theFrame->Number);

    // Register from wxConfig
    config->SetPath("/Accounts");
    bCont = config->GetFirstGroup(str, dummy);
    while ( bCont ) {
        RegisterByName(str);
        bCont = config->GetNextGroup(str, dummy);
    }

    return TRUE;
}

void theApp::RegisterByName(wxString RegName)
{
    wxConfig    *config = new wxConfig("iaxComm");
    wxChar      KeyPath[256];
    wxListItem  item;

    wxStringTokenizer tok(RegName, _T(":@"));
    char user[256], pass[256], host[256];

    if(strlen(RegName) == 0)
        return;

    if(tok.CountTokens() == 3) {

        strncpy( user , tok.GetNextToken().c_str(), 256);
        strncpy( pass , tok.GetNextToken().c_str(), 256);
        strncpy( host , tok.GetNextToken().c_str(), 256);
    } else {
        // Check if it's a Speed Dial
        wxStrcpy(KeyPath,     "/Accounts/");
        wxStrcat(KeyPath,     RegName);
        config->SetPath(KeyPath);
        if(!config->Exists(KeyPath)) {
            theFrame->SetStatusText("Register format error");
            return;
        }
        wxStrcpy(user, config->Read("Username", ""));
        wxStrcpy(pass, config->Read("Password", ""));
        wxStrcpy(host, config->Read("Host", ""));
    }
    iaxc_register(user, pass, host);
}

int theApp::OnExit(void)
{
    // Delete the single instance checker
    wxLogDebug( "Deleting the single instance checker." );
#ifndef __WXMAC__
    delete m_single_instance_checker;
#endif
    return 0;
}

//----------------------------------------------------------------------------------------
// Private methods
//----------------------------------------------------------------------------------------

void theApp::load_xrc_resource( const wxString& xrc_filename )
{
    wxConfig        *config = new wxConfig("iaxComm");
    wxString         xrc_fullname;
    static wxString  xrc_subdirectory = "";

#ifdef __WXMAC__
    if(xrc_subdirectory.IsEmpty()) {
        CFBundleRef mainBundle = CFBundleGetMainBundle ();
        CFURLRef resDir = CFBundleCopyResourcesDirectoryURL (mainBundle);
        CFURLRef resDirAbs = CFURLCopyAbsoluteURL (resDir);
        CFStringRef resPath = CFURLCopyFileSystemPath(resDirAbs, kCFURLPOSIXPathStyle);
        char path[1024];
        CFStringGetCString(resPath, path, 1024, kCFStringEncodingASCII);
        xrc_subdirectory = wxString(path) + wxFILE_SEP_PATH + _T("rc");
    }
#endif

    // First, look where we got the last xrc
    if(!xrc_subdirectory.IsEmpty()) {
        xrc_fullname = xrc_subdirectory + wxFILE_SEP_PATH + xrc_filename;
        if ( ::wxFileExists( xrc_fullname ) ) {
            wxXmlResource::Get()->Load( xrc_fullname );
            return;
        }
    }

    // Next, check where config points
    if(config->Exists("/XRCDirectory")) {
        xrc_fullname = config->Read("/XRCDirectory", "") + wxFILE_SEP_PATH + xrc_filename;
        if ( ::wxFileExists( xrc_fullname ) ) {
            wxXmlResource::Get()->Load( xrc_fullname );
            return;
        }
    }

    // Third, check in cwd
    xrc_fullname = wxGetCwd() + wxFILE_SEP_PATH + "rc" + wxFILE_SEP_PATH + xrc_filename;
    if ( ::wxFileExists( xrc_fullname ) ) {
        wxXmlResource::Get()->Load( xrc_fullname );
        return;
    }

    wxString dirHome;
    wxGetHomeDir(&dirHome);

    // Last, look in ~/rc
    xrc_fullname = dirHome + wxFILE_SEP_PATH + xrc_filename;
    if ( ::wxFileExists( xrc_fullname ) ) {
        wxXmlResource::Get()->Load( xrc_fullname );
        return;
    }
}

#ifdef __WXMSW__
BEGIN_EVENT_TABLE(MyTaskBarIcon, wxTaskBarIcon)
    EVT_MENU(XRCID("TaskBarRestore"), MyTaskBarIcon::OnRestore)
    EVT_MENU(XRCID("TaskBarHide"),    MyTaskBarIcon::OnHide)
    EVT_MENU(XRCID("TaskBarExit"),    MyTaskBarIcon::OnExit)
END_EVENT_TABLE()

void MyTaskBarIcon::OnRestore(wxCommandEvent&)
{
    wxGetApp().theFrame->Show(TRUE);
}

void MyTaskBarIcon::OnHide(wxCommandEvent&)
{
    wxGetApp().theFrame->Show(FALSE);
}

void MyTaskBarIcon::OnExit(wxCommandEvent&)
{
    wxGetApp().theFrame->Close(TRUE);
    wxGetApp().ProcessIdle();
}

void MyTaskBarIcon::OnLButtonDClick(wxEvent&)
{
    wxGetApp().theFrame->Close(TRUE);
    wxGetApp().ProcessIdle();
}

void MyTaskBarIcon::OnRButtonDown(wxEvent&)
{
    PopupMenu(wxXmlResource::Get()->LoadMenu(wxT("TaskBarMenu")));
}

void MyTaskBarIcon::OnLButtonDown(wxEvent&)
{
    wxGetApp().theFrame->Show(TRUE);
}
#endif

//----------------------------------------------------------------------------------------
//
//----------------------------------------------------------------------------------------

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

    // handle events via posting.
  #ifdef USE_GUILOCK
    static int iaxc_callback(iaxc_event e)
    {
        int ret;
        if (!wxThread::IsMain()) wxMutexGuiEnter();
            ret = wxGetApp().theFrame->HandleIAXEvent(&e);
        if (!wxThread::IsMain()) wxMutexGuiLeave();
            return ret;
    }
  #else
    static int iaxc_callback(iaxc_event e)
    {
        iaxc_event *copy;
        wxCommandEvent evt(wxEVT_COMMAND_MENU_SELECTED, IAXCLIENT_EVENT);

        copy = (iaxc_event *)malloc(sizeof(iaxc_event));
        memcpy(copy,&e,sizeof(iaxc_event));

        evt.SetClientData(copy);

        //fprintf(stderr, "Posting Event\n");
        wxPostEvent(wxGetApp().theFrame, evt);

        // pretend we handle all events.
        return 1;
    }
  #endif

}
