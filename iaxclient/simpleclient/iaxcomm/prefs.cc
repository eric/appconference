//----------------------------------------------------------------------------------------
// Name:        prefs.cc
// Purpose:     preferences dialog
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
    #pragma implementation "prefs.h"
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
#include "prefs.h"

//----------------------------------------------------------------------------------------
// Remaining headers
// ---------------------------------------------------------------------------------------

#include "app.h"
#include "frame.h"
#include "main.h"
#include "calls.h"

//----------------------------------------------------------------------------------------
// Event table: connect the events to the handler functions to process them
//----------------------------------------------------------------------------------------
BEGIN_EVENT_TABLE(PrefsDialog, wxDialog)
    EVT_BUTTON  ( XRCID("BrowseRingTone"),   PrefsDialog::OnBrowse)
    EVT_BUTTON  ( XRCID("BrowseIntercom"),   PrefsDialog::OnBrowse)
    EVT_BUTTON  ( XRCID("BrowseRingBack"),   PrefsDialog::OnBrowse)
    EVT_BUTTON  ( XRCID("PreviewRingTone"),  PrefsDialog::OnPreviewRingTone)
    EVT_BUTTON  ( XRCID("PreviewIntercom"),  PrefsDialog::OnPreviewIntercom)
    EVT_BUTTON  ( XRCID("PreviewRingBack"),  PrefsDialog::OnPreviewRingBack)
    EVT_TEXT    ( XRCID("RingBack"),         PrefsDialog::OnAudioDirty)
    EVT_TEXT    ( XRCID("RingTone"),         PrefsDialog::OnAudioDirty)
    EVT_TEXT    ( XRCID("Intercom"),         PrefsDialog::OnAudioDirty)
    EVT_BUTTON  ( XRCID("SaveAudio"),        PrefsDialog::OnSaveAudio)
    EVT_BUTTON  ( XRCID("ApplyAudio"),       PrefsDialog::OnApplyAudio)

    EVT_BUTTON  ( XRCID("SaveCallerID"),     PrefsDialog::OnSaveCallerID)
    EVT_BUTTON  ( XRCID("ApplyCallerID"),    PrefsDialog::OnApplyCallerID)
    EVT_TEXT    ( XRCID("Name"),             PrefsDialog::OnCallerIDDirty)
    EVT_TEXT    ( XRCID("Number"),           PrefsDialog::OnCallerIDDirty)

    EVT_BUTTON  ( XRCID("SaveMisc"),         PrefsDialog::OnSaveMisc)
    EVT_BUTTON  ( XRCID("ApplyMisc"),        PrefsDialog::OnApplyMisc)
    EVT_TEXT    ( XRCID("UseSkin"),          PrefsDialog::OnMiscDirty)
    EVT_CHOICE  ( XRCID("DefaultAccount"),   PrefsDialog::OnMiscDirty)
    EVT_TEXT    ( XRCID("IntercomPass"),     PrefsDialog::OnMiscDirty)
    EVT_SPINCTRL( XRCID("nCalls"),           PrefsDialog::OnMiscDirty)

    EVT_BUTTON  ( XRCID("SaveFilters"),      PrefsDialog::OnSaveFilters)
    EVT_BUTTON  ( XRCID("ApplyFilters"),     PrefsDialog::OnApplyFilters)
    EVT_CHECKBOX( XRCID("AGC"),              PrefsDialog::OnFiltersDirty)
    EVT_CHECKBOX( XRCID("AAGC"),             PrefsDialog::OnFiltersDirty)
    EVT_CHECKBOX( XRCID("CN"),               PrefsDialog::OnFiltersDirty)
    EVT_CHECKBOX( XRCID("NoiseReduce"),      PrefsDialog::OnFiltersDirty)
    EVT_CHECKBOX( XRCID("EchoCancel"),       PrefsDialog::OnFiltersDirty)

    EVT_BUTTON  ( XRCID("SaveCodecs"),       PrefsDialog::OnSaveCodecs)
    EVT_BUTTON  ( XRCID("ApplyCodecs"),      PrefsDialog::OnApplyCodecs)

    EVT_CHECKBOX( XRCID("AllowuLaw"),        PrefsDialog::OnCodecAllow)
    EVT_CHECKBOX( XRCID("AllowaLaw"),        PrefsDialog::OnCodecAllow)
    EVT_CHECKBOX( XRCID("AllowGSM"),         PrefsDialog::OnCodecAllow)
    EVT_CHECKBOX( XRCID("AllowSpeex"),       PrefsDialog::OnCodecAllow)
    EVT_CHECKBOX( XRCID("AllowiLBC"),        PrefsDialog::OnCodecAllow)

    EVT_RADIOBUTTON(XRCID("PreferuLaw"),     PrefsDialog::OnCodecPrefer)
    EVT_RADIOBUTTON(XRCID("PreferaLaw"),     PrefsDialog::OnCodecPrefer)
    EVT_RADIOBUTTON(XRCID("PreferGSM"),      PrefsDialog::OnCodecPrefer)
    EVT_RADIOBUTTON(XRCID("PreferSpeex"),    PrefsDialog::OnCodecPrefer)
    EVT_RADIOBUTTON(XRCID("PreferiLBC"),     PrefsDialog::OnCodecPrefer)

    EVT_CHECKBOX( XRCID("SPXEnhance"),       PrefsDialog::OnSpeexTune)
    EVT_SPINCTRL( XRCID("SPXQuality"),       PrefsDialog::OnSpeexTune)
    EVT_SPINCTRL( XRCID("SPXBitrate"),       PrefsDialog::OnSpeexTune)
    EVT_SPINCTRL( XRCID("SPXABR"),           PrefsDialog::OnSpeexTune)
    EVT_CHECKBOX( XRCID("SPXVBR"),           PrefsDialog::OnSpeexTune)
    EVT_SPINCTRL( XRCID("SPXComplexity"),    PrefsDialog::OnSpeexTune)

    EVT_BUTTON  ( XRCID("CancelAudio"),      PrefsDialog::OnCancel)
    EVT_BUTTON  ( XRCID("CancelCallerID"),   PrefsDialog::OnCancel)
    EVT_BUTTON  ( XRCID("CancelMisc"),       PrefsDialog::OnCancel)
    EVT_BUTTON  ( XRCID("CancelFilters"),    PrefsDialog::OnCancel)
    EVT_BUTTON  ( XRCID("CancelCodecs"),     PrefsDialog::OnCancel)

END_EVENT_TABLE()

//----------------------------------------------------------------------------------------
// Public methods
//----------------------------------------------------------------------------------------

PrefsDialog::PrefsDialog(wxWindow* parent)
{    
    wxConfig *config = new wxConfig("iaxComm");
    long      dummy;
    bool      bCont;
    wxString  str;

    wxXmlResource::Get()->LoadDialog(this, parent, wxT("Prefs"));

    // Reach in for our controls

    RingBack       = XRCCTRL(*this, "RingBack",       wxTextCtrl);
    RingTone       = XRCCTRL(*this, "RingTone",       wxTextCtrl);
    Intercom       = XRCCTRL(*this, "Intercom",       wxTextCtrl);
    SaveAudio      = XRCCTRL(*this, "SaveAudio",      wxButton);
    ApplyAudio     = XRCCTRL(*this, "ApplyAudio",     wxButton);
    CancelAudio    = XRCCTRL(*this, "CancelAudio",    wxButton);

    Name           = XRCCTRL(*this, "Name",           wxTextCtrl);
    Number         = XRCCTRL(*this, "Number",         wxTextCtrl);
    SaveCallerID   = XRCCTRL(*this, "SaveCallerID",   wxButton);
    ApplyCallerID  = XRCCTRL(*this, "ApplyCallerID",  wxButton);
    CancelCallerID = XRCCTRL(*this, "CancelCallerID", wxButton);

    UseSkin        = XRCCTRL(*this, "UseSkin",        wxComboBox);
    DefaultAccount = XRCCTRL(*this, "DefaultAccount", wxChoice);
    IntercomPass   = XRCCTRL(*this, "IntercomPass",   wxTextCtrl);
    nCalls         = XRCCTRL(*this, "nCalls",         wxSpinCtrl);
    SaveMisc       = XRCCTRL(*this, "SaveMisc",       wxButton);
    ApplyMisc      = XRCCTRL(*this, "ApplyMisc",      wxButton);
    CancelMisc     = XRCCTRL(*this, "CancelMisc",     wxButton);

    AGC            = XRCCTRL(*this, "AGC",            wxCheckBox);
    AAGC           = XRCCTRL(*this, "AAGC",           wxCheckBox);
    CN             = XRCCTRL(*this, "CN",             wxCheckBox);
    NoiseReduce    = XRCCTRL(*this, "NoiseReduce",    wxCheckBox);
    EchoCancel     = XRCCTRL(*this, "EchoCancel",     wxCheckBox);
    SaveFilters    = XRCCTRL(*this, "SaveFilters",    wxButton);
    ApplyFilters   = XRCCTRL(*this, "ApplyFilters",   wxButton);
    CancelFilters  = XRCCTRL(*this, "CancelFilters",  wxButton);

    AllowuLaw      = XRCCTRL(*this, "AllowuLaw",      wxCheckBox);
    AllowaLaw      = XRCCTRL(*this, "AllowaLaw",      wxCheckBox);
    AllowGSM       = XRCCTRL(*this, "AllowGSM",       wxCheckBox);
    AllowSpeex     = XRCCTRL(*this, "AllowSpeex",     wxCheckBox);
    AllowiLBC      = XRCCTRL(*this, "AllowiLBC",      wxCheckBox);

    PreferuLaw     = XRCCTRL(*this, "PreferuLaw",     wxRadioButton);
    PreferaLaw     = XRCCTRL(*this, "PreferaLaw",     wxRadioButton);
    PreferGSM      = XRCCTRL(*this, "PreferGSM",      wxRadioButton);
    PreferSpeex    = XRCCTRL(*this, "PreferSpeex",    wxRadioButton);
    PreferiLBC     = XRCCTRL(*this, "PreferiLBC",     wxRadioButton);

    SPXEnhance     = XRCCTRL(*this, "SPXEnhance",     wxCheckBox);
    SPXQuality     = XRCCTRL(*this, "SPXQuality",     wxSpinCtrl);
    SPXBitrate     = XRCCTRL(*this, "SPXBitrate",     wxSpinCtrl);
    SPXABR         = XRCCTRL(*this, "SPXABR",         wxSpinCtrl);
    SPXVBR         = XRCCTRL(*this, "SPXVBR",         wxCheckBox);
    SPXComplexity  = XRCCTRL(*this, "SPXComplexity",  wxSpinCtrl);

    SaveCodecs     = XRCCTRL(*this, "SaveCodecs",     wxButton);
    ApplyCodecs    = XRCCTRL(*this, "ApplyCodecs",    wxButton);
    CancelCodecs   = XRCCTRL(*this, "CancelCodecs",   wxButton);

    config->SetPath("/Prefs");

    RingTone->SetValue(wxGetApp().IncomingRingName);
    RingBack->SetValue(wxGetApp().RingBackToneName);
    Intercom->SetValue(wxGetApp().IntercomToneName);

    Name->SetValue(wxGetApp().Name);
    Number->SetValue(wxGetApp().Number);

    UseSkin->Append("default");
    UseSkin->Append("compact");
    UseSkin->Append("logo");
    UseSkin->Append("micro");
    UseSkin->SetValue(config->Read("UseSkin", "default"));

    config->SetPath("/Accounts");
    bCont = config->GetFirstGroup(str, dummy);
    while ( bCont ) {
        DefaultAccount->Append(str);
        bCont = config->GetNextGroup(str, dummy);
    }
    dummy = DefaultAccount->FindString(config->Read("/Prefs/DefaultAccount", ""));
    if(dummy <= 0)
        dummy = 0;
    DefaultAccount->SetSelection(dummy);

    IntercomPass->SetValue(config->Read("/Prefs/IntercomPass", ""));
    nCalls->SetValue(wxGetApp().nCalls);

    AGC->SetValue(wxGetApp().theFrame->AGC);
    AAGC->SetValue(wxGetApp().theFrame->AAGC);
    CN->SetValue(wxGetApp().theFrame->CN);
    NoiseReduce->SetValue(wxGetApp().theFrame->NoiseReduce);
    EchoCancel->SetValue(wxGetApp().theFrame->EchoCancel);

    AllowuLaw->SetValue( wxGetApp().theFrame->AllowuLawVal);
    AllowaLaw->SetValue( wxGetApp().theFrame->AllowaLawVal);
    AllowGSM->SetValue(  wxGetApp().theFrame->AllowGSMVal);
    AllowSpeex->SetValue(wxGetApp().theFrame->AllowSpeexVal);
    AllowiLBC->SetValue( wxGetApp().theFrame->AllowiLBCVal);

    // Our local copy, because we may play with it and not Apply it
    LocalPreferredBitmap = wxGetApp().theFrame->PreferredBitmap;

    if(LocalPreferredBitmap == IAXC_FORMAT_ILBC)
        PreferiLBC->SetValue(TRUE);

    if(LocalPreferredBitmap == IAXC_FORMAT_SPEEX)
        PreferSpeex->SetValue(TRUE);

    if(LocalPreferredBitmap == IAXC_FORMAT_GSM)
        PreferGSM->SetValue(TRUE);

    if(LocalPreferredBitmap == IAXC_FORMAT_ALAW)
        PreferaLaw->SetValue(TRUE);

    if(LocalPreferredBitmap == IAXC_FORMAT_ULAW)
        PreferuLaw->SetValue(TRUE);

    SPXEnhance->SetValue(    wxGetApp().theFrame->SPXEnhanceVal);
    SPXQuality->SetValue(    wxGetApp().theFrame->SPXQualityVal);
    SPXBitrate->SetValue(    wxGetApp().theFrame->SPXBitrateVal);
    SPXABR->SetValue(        wxGetApp().theFrame->SPXABRVal);
    SPXVBR->SetValue(        wxGetApp().theFrame->SPXVBRVal);
    SPXComplexity->SetValue( wxGetApp().theFrame->SPXComplexityVal);

    delete config;

    //Populating wxTextCtrls makes EVT_TEXT, just as if user did it

    SaveAudio->Disable();
    ApplyAudio->Disable();
    CancelAudio->SetLabel("Done");

    SaveCallerID->Disable();
    ApplyCallerID->Disable();
    CancelCallerID->SetLabel("Done");

    SaveMisc->Disable();
    ApplyMisc->Disable();
    CancelMisc->SetLabel("Done");
}

//----------------------------------------------------------------------------------------
// Private methods
//----------------------------------------------------------------------------------------

void PrefsDialog::OnPreviewRingTone(wxCommandEvent &event)
{
    // We only want to preview a single ring
    wxGetApp().IncomingRing.LoadTone(RingTone->GetValue(), 0);
    wxGetApp().IncomingRing.Start(1);
    wxGetApp().IncomingRing.LoadTone(RingTone->GetValue(), 10);
}

void PrefsDialog::OnPreviewRingBack(wxCommandEvent &event)
{
    wxGetApp().RingbackTone.LoadTone(RingBack->GetValue(), 0);
    wxGetApp().RingbackTone.Start(1);
    wxGetApp().RingbackTone.LoadTone(RingBack->GetValue(), 10);
}

void PrefsDialog::OnPreviewIntercom(wxCommandEvent &event)
{
    wxGetApp().IntercomTone.LoadTone(Intercom->GetValue(), 0);
    wxGetApp().IntercomTone.Start(1);
    wxGetApp().IntercomTone.LoadTone(Intercom->GetValue(), 1);
}

void PrefsDialog::OnBrowse(wxCommandEvent &event)
{
    wxString dirHome;
    wxGetHomeDir(&dirHome);

    wxFileDialog where(NULL, _("Raw sound file"), dirHome, "", "*.*", wxOPEN );
    where.ShowModal();

    if(event.GetId() == XRCID("BrowseRingBack"))
        RingBack->SetValue(where.GetPath());

    if(event.GetId() == XRCID("BrowseRingTone"))
        RingTone->SetValue(where.GetPath());

    if(event.GetId() == XRCID("BrowseIntercom"))
        Intercom->SetValue(where.GetPath());

}

void SetCallerID(wxString name, wxString number)
{
    iaxc_set_callerid((char *)name.c_str(),
                      (char *)number.c_str());
}

void PrefsDialog::OnSaveAudio(wxCommandEvent &event)
{
    wxConfig *config = new wxConfig("iaxComm");
    config->SetPath("/Prefs");

    config->Write("RingTone",   RingTone->GetValue());
    config->Write("RingBack",   RingBack->GetValue());
    config->Write("Intercom",   Intercom->GetValue());

    delete config;
    SaveAudio->Disable();
    OnApplyAudio(event);
}

void PrefsDialog::OnApplyAudio(wxCommandEvent &event)
{
    wxGetApp().IncomingRingName = RingTone->GetValue();
    wxGetApp().RingBackToneName = RingBack->GetValue();
    wxGetApp().IntercomToneName = Intercom->GetValue();

    wxGetApp().IncomingRing.LoadTone(RingTone->GetValue(), 10);
    wxGetApp().RingbackTone.LoadTone(RingBack->GetValue(), 10);
    wxGetApp().IntercomTone.LoadTone(Intercom->GetValue(),  1);

    ApplyAudio->Disable();
    CancelAudio->SetLabel("Done");
}

void PrefsDialog::OnSaveCallerID(wxCommandEvent &event)
{
    wxConfig *config = new wxConfig("iaxComm");
    config->SetPath("/Prefs");

    config->Write("Name",           Name->GetValue());
    config->Write("Number",         Number->GetValue());

    delete config;
    SaveCallerID->Disable();
    OnApplyCallerID(event);
}

void PrefsDialog::OnApplyCallerID(wxCommandEvent &event)
{
    wxGetApp().Name   = Name->GetValue();
    wxGetApp().Number = Number->GetValue();
    SetCallerID(wxGetApp().Name, wxGetApp().Number);

    ApplyCallerID->Disable();
    CancelCallerID->SetLabel("Done");
}

void PrefsDialog::OnSaveMisc(wxCommandEvent &event)
{
    wxConfig *config = new wxConfig("iaxComm");
    config->SetPath("/Prefs");

    config->Write("UseSkin",        UseSkin->GetValue());
    config->Write("DefaultAccount", DefaultAccount->GetStringSelection());
    config->Write("IntercomPass",   IntercomPass->GetValue());
    config->Write("nCalls",         nCalls->GetValue());

    delete config;
    SaveMisc->Disable();
    OnApplyMisc(event);
}

void PrefsDialog::OnApplyMisc(wxCommandEvent &event)
{
    // Update the Default Account on the main panel, if there is one
    wxGetApp().DefaultAccount = DefaultAccount->GetStringSelection();
    wxGetApp().theFrame->ShowDirectoryControls();


    wxGetApp().theFrame->IntercomPass = IntercomPass->GetValue();
    wxGetApp().nCalls                 = nCalls->GetValue();

  #ifdef __WXMSW__
    // This segfaults on Linux, need to look into this
    wxGetApp().theFrame->RePanel(UseSkin->GetValue());
  #endif

    ApplyMisc->Disable();
    CancelMisc->SetLabel("Done");
}

void PrefsDialog::OnSaveFilters(wxCommandEvent &event)
{
    wxConfig *config = new wxConfig("iaxComm");
    config->SetPath("/Prefs");

    config->Write("AGC",            AGC->GetValue());
    config->Write("AAGC",           AAGC->GetValue());
    config->Write("CN",             CN->GetValue());
    config->Write("NoiseReduce",    NoiseReduce->GetValue());
    config->Write("EchoCancel",     EchoCancel->GetValue());

    delete config;
    SaveFilters->Disable();
    OnApplyFilters(event);
}

void PrefsDialog::OnApplyFilters(wxCommandEvent &event)
{
    wxGetApp().theFrame->AGC         = AGC->GetValue();
    wxGetApp().theFrame->AAGC        = AAGC->GetValue();
    wxGetApp().theFrame->CN          = CN->GetValue();
    wxGetApp().theFrame->NoiseReduce = NoiseReduce->GetValue();
    wxGetApp().theFrame->EchoCancel  = EchoCancel->GetValue();

    wxGetApp().theFrame->ApplyFilters();

    ApplyFilters->Disable();
    CancelFilters->SetLabel("Done");
}

void PrefsDialog::OnCodecPrefer(wxCommandEvent &event)
{
    if(PreferiLBC->GetValue()) 
        LocalPreferredBitmap = IAXC_FORMAT_ILBC;

    if(PreferGSM->GetValue())
        LocalPreferredBitmap = IAXC_FORMAT_GSM;

    if(PreferSpeex->GetValue())
        LocalPreferredBitmap = IAXC_FORMAT_SPEEX;

    if(PreferuLaw->GetValue())
        LocalPreferredBitmap = IAXC_FORMAT_ULAW;

    if(PreferaLaw->GetValue())
        LocalPreferredBitmap = IAXC_FORMAT_SPEEX;

    OnCodecAllow(event);
}

void PrefsDialog::OnCodecAllow(wxCommandEvent &event)
{
    // If a codec is preferred, then it must be allowed
    if(PreferiLBC->GetValue())
        AllowiLBC->SetValue(TRUE);

    if(PreferGSM->GetValue())
        AllowGSM->SetValue(TRUE);

    if(PreferSpeex->GetValue())
        AllowSpeex->SetValue(TRUE);

    if(PreferuLaw->GetValue())
        AllowuLaw->SetValue(TRUE);

    if(PreferaLaw->GetValue())
        AllowaLaw->SetValue(TRUE);

    ApplyCodecs->Enable();
    SaveCodecs->Enable();
    CancelCodecs->SetLabel("Cancel");
}

void PrefsDialog::OnSpeexTune(wxCommandEvent &event)
{
    // Not sure if there's anything different to do here
    OnCodecAllow(event);
}

void PrefsDialog::OnSaveCodecs(wxCommandEvent &event)
{
    wxConfig *config = new wxConfig("iaxComm");
    config->SetPath("/Codecs");

    config->Write("AllowuLaw",       AllowuLaw->GetValue());
    config->Write("AllowaLaw",       AllowaLaw->GetValue());
    config->Write("AllowGSM",        AllowGSM->GetValue());
    config->Write("AllowSpeex",      AllowSpeex->GetValue());
    config->Write("AllowiLBC",       AllowiLBC->GetValue());
    config->Write("Preferred",       LocalPreferredBitmap);
    
    config->SetPath("/Codecs/SpeexTune");
    
    config->Write("SPXEnhance",      SPXEnhance->GetValue());
    config->Write("SPXQuality",      SPXQuality->GetValue());
    config->Write("SPXBitrate",      SPXBitrate->GetValue());
    config->Write("SPXABR",          SPXABR->GetValue());
    config->Write("SPXVBR",          SPXVBR->GetValue());
    config->Write("SPXComplexity",   SPXComplexity->GetValue());


    delete config;
    SaveCodecs->Disable();
    OnApplyCodecs(event);
}

void PrefsDialog::OnApplyCodecs(wxCommandEvent &event)
{
    wxGetApp().theFrame->AllowuLawVal     = AllowuLaw->GetValue();
    wxGetApp().theFrame->AllowaLawVal     = AllowaLaw->GetValue();
    wxGetApp().theFrame->AllowGSMVal      = AllowGSM->GetValue();
    wxGetApp().theFrame->AllowSpeexVal    = AllowSpeex->GetValue();
    wxGetApp().theFrame->AllowiLBCVal     = AllowiLBC->GetValue();

    wxGetApp().theFrame->PreferredBitmap  = LocalPreferredBitmap;

    wxGetApp().theFrame->SPXEnhanceVal    = SPXEnhance->GetValue();
    wxGetApp().theFrame->SPXQualityVal    = SPXQuality->GetValue();
    wxGetApp().theFrame->SPXBitrateVal    = SPXBitrate->GetValue();
    wxGetApp().theFrame->SPXABRVal        = SPXABR->GetValue();
    wxGetApp().theFrame->SPXVBRVal        = SPXVBR->GetValue();
    wxGetApp().theFrame->SPXComplexityVal = SPXComplexity->GetValue();

    wxGetApp().theFrame->ApplyCodecs();

    ApplyCodecs->Disable();
    CancelCodecs->SetLabel("Done");
}

void PrefsDialog::OnAudioDirty(wxCommandEvent &event)
{
    ApplyAudio->Enable();
    SaveAudio->Enable();
    CancelAudio->SetLabel("Cancel");
}

void PrefsDialog::OnCallerIDDirty(wxCommandEvent &event)
{
    ApplyCallerID->Enable();
    SaveCallerID->Enable();
    CancelCallerID->SetLabel("Cancel");
}

void PrefsDialog::OnMiscDirty(wxCommandEvent &event)
{
    ApplyMisc->Enable();
    SaveMisc->Enable();
    CancelMisc->SetLabel("Cancel");
}

void PrefsDialog::OnFiltersDirty(wxCommandEvent &event)
{
    ApplyFilters->Enable();
    SaveFilters->Enable();
    CancelFilters->SetLabel("Cancel");
}
