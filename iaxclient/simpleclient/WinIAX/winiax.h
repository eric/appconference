#include "gsm\inc\gsm.h"
#include "stdio.h"
#include "process.h"
//#include "sox.h"


HWND m_hwndMainDlg;
UINT_PTR m_iTimerId;
HBRUSH hBr;








int TMessageBox(char *szMsg,int btn=MB_OK);
BOOL OnInitDialog();
BOOL OnSysCommand(UINT uCmd);
INT_PTR DialogProc( HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
void OnBnDial();
void OnBnHangup();
void status_callback(char *msg);
int levels_callback(float input, float output);
void SendDTMF(char num);
BOOL OnTimer(UINT nIDEvent);
VOID CALLBACK TimerProc(HWND hwnd,UINT uMsg,UINT_PTR idEvent,DWORD dwTime);
