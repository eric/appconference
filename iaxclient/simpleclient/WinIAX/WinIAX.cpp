// WinIAX.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "winiax.h"
#include "resource.h"
#include "iaxclient.h"
#include "commctrl.h"
//#include "stdarg.h"
//#include "varargs.h"

#define APP_NAME "WinIAX Client"
#define LEVEL_MAX -10
#define LEVEL_MIN -50
#define TIMER_PROCESS_CALLS 1001

int iaxc_callback(iaxc_event e);

int APIENTRY WinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPSTR     lpCmdLine,
                     int       nCmdShow)
{
 	// TODO: Place code here.
	WNDCLASSEX wnd;
	wnd.cbSize=sizeof WNDCLASSEX;
	wnd.lpfnWndProc=(WNDPROC)DialogProc;
//	wnd.hIcon=LoadIcon(NULL,MAKEINTRESOURCE(IDI_ICON1));
//	wnd.hIconSm=LoadIcon(NULL,MAKEINTRESOURCE(IDI_ICON1));
//	wnd.hIconSm=(HICON)LoadImage(hInstance ,MAKEINTRESOURCE(IDI_ICON1),IMAGE_ICON,GetSystemMetrics(SM_CXSMICON),GetSystemMetrics(SM_CYSMICON),LR_DEFAULTCOLOR); 
 
	RegisterClassEx(&wnd);
	INITCOMMONCONTROLSEX cmtcl;
	cmtcl.dwSize=sizeof (INITCOMMONCONTROLSEX);
	cmtcl.dwICC=ICC_WIN95_CLASSES;
	InitCommonControlsEx(&cmtcl); 
	hBr=CreateSolidBrush(RGB(40,100,150));
//	m_hwndMainDlg=CreateDialog(hInstance,MAKEINTRESOURCE(IDD_MAIN_DLG),NULL,(DLGPROC)DialogProc);
	m_hwndMainDlg=CreateDialog(hInstance,MAKEINTRESOURCE(IDD_MAIN_DLG),NULL,(DLGPROC)DialogProc);
	if(!m_hwndMainDlg)
	{	
//		GetLastError();
		TMessageBox("Couldnt create main dialog box\nAborting...");
		return 0;
	}
	ShowWindow(m_hwndMainDlg,SW_SHOW);
	
//	PBM_SETRANGE(0,LEVEL_MAX-LEVEL_MIN);

	
	MSG msg;
	while(GetMessage(&msg,NULL,0,0))
	{
		if(!IsDialogMessage(m_hwndMainDlg,&msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
//	KillTimer(NULL,m_iTimerId);
	iaxc_dump_call();
	iaxc_process_calls();
	for(int i=0;i<10;i++) {
	  iaxc_millisleep(100);
	  iaxc_process_calls();
	}
	iaxc_stop_processing_thread();
	iaxc_shutdown();
	return 0;
}


int TMessageBox(char *szMsg,int btn)
{
	return MessageBox(m_hwndMainDlg,szMsg,APP_NAME,btn);
}


INT_PTR DialogProc( HWND hwndDlg,  // handle to dialog box
  UINT uMsg,     // message
  WPARAM wParam, // first message parameter
  LPARAM lParam  // second message parameter
)
{
	BOOL bRet=FALSE;
	switch (uMsg)
	{
	case WM_INITDIALOG:
		bRet=OnInitDialog();
		break;
	case WM_SYSCOMMAND:
		bRet=OnSysCommand(wParam);
		break;
	case WM_DESTROY:
		EndDialog(m_hwndMainDlg,0);
		PostQuitMessage(0);
		bRet=FALSE;
		break;
	case WM_TIMER:
		bRet=OnTimer(wParam);
		break;
	case WM_CTLCOLORDLG:
		return (INT_PTR)hBr;
		break;
	case WM_CTLCOLORSTATIC:
		SetBkColor((HDC)wParam,RGB(40,100,150));
		SetTextColor((HDC)wParam,RGB(255,255,255));
		return (INT_PTR)hBr;
		break;
	case WM_COMMAND:
		{
			switch (LOWORD(wParam))
			{
			case IDC_BN_DIAL:
				OnBnDial();
//				TMessageBox("Dial Clicked");
				break;
			case IDC_BN_HANGUP:
				OnBnHangup();
				TMessageBox("Hangup Clicked");
				break;
			case IDC_BN_1:
				SendDTMF('1');
				break;
			case IDC_BN_2:
				SendDTMF('2');
				break;
			case IDC_BN_3:
				SendDTMF('3');
				break;
			case IDC_BN_4:
				SendDTMF('4');
				break;
			case IDC_BN_5:
				SendDTMF('5');
				break;
			case IDC_BN_6:
				SendDTMF('6');
				break;
			case IDC_BN_7:
				SendDTMF('7');
				break;
			case IDC_BN_8:
				SendDTMF('8');
				break;
			case IDC_BN_9:
				SendDTMF('9');
				break;
			case IDC_BN_ASTERISK:
				SendDTMF('*');
				break;
			case IDC_BN_0:
				SendDTMF('0');
				break;
			case IDC_BN_HASH:
				SendDTMF('#');
				break;
				
//			case IDC_PROG_INPUT:
//				break;
//			case IDC_PROG_OUTPUT:
//				bRet=TRUE;
//				break;
//			case IDC_E_IAX_URI:
//				break;
				
			}
		}
//		bRet=FALSE;
		break;
	}
	return bRet;
}

BOOL OnInitDialog()
{
//	TMessageBox("OnInitDialog");
	double silence_threshold = -99;

	iaxc_initialize(AUDIO_INTERNAL_PA,1);
	iaxc_set_encode_format(IAXC_FORMAT_GSM);
	iaxc_set_silence_threshold(silence_threshold);

	iaxc_set_event_callback(iaxc_callback);

	iaxc_start_processing_thread();
	PostMessage(GetDlgItem(m_hwndMainDlg,IDC_PROG_OUTPUT),PBM_SETRANGE,0,LEVEL_MIN-LEVEL_MAX);
	PostMessage(GetDlgItem(m_hwndMainDlg,IDC_PROG_INPUT),PBM_SETRANGE,0,LEVEL_MIN-LEVEL_MAX);

//	m_iTimerId=SetTimer(NULL,0,500,(TIMERPROC)TimerProc);
	return TRUE;
}

BOOL OnSysCommand(UINT uCmd)
{
	BOOL bRet=FALSE;
	switch (uCmd)
	{
	case SC_CLOSE:
		DestroyWindow(m_hwndMainDlg);
		bRet=TRUE;
		break;
	case SC_MINIMIZE:
		ShowWindow(m_hwndMainDlg,SW_MINIMIZE);
		bRet=TRUE;
		break;
	}
	return bRet;
}

void OnBnDial()
{
	char szString[MAX_PATH];
	GetDlgItemText(m_hwndMainDlg,IDC_E_IAX_URI,szString,MAX_PATH);
	iaxc_call(szString);
}

void OnBnHangup()
{
	iaxc_dump_call();
}
int status_callback(char *msg)
{
	SetDlgItemText(m_hwndMainDlg,IDC_ST_STATUS,msg);
	return 1
}

int levels_callback(float input, float output)
{
	int inputLevel,outputLevel;
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

	PostMessage(GetDlgItem(m_hwndMainDlg,IDC_PROG_OUTPUT),PBM_SETPOS,outputLevel,0);
	PostMessage(GetDlgItem(m_hwndMainDlg,IDC_PROG_INPUT),PBM_SETPOS,inputLevel,0);


	//char szStr[30];
	//sprintf(szStr,"Output %d,input %d",output,input);
	//SetDlgItemText(m_hwndMainDlg,IDC_ST_STATUS,szStr);
//	Set
//	theFrame->input->SetValue(inputLevel); 
//	theFrame->output->SetValue(outputLevel);
	return 1;
}

int iaxc_callback(iaxc_event e)
{
    switch(e.type) {
        case IAXC_EVENT_LEVELS:
            return levels_callback(e.ev.levels.input, e.ev.levels.output);
        case IAXC_EVENT_TEXT:
            return status_callback(e.ev.text.message);
//        case IAXC_EVENT_STATE:
//            return state_callback(e.ev.call);
        default:
            return 0;  // not handled
    }
}


void SendDTMF(char num)
{
	iaxc_send_dtmf(num);
}

BOOL OnTimer(UINT nIDEvent)
{
	BOOL bRet=TRUE;
	if(nIDEvent==TIMER_PROCESS_CALLS)
	{
		iaxc_process_calls();
		bRet=FALSE;
	}
	return bRet;
}

VOID CALLBACK TimerProc(
  HWND hwnd,         // handle to window
  UINT uMsg,         // WM_TIMER message
  UINT_PTR idEvent,  // timer identifier
  DWORD dwTime       // current system time
)
{
	// Using thread instead of timer from now on.
	if(idEvent==m_iTimerId)
	{
		iaxc_process_calls();		
	}	
}
