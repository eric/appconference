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
    long       i;
    wxListItem item;

    m_parent = parent;

    // Column Headings
    InsertColumn( 0, _(""),       wxLIST_FORMAT_CENTER, (16));
    InsertColumn( 1, _("State"),  wxLIST_FORMAT_CENTER, (48));
    InsertColumn( 2, _("Remote"), wxLIST_FORMAT_LEFT,  (200));

    Hide();
    for(i=0;i<MAX_CALLS;i++) {
        InsertItem(i,wxString::Format("%ld", i+1), 0);
        SetItem(i, 2, _T("No call"));
        item.m_itemId=i;
        item.m_mask = 0;
        item.SetTextColour(*wxLIGHT_GREY);
        item.SetBackgroundColour(*wxWHITE);
        SetItem(item);
    }
    Refresh();
    Show();
    AutoSize();
}

void CallList::AutoSize()
{
    SetColumnWidth(2, GetClientSize().x - 65);
    // Stupid boundary condition.  Avoid unwanted HScroller
    SetColumnWidth(2, GetClientSize().x - 64);
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
//    Don't need to select, because single click should have done it
//    int selected = event.m_itemIndex;
//    iaxc_select_call(selected);

    iaxc_dump_call();
}

int CallList::HandleStateEvent(struct iaxc_ev_call_state c)
{
    wxListItem stateItem; // for all the state color

    stateItem.m_itemId = c.callNo;

    if(c.state & IAXC_CALL_STATE_RINGING) {
      wxGetApp().theFrame->Show();
      wxGetApp().theFrame->Raise();
    }
 
    // first, handle inactive calls
    if(!(c.state & IAXC_CALL_STATE_ACTIVE)) {
        //fprintf(stderr, "state for item %d is free\n", c.callNo);
        SetItem(c.callNo, 2, _T("No call") );
        SetItem(c.callNo, 1, _T("") );
        stateItem.SetTextColour(*wxLIGHT_GREY);
        stateItem.SetBackgroundColour(*wxWHITE);
    } else {
        // set remote 
        SetItem(c.callNo, 2, c.remote );

        bool outgoing = c.state & IAXC_CALL_STATE_OUTGOING;
        bool ringing = c.state & IAXC_CALL_STATE_RINGING;
        bool complete = c.state & IAXC_CALL_STATE_COMPLETE;

        if( ringing && !outgoing ) {
            stateItem.SetTextColour(*wxBLACK);
            stateItem.SetBackgroundColour(*wxRED);
        } else {
            stateItem.SetTextColour(*wxBLUE);
            stateItem.SetBackgroundColour(*wxWHITE);
        }

        if(outgoing) {
            if(ringing) 
               SetItem(c.callNo, 1, _T("<r>") );
           else if(complete)
               SetItem(c.callNo, 1, _T("<->") );
           else // not accepted yet..
               SetItem(c.callNo, 1, _T("< >") );
        } else {
            if(ringing) 
                SetItem(c.callNo, 1, _T(">R<") );
            else if(complete)
                SetItem(c.callNo, 1, _T(">-<") );
            else // not accepted yet..  shouldn't happen!
                SetItem(c.callNo, 1, _T("> <") );
        } 
    // XXX do something more noticable if it's incoming, ringing!
    }
   
    SetItem( stateItem ); 
    
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

//----------------------------------------------------------------------------------------
// Private methods
//----------------------------------------------------------------------------------------

