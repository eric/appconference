//----------------------------------------------------------------------------------------
// Name:        calls,h
// Purpose:     Describes call appearances listctrl
// Author:      Michael Van Donselaar
// Modified by:
// Created:     2003
// Copyright:   (c) Michael Van Donselaar ( michael@vandonselaar.org )
// Licence:     GPL
//----------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------
// Begin single inclusion of this .h file condition
//----------------------------------------------------------------------------------------

#ifndef _CALLS_H_
#define _CALLS_H_

//----------------------------------------------------------------------------------------
// GCC interface
//----------------------------------------------------------------------------------------

#if defined(__GNUG__) && ! defined(__APPLE__)
    #pragma interface "calls.h"
#endif

//----------------------------------------------------------------------------------------
// Headers
//----------------------------------------------------------------------------------------

#include "app.h"

//----------------------------------------------------------------------------------------
// Class definition: MyFrame
//----------------------------------------------------------------------------------------

class CallList : public wxListCtrl
{

public:

    CallList        ( wxWindow *parent,
                      wxWindowID id = -1,
                      const wxPoint &pos = wxDefaultPosition,
                      const wxSize &size = wxDefaultSize,
                      long style = wxLC_REPORT|wxLC_HRULES);
                   
    void              OnSize( wxSizeEvent &event);
    void              AutoSize();
    void              OnSelect(wxListEvent &event);
    void              OnDClick(wxListEvent &event);
    int               HandleStateEvent(struct iaxc_ev_call_state e);

    struct iaxc_sound ringback;
    struct iaxc_sound ringtone;

private:
    wxWindow         *m_parent;

    DECLARE_EVENT_TABLE()
};
    
//----------------------------------------------------------------------------------------
// End single inclusion of this .h file condition
//----------------------------------------------------------------------------------------

#endif  // _CALLS_H_
