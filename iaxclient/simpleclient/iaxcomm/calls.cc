//----------------------------------------------------------------------------------------
// Name:        calls.cpp
// Purpose:     Call appearances listctrl
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
    #pragma implementation "calls.h"
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

#include "calls.h"  

//----------------------------------------------------------------------------------------
// Remaining headers
// ---------------------------------------------------------------------------------------

#include "app.h"
#include "main.h"
#include "prefs.h"
#include "frame.h"
#include <wx/ffile.h>

#include <math.h>
#ifndef M_PI
  #define M_PI 3.14159265358979323846
#endif
 
//----------------------------------------------------------------------------------------
// Event table: connect the events to the handler functions to process them
//----------------------------------------------------------------------------------------

BEGIN_EVENT_TABLE(CallList, wxListCtrl)
    EVT_SIZE               (                CallList::OnSize)  
    EVT_LIST_ITEM_SELECTED (XRCID("Calls"), CallList::OnSelect)
    EVT_LIST_ITEM_ACTIVATED(XRCID("Calls"), CallList::OnDClick)
END_EVENT_TABLE()

//----------------------------------------------------------------------------------------
// Public methods
//----------------------------------------------------------------------------------------

CallList::CallList(wxWindow *parent, wxWindowID id, const wxPoint& pos, 
                    const wxSize& size, long style)
                  : wxListCtrl( parent, id, pos, size, style)
{
    wxConfig   *config = new wxConfig("iaxComm");
    long        i;
    int         nCalls;
    wxListItem  item;
    wxString    Filename;
    wxFFile     fRingTone;
    wxFFile     fRingBack;

    m_parent = parent;

    // Column Headings
    InsertColumn( 0, _(""),       wxLIST_FORMAT_CENTER, (20));
    InsertColumn( 1, _("State"),  wxLIST_FORMAT_CENTER, (60));
    InsertColumn( 2, _("Remote"), wxLIST_FORMAT_LEFT,  (200));

    Hide();
    nCalls = config->Read("/nCalls", 2);
    for(i=0;i<nCalls;i++) {
        InsertItem(i,wxString::Format("%ld", i+1), 0);
        SetItem(i, 2, _T(""));
        item.m_itemId=i;
        item.m_mask = 0;
        SetItem(item);
    }

    Refresh();
    Show();
    AutoSize();

    icomtone.len = 6000;
    icomtone.data = (short *)calloc(icomtone.len , sizeof(short));
    for( int i=0;i < 3000; i++ )
    {
      if(i<3000) {
          icomtone.data[i] =  (short)(0x7fff*0.4*sin((double)i*440*M_PI/8000));
          if((i>1000) && (i<2000))
              icomtone.data[i] =  (short)(0x7fff*0.4*sin((double)i*880*M_PI/8000));
      }
    }
    icomtone.repeat = 1;

    Filename = config->Read("/RingTone", "");
    if(!Filename.IsEmpty())
        fRingTone.Open(Filename, "r");

    if(fRingTone.IsOpened()) {
        ringtone.len  = fRingTone.Length();
        ringtone.data = (short *)calloc(ringtone.len , sizeof(short));

        fRingTone.Read(&ringtone.data[0], ringtone.len);

    } else {
        ringtone.len  = 6*8000;
        ringtone.data = (short *)calloc(ringtone.len , sizeof(short));

        for( int i=0;i < 2*8000; i++ )
        {
            ringtone.data[i] =  (short)(0x7fff*0.4*sin((double)i*880*M_PI/8000));
            ringtone.data[i] += (short)(0x7fff*0.4*sin((double)i*960*M_PI/8000));
        }
    }
    ringtone.repeat = 10;

    Filename = config->Read("/RingBack", "");
    if(!Filename.IsEmpty())
        fRingBack.Open(Filename, "r");

    if(fRingBack.IsOpened()) {
        ringback.len  = fRingBack.Length();
        ringback.data = (short *)calloc(ringback.len , sizeof(short));
        fRingBack.Read(&ringback.data[0], ringback.len);

    } else {
        ringback.len  = 6*8000;
        ringback.data = (short *)calloc(ringback.len , sizeof(short));

        for( int i=0;i < 2*8000; i++ )
        {
            ringback.data[i] =  (short)(0x7fff*0.4*sin((double)i*440*M_PI/8000));
            ringback.data[i] += (short)(0x7fff*0.4*sin((double)i*480*M_PI/8000));
        }
    }
    ringback.repeat = 10;
}

void CallList::AutoSize()
{
    SetColumnWidth(2, GetClientSize().x - 85);
}

void CallList::OnSize(wxSizeEvent &event)
{
    event.Skip();
#ifndef __WXGTK__
    // XXX FIXME: for some reason not yet investigated, this crashes Linux-GTK (for SK, at least).
    AutoSize();
#endif
}

void CallList::OnSelect(wxListEvent &event)
{
    int selected = event.m_itemIndex;
    iaxc_select_call(selected);
}

void CallList::OnDClick(wxListEvent &event)
{
    //Don't need to select, because single click should have done it
    iaxc_dump_call();
}

int CallList::HandleStateEvent(struct iaxc_ev_call_state c)
{
    wxConfig  *config = new wxConfig("iaxComm");
    wxString   str;
    long       dummy;
    bool       bCont;
    static int wow = 0;

    if(c.state & IAXC_CALL_STATE_RINGING) {
      wxGetApp().theFrame->Show();
      wxGetApp().theFrame->Raise();
    }

    // first, handle inactive calls
    if(!(c.state & IAXC_CALL_STATE_ACTIVE)) {
        SetItem(c.callNo, 2, _T("") );
        SetItem(c.callNo, 1, _T("") );
        iaxc_stop_sound(ringback.id);
        iaxc_stop_sound(ringtone.id);
    } else {
        bool     outgoing = c.state & IAXC_CALL_STATE_OUTGOING;
        bool     ringing  = c.state & IAXC_CALL_STATE_RINGING;
        bool     complete = c.state & IAXC_CALL_STATE_COMPLETE;
        wxString info;
        wxString fullname;

        fullname.Printf("%s", c.remote_name);
        info = fullname.AfterLast('@');  // Hide username:password

        SetItem(c.callNo, 2, info );

        if(outgoing) {
            if(ringing) {
                SetItem(c.callNo, 1, _T("ring out") );
                iaxc_play_sound(&ringback, 0);
            } else {
                if(complete) {
                    SetItem(c.callNo, 1, _T("ACTIVE") );
                    iaxc_stop_sound(ringback.id);
                } else {
                    // not accepted yet..
                    SetItem(c.callNo, 1, _T("---") );
                }
            }
        } else {
            if(ringing) {
                SetItem(c.callNo, 1, _T("ring in") );

                // Look for the caller in our phonebook
                config->SetPath("/PhoneBook");
                bCont = config->GetFirstGroup(str, dummy);
                while ( bCont ) {
                    if(str.IsSameAs(c.remote_name))
                        break;
                    bCont = config->GetNextGroup(str, dummy);
                }
                // Add to phone book if not there already
                if(!str.IsSameAs(c.remote_name)) {
                    str.Printf("%s/Extension", c.remote_name);
                    config->Write(str, c.remote);
                }

                if(strcmp(c.local_context, "intercom") == 0) {
                    if(config->Read("/Intercom","s").IsSameAs(c.local)) {
                        iaxc_play_sound(&icomtone, 1);
                        iaxc_millisleep(1000);
                        iaxc_select_call(c.callNo);
                    }
                } else {



    SetAudioDevices(config->Read("/Input Device", ""),
                    config->Read("/Output Device", ""),
                    config->Read("/Output Device", ""));

                    iaxc_play_sound(&ringtone, 1);
                }
            } else {



    SetAudioDevices(config->Read("/Input Device", ""),
                    config->Read("/Output Device", ""),
                    config->Read("/Ring Device", ""));





                iaxc_stop_sound(ringtone.id);
                if(complete) {
                    SetItem(c.callNo, 1, _T("ACTIVE") );
                } else { 
                    // not accepted yet..  shouldn't happen!
                    SetItem(c.callNo, 1, _T("???") );
                }
            }
        } 
    }
    
    // select if necessary
    if((c.state & IAXC_CALL_STATE_SELECTED) &&
      !(GetItemState(c.callNo,wxLIST_STATE_SELECTED|wxLIST_STATE_SELECTED))) 
    {
        //fprintf(stderr, "setting call %d to selected\n", c.callNo);
        SetItemState(c.callNo,wxLIST_STATE_SELECTED,wxLIST_STATE_SELECTED);
    }
    AutoSize();
    Refresh();

    return 0;
}

