
Tcl interface to the iax2 client lib.
It links statically to the iaxclient library. 

Copyright (c) 2006 Mats Bengtsson
Copyright (c) 2006 Antonio Cano damas

BSD-style license

Debian dependencies:
	tcl8.4-dev
	tcllib
	tclthread

Compile:
	./configure --with-tcl=/usr/lib/tcl8.4
	make
[Note: /usr/lib/tcl8.4 is the directory where is located the tclConfig.sh script]

MacOSX: ProjectBuilder project 
Windows: Dev-C++ (Bloodshed) project

Usage:

    iaxclient::answer
        answer call

    iaxclient::callerid name num
        sets caller id

    iaxclient::changeline line
        changes line

    iaxclient::devices input|output|ring ?-current?
        returns a list {name deviceID} if -current
	else lists all devices as {{name deviceID} ...}

    iaxclient::dial user:pass@server/nnn
        dials client

    iaxclient::formats codec
    
    iaxclient::getport
        returns the listening port number

    iaxclient::hangup
        hang up current call

    iaxclient::hold

    iaxclient::info
        not implemented

    iaxclient::level input|output ?level?
        sets or gets the respective levels with 0 < level < 1

    iaxclient::notify eventType ?tclProc?
        sets or gets a notfier procedure for an event type.
	The valid types and callback forms are:
	<Text>          'procName type callNo message'
	<Levels>        'procName in out'
	<State>         'procName callNo state format remote remote_name local local_context'	               
	<NetStats>      'procName args' where args is a list of '-key value' pairs
	<Url>
	<Video>
	<Registration>  'procName id reply msgcount'
	                reply : ack, rej, timeout

	It returns the present tclProc if any. If you specify an empty
	tclProc the callback will be removed.

    iaxclient::playtone tone

    iaxclient::register username password hostname
        Returns the session id.

    iaxclient::reject
        reject current call

    iaxclient::sendtext text

    iaxclient::sendtone tone

    iaxclient::state

    iaxclient::transfer

    iaxclient::unhold

    iaxclient::unregister sessionID


A tone is any single character from the set 123A456B789C*0#D

A state is a list with any of: free, active, outgoing, ringing, complete, 
selected, busy, or transfer.

A codec is any of G723_1, GSM, ULAW, ALAW, G726, ADPCM, SLINEAR, LPC10,
G729A, SPEEX, or ILBC


