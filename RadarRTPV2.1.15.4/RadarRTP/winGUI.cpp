// This module creates the graphical user interface under WIndows.
// Portions of this were automatically created by Visual Studio
// The original was in the "main" file.  I moved the GUI routines here from that file
// Written by Frank Robey
// Dec 2013-Dec 2014, Original implementation
// Mar 2018, Moved to separate file
/*
RadarRTP - Radar Real time Program (RTP)

© 2022 Massachusetts Institute of Technology.

Distributed under GNU GPLv2.

This program is free software; you can redistribute it and/or modify it under the terms of the GNU
General Public License as published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even
the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details, supplied with this source as License.txt
*/

#include "stdafx.h"

#ifndef _RTP_Headless
#ifdef _WINGUI

#include "RadarRTP.h"
#include "radarConfig.h"
#include "ImageDisplay.h"
#include "winGUI.h"
#include "commandIF.h"



#define MAX_LOADSTRING 100

// Global Variables:
#ifndef _RTP_Headless
HINSTANCE hInst;                                // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name

#define UD_MAX_POS 30
#define UD_MIN_POS 0
#define UD_SIM_MAX_POS 0
#define UD_SIM_MIN_POS -100
#define RGBOnColor RGB(80, 128, 80)

HWND hWndApp;
HWND hTrack1, hTrack2;
HWND hTBGroup;
HWND hUpDown1, hEditUD1, hStaticUD1, hUpDown2, hEditUD2, hStaticUD2;
HWND hlblTB1, hlblTB2;
HWND hwndConfigBox, hwndControlBox, hwndButtonRecProc, hwndButtonRecRaw, hwndButtonSim;
HWND hwndSimAmp, hwndRecFile, hStaticSim;
HWND hwndConfigStaticTextLeft, hwndConfigStaticTextRight;
HWND hPeakOverlay;
//HWND hwndRecResults, hwndRecProc;

HGDIOBJ hfntRotate;  // I was constantly creating and deleting this so now making static

// State of ownerdrawn buttons. 
//bool btnSimState;
//bool btnRecRawState;
//bool btnRecProcState;

// local copy of radar configuration and state. Set when window is created
RadarConfig dRadarConfig;
RadarState dRadarState;

#ifdef UNICODE
#define tstring wstring
#else
#define tstring string
#endif


//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEXW wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_RADARRTP));
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	// wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
	wcex.hbrBackground = GetSysColorBrush(COLOR_3DFACE);
	wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_RADARRTP);
	wcex.lpszClassName = szWindowClass;
	wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

	return RegisterClassExW(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow, std::string WinTitle)
{
	// Figure out how big window should be.  Could put the size in the ini file... 
	RECT monsize;
	const HWND hDesktop = GetDesktopWindow();	// Get a handle to the current desktop window 
	int WinWidth, WinHeight;

	// Give the window a title
	//int chn = swprintf(szTitle, sizeof(szTitle)/sizeof(szTitle[0]), L"%hs", gRadarConfig.ProcWindowFName.c_str());
	int chn = swprintf(szTitle, sizeof(szTitle) / sizeof(szTitle[0]), L"%hs", WinTitle.c_str() );
	if (chn <= MAX_LOADSTRING) szTitle[chn] = 0x0; // null terminate string
	LoadStringW(hInstance, IDC_RADARRTP, szWindowClass, MAX_LOADSTRING);

	MyRegisterClass(hInstance);
	
	hInst = hInstance; // Store instance handle in our global variable
	GetWindowRect(hDesktop, &monsize);	// Get the size of Desktop window 
	WinWidth = (int)((monsize.right)*0.9);
	WinHeight = (int)((monsize.bottom)*0.95);
	hWndApp = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
		50, (int) (monsize.bottom*0.05), WinWidth, WinHeight, nullptr, nullptr, hInstance, nullptr);

	if (!hWndApp) 	{
		return FALSE;
	}

	init_bitmaps();
	ShowWindow(hWndApp, SW_SHOW); // nCmdShow);
	UpdateWindow(hWndApp);	
	return TRUE;
}

void CreateRotatedFont()
{
	LOGFONT lf;

	GetObject(GetStockObject(DEFAULT_GUI_FONT), sizeof(LOGFONT), &lf);

	lf.lfOrientation = 900;
	lf.lfEscapement = 900;
	hfntRotate = CreateFontIndirect(&lf);
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE:  Processes messages for the main window.
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	LPNMUPDOWN lpnmud;
	UINT code;
	HDC hmemdc;
	
	static HBRUSH hBrush;
	static HPEN   hPen;
	RECT clientrec;
	PAINTSTRUCT ps;
	HDC hdc;
	RECT  rectImage, DispSize;
	static bool firstpaint = true;
	COLORREF  textcol = RGB(0, 0, 0);
	TCHAR text1[] = L"Range-Doppler Image";
	TCHAR text2[] = L"Speed (m/s)";
	TCHAR text3[] = L"Range (m)";
	TCHAR text4[] = L"Time (TBD)";

	LPCWSTR cSimOff = L"Simulation";
	LPCWSTR cSimOn = L"Simulation is On";
	LPCWSTR cRecRawOff = L"Record Raw";
	LPCWSTR cRecRawOn = L"Record Raw On";
	LPCWSTR cRecProcOff = L"Record Results";
	LPCWSTR cRecProcOn = L"Record Res. On";
	int BorderX = 10;
	int BorderY = 10;
	int ImHeight;
	int LabelSize = 50;
	HGDIOBJ hfntPrev;



	cmdbuffer cmdBuff;

	switch (message)
	{
		dRadarConfig = GetRadarConfig();
		dRadarState = GetRadarState();
	case WM_CREATE:
	{
		GetClientRect(hWnd, &clientrec);

		// Create the rotated font
		CreateRotatedFont();

		// Get current configuration and state to setup the initial control positions
		dRadarConfig = GetRadarConfig();
		dRadarState = GetRadarState();

		// Build the basic displays
		CreateConfigControls(hWnd);
		CreateDisplayControls(hWnd);
		CreateRecordControls(hWnd);
		CreateSimControls(hWnd);
		CreateImageWindows(hWnd);
		UpdateConfigText();
		break;
	}
	case WM_COMMAND:
	{
		if (HIWORD(wParam) == BN_CLICKED) {  // All WM_COMMANDS should be BN_CLICKED in this program

			switch (LOWORD(wParam)) {

			case IDC_SIM_ON_BTN: {
				cmdBuff.cmd = SetSimOn;
				dRadarState.SimOn = !dRadarState.SimOn;
				//btnSimState = !btnSimState;
				cmdBuff.onoff = dRadarState.SimOn;
				CommandParse(cmdBuff);
				// std::cout << "Sim Button pressed " << cmdBuff.onoff<<" simstate " << gRadarState.SimOn << std::endl;
				// gRadarState.SimOn = checked;  // Toggle simulation state
				InvalidateRect((HWND)lParam, NULL, TRUE);
				//MessageBeep(MB_OK);
				break;
			}
			case IDC_REC_RAW_BTN: {

				dRadarState.RawRecording = toggle_raw_recording();
				InvalidateRect((HWND)lParam, NULL, TRUE);
				//MessageBeep(MB_OK);
				break;
			}
			case IDC_REC_PROC_BTN: {
				
				dRadarState.DataRecording = toggle_proc_recording();
				InvalidateRect((HWND)lParam, NULL, TRUE);
				break;
			}
			case IDC_PEAK_OVERLAY_BTN: {
				BOOL checked = IsDlgButtonChecked(hWnd, IDC_PEAK_OVERLAY_BTN);
				if (checked==BST_CHECKED) {
					CheckDlgButton(hWnd, IDC_PEAK_OVERLAY_BTN, BST_UNCHECKED);
					cmdBuff.onoff = FALSE;
					log_message("PeakOverlay is now unchecked");
				}
				else {
					CheckDlgButton(hWnd, IDC_PEAK_OVERLAY_BTN, BST_CHECKED);
					cmdBuff.onoff = TRUE;
					log_message("PeakOverlay is now checked");
				}
				cmdBuff.cmd = SetDispMarkPeak;
				dRadarState.PeakOverlay = cmdBuff.onoff;
				CommandParse(cmdBuff);

				break;
			}

			case ID_CM_HOT: {
				SetColorMap(cmaptype::COLORMAP_HOT);
				SetColorBarImage();
				break;
			}
			case ID_CM_JET: {
				SetColorMap(cmaptype::COLORMAP_JET);
				SetColorBarImage();
				break;
			}
			case ID_CM_GRAY: {
				SetColorMap(cmaptype::COLORMAP_GRAY);
				SetColorBarImage();
				break;
			}
			case IDM_ABOUT: {
				DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
				break;
			}
			case IDM_EXIT: {	//stop_radar();

				log_message("Exit called");
				stop_radar();
				StopRadarDisp();
				//DestroyWindow(hWnd);

				PostQuitMessage(0);

				break;
			}
			default: {
				return DefWindowProc(hWnd, message, wParam, lParam);
			}
					 
			}
			break;
		}

	}
	break;
	case WM_PAINT:
	//case WM_SIZE:
	//case WM_MOVE:
		{
		//log_message("Repaint");
		// Update the image display
		UpdateImageWindows(hWnd);

		// Update the remainder of the image window - labels
		hdc = BeginPaint(hWnd, &ps);
		
		hmemdc = CreateCompatibleDC(hdc);


		hPen = CreatePen(PS_SOLID, 2, RGB(128, 0, 255));
		SelectObject(hmemdc, hPen);      // select into the DC 
		SetBkColor(hmemdc, RGB(0, 0, 255));

		GetWindowRect(hWnd, &DispSize);

		ImHeight = (DispSize.bottom - DispSize.top - iOutPanelHeight) / 2 - BorderY;

		// Do labels that are rotated
		hfntPrev = SelectObject(hdc, hfntRotate);

		SetBkMode(hdc, TRANSPARENT);
		SetTextColor(hdc, textcol);
		TextOut(hdc, iControlPanelWidth + BorderX + LabelSize / 2, ImHeight / 2 + BorderY, text4, ARRAYSIZE(text4));	// Time
		TextOut(hdc, iControlPanelWidth + BorderX + LabelSize / 2, ImHeight + ImHeight / 2 + 2 * BorderY, text3, ARRAYSIZE(text3));	// Range
		SelectObject(hdc, hfntPrev);
		
		GetClientRect(hWnd, &rectImage);

		rectImage.left += iControlPanelWidth+BORDER+10;
		UpdateImages(hWnd, hdc, rectImage);
		// Cleanup

		DeleteDC(hmemdc);
		DeleteObject(hPen);

		ReleaseDC(hWnd, hdc);
		EndPaint(hWnd, &ps);
	
	}
	break;
	case WM_TIMER:      /* timeout--update disk space / time running */
		break;

	case WM_NOTIFY:
	{
		code = ((LPNMHDR)lParam)->code;
		switch (code)
		{
		case UDN_DELTAPOS:
		{
			switch (LOWORD(wParam)) {

			case ID_UPDOWN2:
			{
				lpnmud = (NMUPDOWN *)lParam;
				int value = lpnmud->iPos + lpnmud->iDelta;
				if (value < UD_SIM_MIN_POS) {
					 value = UD_SIM_MIN_POS;
				}

				if (value > UD_SIM_MAX_POS) {
					value = UD_SIM_MAX_POS;
				}
				
				cmdBuff.cmd = SetSimAmp;
				cmdBuff.fparams[0] = (float) value;
				CommandParse(cmdBuff);
				dRadarState.SimAmp = value;

				const int asize = 4;
				wchar_t buf[asize];
				size_t cbDest = asize * sizeof(wchar_t);
				StringCbPrintfW(buf, cbDest, L"%d", 10 * value);
				SetWindowTextW(hEditUD2, buf);

			} // UPDOWN2
			break;
			}
		}
		case NM_CUSTOMDRAW:
		{
			//log_message("NM_CUSTOMDRAW");
			if ((((LPNMHDR)lParam)->idFrom == IDC_SIM_ON_BTN) ||
				(((LPNMHDR)lParam)->idFrom == IDC_REC_RAW_BTN) ||
				(((LPNMHDR)lParam)->idFrom == IDC_REC_PROC_BTN))
			{
				LPNMCUSTOMDRAW lpnmCD = (LPNMCUSTOMDRAW)lParam;

				switch (lpnmCD->dwDrawStage)
				{
				case CDDS_PREPAINT:

					SetDCBrushColor(lpnmCD->hdc, RGB(0, 255, 0));
					SetDCPenColor(lpnmCD->hdc, RGB(0, 255, 0));
					SelectObject(lpnmCD->hdc, GetStockObject(DC_BRUSH));
					SelectObject(lpnmCD->hdc, GetStockObject(DC_PEN));

					RoundRect(lpnmCD->hdc, lpnmCD->rc.left + 3,
						lpnmCD->rc.top + 3,
						lpnmCD->rc.right - 3,
						lpnmCD->rc.bottom - 3, 5, 5);

					return TRUE;
				}
			}
			break;
		} // CustomDRAW
		} // code
		break;
	} // WM_NOTIFY

	case WM_DRAWITEM:  // This is due to BS_OWNERDRAW.  Needed to set the background color. What a pain!
		
	{
		int state;
		HWND hBut;
		hBut = NULL;
		std::tstring msgString;
		size_t maxlen = 256;
		msgString = cSimOn;
		//log_message("WM_DRAWITEM");
		if (wParam == IDC_SIM_ON_BTN) {
			//state = gRadarState.SimOn;
			state = dRadarState.SimOn;
			hBut = GetDlgItem(hWnd, IDC_SIM_ON_BTN);
			if (state){
				msgString = cSimOn;
				maxlen= wcslen(cSimOn); }
			else{
				msgString = cSimOff;
				maxlen = wcslen(cSimOff);
			}
		}
		if (wParam == IDC_REC_PROC_BTN)
		{
			hBut = GetDlgItem(hWnd, IDC_REC_PROC_BTN);
			//state = gRadarState.DataRecording;
			state = dRadarState.DataRecording;
			if (state) {
				msgString = cRecProcOn ;
				maxlen = wcslen(cRecProcOn);
			}
			else {
				msgString = cRecProcOff;
				maxlen = wcslen(cRecProcOff);
			}
		}
		if (wParam == IDC_REC_RAW_BTN)
		{
			hBut = GetDlgItem(hWnd, IDC_REC_RAW_BTN);
			//state = gRadarState.RawRecording;
			state = dRadarState.RawRecording;
			if (state) {
				msgString = cRecRawOn;
				maxlen = wcslen(cRecRawOn);
			}
			else {
				msgString = cRecRawOff;
				maxlen = wcslen(cRecRawOff);
			}
		}

			//log_message("Update sim on button");
			LPDRAWITEMSTRUCT lpDIS = (LPDRAWITEMSTRUCT)lParam;
			RECT rect;
			if (hBut != NULL) {
				GetClientRect(hBut, &rect);

				if (state) {
					SetDCBrushColor(lpDIS->hDC, RGB(0, 128, 0));  // Make button green
					SetTextColor(lpDIS->hDC, RGB(0, 0, 0));	// Make text black

				}
				else {
					SetDCBrushColor(lpDIS->hDC, GetSysColor(COLOR_BTNFACE)); // Make fill default
					SetTextColor(lpDIS->hDC, GetSysColor(COLOR_BTNTEXT));
				}

				SelectObject(lpDIS->hDC, GetStockObject(DC_BRUSH));
				RoundRect(lpDIS->hDC, lpDIS->rcItem.left, lpDIS->rcItem.top,
					lpDIS->rcItem.right, lpDIS->rcItem.bottom, 5, 5);
				SetBkMode(lpDIS->hDC, TRANSPARENT);
				SetTextAlign(lpDIS->hDC, TA_CENTER | TA_BASELINE);
				TextOut(lpDIS->hDC, rect.right / 2, rect.bottom / 2, msgString.c_str(), (int) msgString.length());
				
				return TRUE;
			}
			else return FALSE;
				
	}
	break;
	case WM_USER: // Message if just update the display		
	{
		hdc = GetDC(hWnd);
		
		GetClientRect(hWnd, &rectImage);
		rectImage.left += iControlPanelWidth + BORDER + 10;

		UpdateImages(hWnd, hdc, rectImage);

		// Cleanup
		ReleaseDC(hWnd, hdc);

		break;
	}
	case WM_HSCROLL:
	{
		UpdateCBLabel();
	}
	break;

	case WM_CLOSE:
	case WM_QUIT:
	case WM_DESTROY:
	{
		stop_radar();
		StopRadarDisp();

		PostQuitMessage(0);
		break;
	}

	case WM_CHAR:
	{
		// This should also call an ascii key handler so that it doesn't matter which window has focus
		// key code is in wParam.  See e.g. swprintf_s(msg, L"WM_CHAR: %c\n", (wchar_t)wParam);
		// and OutputDebugString(msg);
		int key;
		key = (int)wParam;
		std::atomic<bool> exitflag = 0;  // Tis is for compatibilty with the console monitor. 
		// interpreKey will start shut down if appropriate key is pressed
		
		int retval = interpretKey(key, std::ref(exitflag));
		if (!retval) {
			updateButtonStatus();
		}
		//else { log_message("No key interpreted"); }
		break;
	}

	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

void updateButtonStatus()
{
	log_message("Key pressed. Sending message");
	InvalidateRect((HWND)hwndButtonRecRaw, NULL, TRUE);
	InvalidateRect(hwndButtonRecProc, NULL, TRUE);
}


// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
		return (INT_PTR)TRUE;

	case WM_COMMAND:
	{
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}
	}
	return (INT_PTR)FALSE;
}

void CreateDisplayControls(HWND hwnd) 
{
	INITCOMMONCONTROLSEX icex;

	icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
	icex.dwICC = ICC_LISTVIEW_CLASSES;
	InitCommonControlsEx(&icex);	
	
	// First the trackbar controls
	HWND hLeftLabel1 = CreateWindowW(L"Static", L"Scale",
		WS_CHILD | WS_VISIBLE, 0, 0, 80-BORDER, 25, hwnd, (HMENU)1, NULL, NULL);
	HWND hLeftLabel2 = CreateWindowW(L"Static", L"Reference",
		WS_CHILD | WS_VISIBLE, 0, 0, 80 - BORDER, 25, hwnd, (HMENU)1, NULL, NULL);

	HWND hRightLabel1 = CreateWindowW(L"Static", L"",
		WS_CHILD | WS_VISIBLE, 0, 0, 30, 25, hwnd, (HMENU)2, NULL, NULL);
	HWND hRightLabel2 = CreateWindowW(L"Static", L"",
		WS_CHILD | WS_VISIBLE, 0, 0, 30, 25, hwnd, (HMENU)2, NULL, NULL);

	// Create the Displays control Group Box

	hTBGroup = CreateWindowW(L"Button", L"Display Control",
		WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
		iDispControlPanPosX, iDispControlPanPosY, iControlPanelWidth, 195, hwnd, (HMENU)0, NULL, NULL);
	// This is the scale
	hTrack1 = CreateWindowW(TRACKBAR_CLASSW, L"Trackbar1",
		WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS,
		iDispControlPanPosX + 80, iDispControlPanPosY + 20, 80, 25, hwnd, (HMENU)3, NULL, NULL);
	// Range
	SendMessageW(hTrack1, TBM_SETRANGE, TRUE, MAKELONG(0, 9));
	SendMessageW(hTrack1, TBM_SETTICFREQ, 1, 0);
	//int defPos = (int)((gRadarState.DispRange - 10.0) / 10.0);
	int defPos = (int)((dRadarState.DispRange - 10.0) / 10.0);
	
	SendMessageW(hTrack1, TBM_SETPOS, FALSE, defPos);
	SendMessageW(hTrack1, TBM_SETBUDDY, TRUE, (LPARAM)hLeftLabel1);
	SendMessageW(hTrack1, TBM_SETBUDDY, FALSE, (LPARAM)hRightLabel1);

	// This is the reference level
	hTrack2 = CreateWindowW(TRACKBAR_CLASSW, L"Trackbar2",
		WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS,
		iDispControlPanPosX + 80, iDispControlPanPosY + 50, 80, 25, hwnd, (HMENU)3, NULL, NULL);
	//Scale
	SendMessageW(hTrack2, TBM_SETRANGE, TRUE, MAKELONG(0, 10));
	SendMessageW(hTrack2, TBM_SETTICFREQ, 1, 0);
	//defPos = (int) ( ( gRadarState.RefLeveldB-gRadarConfig.MinRefLevel ) / 10.0);
	defPos = (int)((dRadarState.RefLeveldB - dRadarConfig.MinRefLevel) / 10.0);
	//std::cout << "Initial pos = " << defPos << " Ref, Min = " << gRadarState.RefLeveldB << " " << gRadarConfig.MinRefLevel << std::endl;
	SendMessageW(hTrack2, TBM_SETPOS, FALSE, defPos);
	SendMessageW(hTrack2, TBM_SETBUDDY, TRUE, (LPARAM)hLeftLabel2);
	SendMessageW(hTrack2, TBM_SETBUDDY, FALSE, (LPARAM)hRightLabel2);
	defPos = (int) SendMessageW(hTrack2, TBM_GETPOS, 0, 0);
	
	// Now create radio buttons for selecting the colormap
	int cmSelBaseLeft = iDispControlPanPosX + 5;
	int cmSelBaseTop = iDispControlPanPosY + 80;
	int buttonInc = 25;
	CreateWindowW(L"Button", L"Colormap",
		WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
		cmSelBaseLeft, cmSelBaseTop, 90, 110, hwnd, (HMENU)0, NULL, NULL);
	HWND hHotRadioBut = CreateWindowW(L"Button", L"Hot",
		WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
		cmSelBaseLeft + 10, cmSelBaseTop + buttonInc, 70, 30, hwnd, (HMENU)ID_CM_HOT, NULL, NULL);
	SendMessageW(hHotRadioBut, BM_SETCHECK, BST_CHECKED, 1);
	CreateWindowW(L"Button", L"Jet",
		WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
		cmSelBaseLeft + 10, cmSelBaseTop + 2 * buttonInc, 70, 30, hwnd, (HMENU)ID_CM_JET, NULL, NULL);
	CreateWindowW(L"Button", L"Gray",
		WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
		cmSelBaseLeft + 10, cmSelBaseTop + 3 * buttonInc, 70, 30, hwnd, (HMENU)ID_CM_GRAY, NULL, NULL);
	// This is the up/down button to control the offset
	CreateWindowW(L"Button", L"Options",
		WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
		cmSelBaseLeft + 105, cmSelBaseTop, 95, 110, hwnd, (HMENU)0, NULL, NULL);
	hPeakOverlay = CreateWindowW(L"Button", L"Peak",
		WS_CHILD | WS_VISIBLE | BS_CHECKBOX,
		cmSelBaseLeft + 120, cmSelBaseTop+30, 70, 25, hwnd, (HMENU)IDC_PEAK_OVERLAY_BTN, NULL, NULL);
	CheckDlgButton(hwnd, IDC_PEAK_OVERLAY_BTN, BST_CHECKED);
	return;
}
void UpdateConfigText(void)
{
	wchar_t buf[256];
	const wchar_t *labels = L"Frequency\nBandwidth\nSample Rate\nWRI Length\nCPI Length";

	SetWindowTextW(hwndConfigStaticTextLeft, labels);

	swprintf(buf, sizeof(buf)/sizeof(buf[0]), L"%-3.1lf  GHz\n%-5.1lf MHz\n%-6.0lf Hz\n%-10d\n%-10d",
		dRadarConfig.CenterFreq/1.0e9, dRadarConfig.Bandwidth / 1.0e6,
		dRadarConfig.SampleRate, dRadarConfig.NSamplesPerWRI,
		dRadarConfig.NWRIPerCPI);

	SetWindowTextW(hwndConfigStaticTextRight, buf);

}
void CreateConfigControls(HWND hWnd)
{
	hwndConfigBox = CreateWindow(
		L"BUTTON",  // Predefined class; Unicode assumed 
		L"Configuration",      // Button text 
		WS_VISIBLE | WS_CHILD | BS_GROUPBOX,
		iConfigPanPosX,         // x position 
		iConfigPanPosY,         // y position 
		iControlPanelWidth,        // Width
		iConfigPanPosHeight,        // Height
		hWnd,     // Parent window
		0,
		(HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE),
		NULL);      // Pointer not needed.
	hwndConfigStaticTextLeft = CreateWindowW(WC_STATICW, L"", WS_CHILD | WS_VISIBLE | SS_LEFT,
		iConfigPanPosX + BORDER,         // x position 
		iConfigPanPosY + 20, iControlPanelWidth / 2 - BORDER, iConfigPanPosHeight - 30, hWnd, (HMENU)ID_STATICUD1, NULL, NULL);
	hwndConfigStaticTextRight = CreateWindowW(WC_STATICW, L"", WS_CHILD | WS_VISIBLE | SS_LEFT,
		iConfigPanPosX + iControlPanelWidth / 2, //+BORDER,         // x position 
		iConfigPanPosY + 20, iControlPanelWidth / 2 - BORDER, iConfigPanPosHeight - 30, hWnd, NULL, NULL, NULL);
	return;
}
void CreateRecordControls(HWND hWnd)
{
	int ButXpos, ButYBase;
	ButXpos = iRecordPanPosX + iControlPanelWidth / 2 - BUTWIDTH / 2;
	ButYBase = iRecordPanPosY + BORDER + 20;
	
	hwndControlBox = CreateWindow(
		L"BUTTON",  // Predefined class; Unicode assumed 
		L"Recording",      // Button text 
		WS_VISIBLE | WS_CHILD | BS_GROUPBOX ,
		iRecordPanPosX, iRecordPanPosY,         // x,y position 
		iControlPanelWidth,        // Width
		2 * BUTHEIGHT + BORDER * 4 + 20,        //  height
		hWnd,     // Parent window
		0,
		(HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE),
		NULL);      // Pointer not needed.

	hwndButtonRecProc=CreateWindowW(L"Button", L"Record Results",
		//WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON | BS_TEXT | BS_CENTER | BS_VCENTER, // | BS_OWNERDRAW
		WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
		ButXpos, ButYBase, BUTWIDTH, BUTHEIGHT,
		hWnd, (HMENU)IDC_REC_PROC_BTN, NULL, NULL);

	hwndButtonRecRaw = CreateWindowW(L"Button", L"Record Raw",
		WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,  //| BS_VCENTER , //
		ButXpos, ButYBase + BUTHEIGHT + BORDER, BUTWIDTH, BUTHEIGHT,
		hWnd, (HMENU)IDC_REC_RAW_BTN, NULL, NULL);

	return;
}
void CreateSimControls(HWND hWnd)
{
	static HBITMAP hBitmap;
	int ButXpos, ButYBase;

	ButXpos = iSimControlPanPosX + iControlPanelWidth / 2 - BUTWIDTH / 2;
	ButYBase = iSimControlPanPosY + BORDER + 20;

	hTBGroup = CreateWindowW(L"Button", L"Sim Control",
		WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
		iSimControlPanPosX, iSimControlPanPosY,
		iControlPanelWidth, 2 * BUTHEIGHT + 2 * BORDER + 20, hWnd, (HMENU)0, NULL, NULL);

	hwndButtonSim = CreateWindowW(L"Button", L"Sim On/off",
		//WS_VISIBLE | WS_CHILD,
		WS_VISIBLE | WS_CHILD | BS_OWNERDRAW ,
		ButXpos,
		ButYBase, BUTWIDTH, BUTHEIGHT, hWnd, (HMENU)IDC_SIM_ON_BTN, NULL, NULL);
	
	int edwinwid = 70;
	hStaticSim = CreateWindowW(WC_STATICW, L"Amplitude (dB)", WS_CHILD | WS_VISIBLE | SS_LEFT,
		iSimControlPanPosX + BORDER + 10, iSimControlPanPosY + BUTHEIGHT + BORDER + 30,
		iControlPanelWidth - 2 * BORDER - edwinwid, 25, hWnd, NULL, NULL, NULL);
	// Up/down control
	hUpDown2 = CreateWindowW(UPDOWN_CLASSW, NULL, WS_CHILD | WS_VISIBLE
		| UDS_SETBUDDYINT | UDS_ALIGNRIGHT,
		0, 0, 0, 0, hWnd, (HMENU)ID_UPDOWN2, NULL, NULL);

	hEditUD2 = CreateWindowExW(WS_EX_CLIENTEDGE, WC_EDITW, NULL, WS_CHILD
		| WS_VISIBLE | ES_RIGHT,
		iSimControlPanPosX + iControlPanelWidth - BORDER - edwinwid, iSimControlPanPosY + BUTHEIGHT + BORDER + 30,
		edwinwid, 25, hWnd,
		(HMENU)ID_EDITUD2, NULL, NULL);

	SendMessageW(hUpDown2, UDM_SETBUDDY, (WPARAM)hEditUD2, 0);
	SendMessageW(hUpDown2, UDM_SETRANGE, 0, MAKELPARAM(UD_SIM_MAX_POS, UD_SIM_MIN_POS));
	int SimAmpPos;
	SimAmpPos = (int) dRadarState.SimAmp;
	SendMessageW(hUpDown2, UDM_SETPOS, 0,SimAmpPos);

	// Just for fun, load and display a radar image.
	hBitmap = (HBITMAP)LoadImageW(NULL, L"c:/data/tradex.bmp", IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
	if (hBitmap) {
		log_message("Bitmap loaded.");

		HWND hwndPicture = CreateWindowW(WC_STATICW, L"Picture", WS_CHILD | WS_VISIBLE | SS_BITMAP,
			BORDER + 50, iSimControlPanPosY + 110,
			40, 80, hWnd, NULL, NULL, NULL);
		SendMessage(hwndPicture, STM_SETIMAGE, (WPARAM)IMAGE_BITMAP, (LPARAM)hBitmap);
	}
	else { log_message("Bitmap not loaded."); }
	return;
}

void updateGUI()
{
	 
	//SendMessage(hwndButtonRecProc, BM_SETSTATE, dRadarState.DataRecording,0);
	//SendMessage(hwndButtonRecRaw, BM_SETSTATE, dRadarState.RawRecording, 0);
	
	//SendMessage(hPeakOverlay, BM_SETCHECK, dRadarState.PeakOverlay,0);
	//SendMessage(hwndButtonSim, BM_SETCHECK, dRadarState.SimOn,0);
	int SimAmpPos;
	SimAmpPos = (int)dRadarState.SimAmp;
	SendMessageW(hUpDown2, UDM_SETPOS, 0, SimAmpPos);

	int defPos = (int)((dRadarState.DispRange - 10.0) / 10.0);
	SendMessageW(hTrack1, TBM_SETPOS, FALSE, defPos);
	defPos = (int)((dRadarState.RefLeveldB - dRadarConfig.MinRefLevel) / 10.0);
	SendMessageW(hTrack2, TBM_SETPOS, FALSE, defPos);

	return;
}
#endif

#endif /* WINGUI*/

#endif /* RTP_HEADLESS */