//----------------------------------------------------------------------------------------
// Name:        dial.cc
// Purpose:     dial class
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
    #pragma implementation "dial.h"
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

#include "dial.h"  

//----------------------------------------------------------------------------------------
// Remaining headers
// ---------------------------------------------------------------------------------------

#include "app.h"
#include "frame.h"
#include "main.h"

void DialEntry( wxString& EntryName )
{
    wxString Msg;
    wxString FQIN;

    wxConfig *config = new wxConfig("iaxComm");
    wxString  val;
    wxString  KeyPath;


    KeyPath = "/Entries/" + EntryName;
    config->SetPath(KeyPath);

    if(!config->Exists(KeyPath)) {
        EntryName << " unknown";
        wxMessageBox(KeyPath, EntryName);
        return;
    }
    wxString ServerName = config->Read("Server", "");
    wxString Extension  = config->Read("Extension", "s");

    DialDirect(ServerName, Extension);
}

void DialDirect( wxString& ServerName, wxString& Extension )
{
    wxString Msg;
    wxString FQIN;

    wxConfig *config = new wxConfig("iaxComm");
    wxString  val;
    wxString  KeyPath;

    if(Extension.IsEmpty()) {
        Extension = "s";
    }

    KeyPath = "/Servers/" + ServerName;
    config->SetPath(KeyPath);

    if(!config->Exists(KeyPath)) {
        ServerName << " unknown";
        wxMessageBox(KeyPath, ServerName);
        return;
    }

    wxString HostName = config->Read("Host", "");
    wxString UserName = config->Read("Username", "");
    wxString Password = config->Read("Password", "");

    Msg.Printf("User:\t%s\nPassword:\t%s\nHost:\t%s\nExt:\t%s", UserName.c_str(),
                                                                Password.c_str(),
                                                                HostName.c_str(),
                                                                Extension.c_str());

    FQIN.Printf("%s:%s@%s/%s", UserName.c_str(),
                               Password.c_str(),
                               HostName.c_str(),
                               Extension.c_str());

    Msg << "\n\n"<<FQIN;

//    wxMessageBox(Msg, _("Dialing"));
    iaxc_call((char *)FQIN.c_str());
}
