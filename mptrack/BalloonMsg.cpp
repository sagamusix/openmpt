#include "StdAfx.h"
#include "BalloonMsg.h"

//////////////////////////////////////////////////////////////
// Initialize statics. Changes to these values only kick in when
// the next balloon is displayed.

UINT	CBalloonMsg::s_nInitialDelay	= 0;		// No delay, display immediately. NOW REDUNDANT!
UINT	CBalloonMsg::s_nAutoPop			= 10000;	// Stay up for 10 secs unless RequestCloseAll() is used. Set to zero for unlimited
UINT	CBalloonMsg::s_nMaxTipWidth		= 250;		// Forces line breaks in the body text
UINT	CBalloonMsg::s_nToolBorder		= 10;		// Used to judge when the mouse has moved enough to pop the balloon
UINT	CBalloonMsg::s_nTimerStep		= 30;		// Elapse time in millisecs for our timer
//////////////////////////////////////////////////////////////
#define ENABLEBALLOONS_SUBKEY	_T("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced")
#define ENABLEBALLOONS_VALUE	_T("EnableBalloonTips")
//////////////////////////////////////////////////////////////
#ifndef TTI_INFO
	// ToolTip Icons possible wParam values for TTM_SETTITLE message
	#define TTI_INFO                1
	#define TTI_WARNING             2
	#define TTI_ERROR               3
#endif // !TTI_INFO
//////////////////////////////////////////////////////////////

void CBalloonMsg::Show( LPCTSTR lpszHdr, LPCTSTR lpszBody, LPPOINT pPt /*= NULL*/, HICON hIcon /*= NULL*/ )
// hIcon can be actual icon handle or when running on XP SP2 or better
// the following special UINT values may be used:
//		1 : Info icon.
//		2 : Warning icon
//		3 : Error Icon
//
// If supplied, the point should be in screen coords
{
	// Create the thread..
	CBalloonMsgThread* pThread = (CBalloonMsgThread*) ::AfxBeginThread( RUNTIME_CLASS(CBalloonMsgThread), THREAD_PRIORITY_NORMAL, 0, CREATE_SUSPENDED );
	
	// Initialise the thread and its transparent balloon parent window...
	pThread->m_dwIDPrimaryThread = ::GetCurrentThreadId();
	pThread->m_bAutoDelete = TRUE;
	pThread->m_Wnd.m_strHdr = lpszHdr;
	pThread->m_Wnd.m_strBody = lpszBody;
	pThread->m_Wnd.m_hIcon = hIcon;
	
	// If a point was supplied, set up the balloon to be repositioned...
	if ( pPt )
	{
		pThread->m_Wnd.m_bReposition = TRUE;
		pThread->m_Wnd.m_ptReposition = *pPt;
	}
	
	// Clear the CloseAll flag...
	CBalloonMsgThread::ResetCloseAll();
	
	// Let it go...
	pThread->ResumeThread();
}

void CBalloonMsg::Show( UINT nIDStrHdr, UINT nIDStrBody, LPPOINT pPt /*= NULL*/, HICON hIcon /*= NULL*/ )
// Like the other Show method, but the strings are loaded from resources.
{
	CString	strHdr;
	CString	strBody;
	//
	
	// Load up the strings...
	strHdr.LoadString( nIDStrHdr );
	strBody.LoadString( nIDStrBody );
	
	// Delegate to the base version of the Show method...
	CBalloonMsg::Show( strHdr, strBody, pPt, hIcon );
}

void CBalloonMsg::ShowForCtrl( LPCTSTR lpszHdr, LPCTSTR lpszBody, HWND hCtrl, HICON hIcon /*= NULL*/ )
// Centers the ballon within the nominated ctrl
{
	if ( ::IsWindow(hCtrl) )
	{
		CRect	rCtrl;
		CPoint	pt;
		//
		
		// The balloon will be centered in the nominated ctrl...
		::GetWindowRect( hCtrl, rCtrl );
		pt = rCtrl.CenterPoint();
		
		// Delegate to the base version of the Show method...
		CBalloonMsg::Show( lpszHdr, lpszBody, &pt, hIcon );
	}
	else
		CBalloonMsg::Show( lpszHdr, lpszBody, NULL, hIcon );
}

void CBalloonMsg::ShowForCtrl( UINT nIDStrHdr, UINT nIDStrBody, HWND hCtrl, HICON hIcon /*= NULL*/ )
// Centers the ballon within the nominated ctrl. Strings are loaded from resources.
{
	CString	strHdr;
	CString	strBody;
	//
	
	// Load up the strings...
	strHdr.LoadString( nIDStrHdr );
	strBody.LoadString( nIDStrBody );

	// Delegate to the base version of the ShowForCtrl method...
	CBalloonMsg::ShowForCtrl( strHdr, strBody, hCtrl, hIcon );
}

BOOL CBalloonMsg::BalloonsEnabled()
// Verify that balloons have not been suppressed via a registry tweak.
// Return TRUE if balloons are good to go, FALSE otherwise.
//
// Thanks to DerMeister at CodeProject for spotting this!
// http://www.codeproject.com/script/Membership/Profiles.aspx?mid=22871
{
	HKEY	hSubKey = NULL;
	DWORD	dwType = 0;
	DWORD	dwLen = 0;
	DWORD	dwData = 0;
	BOOL	bResult = FALSE; // Play safe - no balloons unless we find evidence to the contrary
	//
	
	if ( ::RegOpenKeyEx(HKEY_CURRENT_USER, ENABLEBALLOONS_SUBKEY, 0, KEY_READ, &hSubKey) == ERROR_SUCCESS )
	{
		if ( (::RegQueryValueEx(hSubKey, ENABLEBALLOONS_VALUE, NULL, &dwType, NULL, &dwLen) == ERROR_SUCCESS) && (REG_DWORD == dwType) )
		{
			// Extract the string data...
			dwLen = sizeof(DWORD);
			if ( ::RegQueryValueEx(hSubKey, ENABLEBALLOONS_VALUE, NULL, &dwType, (LPBYTE)&dwData, &dwLen) == ERROR_SUCCESS )
				bResult = (dwData != 0);
		}
		else // Couldn't get info on value, assume it's not present
			bResult = TRUE;
			
		// Close the key...
		::RegCloseKey( hSubKey );
	}
	
	return bResult;
}

void CBalloonMsg::SafeShowMsg( UINT nStyles, LPCTSTR lpszHdr, LPCTSTR lpszBody, HWND hCtrl )
// Designed for showing simple messages only, this uses the balloon method if the user
// hasn't disabled it, otherwise it'll fall back to using AfxMessageBox.
{
	// These styles are inappropriate for Balloon presentation!
	ASSERT( !(nStyles & (MB_ABORTRETRYIGNORE | MB_OKCANCEL | MB_RETRYCANCEL | MB_YESNO | MB_YESNOCANCEL)) );
	ASSERT( (nStyles & (MB_ICONINFORMATION | MB_ICONEXCLAMATION | MB_ICONSTOP | MB_ICONQUESTION)) != MB_ICONQUESTION );

	if ( CBalloonMsg::BalloonsEnabled() && (hCtrl || CWnd::GetSafeOwner()) )
	{
		HICON	hIcon = 0;
		//
	
		// Determine what icon to show...
		if ( (nStyles & MB_ICONINFORMATION) == MB_ICONINFORMATION )
			hIcon = (HICON) TTI_INFO;
		else if ( (nStyles & MB_ICONEXCLAMATION) == MB_ICONEXCLAMATION )
			hIcon = (HICON) TTI_WARNING;
		else if ( (nStyles & MB_ICONSTOP) == MB_ICONSTOP )
			hIcon = (HICON) TTI_ERROR;
	
		// If we got a ctrl, put the ballon over it.
		if ( hCtrl )
			CBalloonMsg::ShowForCtrl( lpszHdr, lpszBody, hCtrl, hIcon );
		else // Just put the balloon in the centre of the current parent/owner
		{
			CWnd*	pWnd = CWnd::GetSafeOwner( NULL, NULL );
			CRect	rOwner;
			CPoint	pt;
			//
			
			pWnd->GetWindowRect( rOwner );
			pt = rOwner.CenterPoint();
			CBalloonMsg::Show( lpszHdr, lpszBody, &pt, hIcon );
		}
	}
	else // User has disabled balloons, let's go with AfxMessageBox
	{
		TRACE( "CBalloonMsg::SafeShowMsg: Balloons apparently disabled, using AfxMessageBox instead\n" );
		::AfxMessageBox( lpszBody, nStyles );
	}
}

void CBalloonMsg::SafeShowMsg( UINT nStyles, UINT nIDStrHdr, UINT nIDStrBody, HWND hCtrl )
{
	CString	strHdr;
	CString	strBody;
	//
	
	// Load up the strings...
	strHdr.LoadString( nIDStrHdr );
	strBody.LoadString( nIDStrBody );

	// Delegate to the base version of the SafeShowMsg method...
	CBalloonMsg::SafeShowMsg( nStyles, strHdr, strBody, hCtrl );
}

///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
// CBalloonMsgWnd

/////////////////////////////////////////
// Statics
CString CBalloonMsgWnd::s_strWndClass;
/////////////////////////////////////////

IMPLEMENT_DYNAMIC(CBalloonMsgWnd, CWnd)

CBalloonMsgWnd::CBalloonMsgWnd()
{
	m_pTTBuffer = NULL;
	m_hIcon = NULL;
	m_bReposition = FALSE;
	m_ptReposition = CPoint(0,0);
	m_nTimer = 0;
	m_nTimerCount = 0;
}

CBalloonMsgWnd::~CBalloonMsgWnd()
{
	// Cleanup Tooltip buffer...
	delete [] m_pTTBuffer;
}


BEGIN_MESSAGE_MAP(CBalloonMsgWnd, CWnd)
	ON_WM_CREATE()
	ON_NOTIFY_EX_RANGE(TTN_NEEDTEXTW, 0, 0xFFFF, OnToolTipText)
	ON_NOTIFY_EX_RANGE(TTN_NEEDTEXTA, 0, 0xFFFF, OnToolTipText)
	ON_WM_SETCURSOR()
	ON_WM_TIMER()
END_MESSAGE_MAP()

LPCTSTR CBalloonMsgWnd::GetWndClass()
// Returns the MFC-generated class name that describes our combination
// of styles, brush etc.
{
	if ( s_strWndClass.IsEmpty() )
	{
		// Register the new class, with all defaults...
		s_strWndClass = ::AfxRegisterWndClass( NULL, ::AfxGetApp()->LoadStandardCursor(IDC_ARROW) ); 
	}

	return ( s_strWndClass );
}

BOOL CBalloonMsgWnd::Create( CWnd* pParent )
{
	const int	nBorder = CBalloonMsg::s_nToolBorder;
	CPoint		pt;
	BOOL		bResult = FALSE;
	//


	::GetCursorPos( &pt );
	pt.x -= nBorder;
	pt.y -= nBorder;

	// Create the transparent window...
	bResult = this->CreateEx( WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW,		// Extended styles
							  this->GetWndClass(),						// Registered class name
							  _T("TLHBalloonParent"),					// Window name
							  WS_POPUP,									// Conventional styles
							  pt.x, pt.y,								// X, Y
							  nBorder*2, nBorder*2,						// Width & height the same
							  pParent->GetSafeHwnd(),					// Parent window
							  NULL,										// HMENU or ID (n/a)
							  NULL );									// lpParam
	ASSERT( bResult );

	return (bResult);
}

int CBalloonMsgWnd::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	OSVERSIONINFO	osvi;
	TOOLINFO		ti;
	//

	if (CWnd::OnCreate(lpCreateStruct) == -1)
		return -1;

	memset( &osvi, 0, sizeof(osvi) );
	osvi.dwOSVersionInfoSize = sizeof(osvi);

	// Create the tooltip ctrl
	// Version 2: Extended styles apparently no longer required
	m_wndToolTip.CreateEx( this, TTS_ALWAYSTIP | TTS_NOPREFIX | TTS_BALLOON, 0 /*WS_EX_TRANSPARENT | WS_EX_TOPMOST*/ );

	// Add a tool that covers the whole parent window...
	::ZeroMemory( &ti, sizeof(ti) );
	ti.cbSize = sizeof(ti);
    ti.uFlags = TTF_TRACK;
    ti.hwnd = m_hWnd;
    ti.lpszText = LPSTR_TEXTCALLBACK;
    ti.uId = 1;
	this->GetClientRect( &ti.rect );
    VERIFY( m_wndToolTip.SendMessage(TTM_ADDTOOL, 0, (LPARAM)(LPTOOLINFO) &ti) );
	
	// Apply the icon and title
	m_wndToolTip.SetTitle( (UINT) (UINT_PTR) m_hIcon, m_strHdr );
	
	// Reposition if required
	if ( m_bReposition )
		m_wndToolTip.SendMessage( TTM_TRACKPOSITION, 0, (LPARAM)MAKELONG(m_ptReposition.x, m_ptReposition.y) );
		
	// Activate the TT ctrl...
	m_wndToolTip.SendMessage( TTM_TRACKACTIVATE, (WPARAM)TRUE, (LPARAM)&ti );	

	//
	// Start a timer. The timer has a dual function:
	// 1) Lets us make periodic checks for changes in focus/activation in the thread's OnIdle()
	// 2) Allows us to implement auto-pop for our tracking ballon
	//
	m_nTimer = this->SetTimer( 1, CBalloonMsg::s_nTimerStep, NULL );

	return 0;
}

BOOL CBalloonMsgWnd::OnToolTipText( UINT /*id*/, NMHDR* pNMHDR, LRESULT* pResult )
{
	CPoint				ptMouse;
	TOOLTIPTEXTA*		pTTTA = (TOOLTIPTEXTA*)pNMHDR; // For ANSI versions of the msg
	TOOLTIPTEXTW*		pTTTW = (TOOLTIPTEXTW*)pNMHDR; // For Wide versions of the msg
	BOOL				bResult = FALSE;
	//

	// Set the result now...
	*pResult = 0;

	// Limit the max tooltip width in order to enable multi-line behaviour.
	::SendMessage( pTTTW->hdr.hwndFrom, TTM_SETMAXTIPWIDTH, 0, CBalloonMsg::s_nMaxTipWidth );

	try
	{
		// Ignore messages from the built in tooltip, we are processing them internally
		if( !((pNMHDR->idFrom == (UINT_PTR)m_hWnd) &&
			(((pNMHDR->code == TTN_NEEDTEXTA) && (pTTTA->uFlags & TTF_IDISHWND)) ||
			((pNMHDR->code == TTN_NEEDTEXTW) && (pTTTW->uFlags & TTF_IDISHWND)))) )
		{
			// Get the tooltip text...
			if ( !m_strBody.IsEmpty() )
			{
				// Flush the existing buffer...
				delete [] m_pTTBuffer;
				m_pTTBuffer = NULL;

				// Allocate a buffer of the appropriate size, put the text into it in the
				// appropriate format, and supply it to the tooltip ctrl...
				// This is a severe pain the arse!
				#ifndef _UNICODE
					if (pNMHDR->code == TTN_NEEDTEXTA)
					{
						m_pTTBuffer = new char[m_strBody.GetLength() + 1];
						lstrcpyn( (LPTSTR)m_pTTBuffer, m_strBody, m_strBody.GetLength() + 1 );
						pTTTA->lpszText = (LPSTR) m_pTTBuffer;
					}
					else
					{
						m_pTTBuffer = new WCHAR [m_strBody.GetLength() + 1];
						_mbstowcsz( (LPWSTR) m_pTTBuffer, m_strBody, m_strBody.GetLength() + 1 );
						pTTTW->lpszText = (LPWSTR) m_pTTBuffer;
					}
				#else
					if (pNMHDR->code == TTN_NEEDTEXTA)
					{
						m_pTTBuffer = new char [2*(m_strBody.GetLength() + 1)];
						_wcstombsz( (char*) m_pTTBuffer, m_strBody, 2*(m_strBody.GetLength() + 1) );
						pTTTA->lpszText = (LPSTR) m_pTTBuffer;
					}
					else
					{
						m_pTTBuffer = new WCHAR [m_strBody.GetLength() + 1];
						lstrcpyn( (LPTSTR)m_pTTBuffer, m_strBody,m_strBody.GetLength() + 1 );
						pTTTW->lpszText = (LPWSTR) m_pTTBuffer;
					}
				#endif

				// Success!
				bResult = TRUE;
			}
		}
	}
	catch ( CException* e )
	{
		// Quietly suppress the exception...
		TRACE( _T("Exception in CBalloonMsgWnd::OnToolTipText\n") );
		e->Delete();

		// Indicate failure...
		bResult = NULL;
	}

	return bResult;
}

BOOL CBalloonMsgWnd::PreTranslateMessage(MSG* pMsg)
{
	// We give our tooltip a shot at every message
	if ( ::IsWindow(m_wndToolTip.m_hWnd) )
		m_wndToolTip.RelayEvent( pMsg );
		
	return CWnd::PreTranslateMessage(pMsg);
}

BOOL CBalloonMsgWnd::OnSetCursor(CWnd* /*pWnd*/, UINT /*nHitTest*/, UINT /*message*/)
{
	::SetCursor( ::AfxGetApp()->LoadStandardCursor(IDC_ARROW) );
	return TRUE;
}

BOOL CBalloonMsgWnd::OnNotify(WPARAM wParam, LPARAM lParam, LRESULT* pResult)
{
	NMHDR*	pHdr = (NMHDR*) lParam;
	//
	
	if ( pHdr->hwndFrom == m_wndToolTip.m_hWnd )
	{
		switch ( pHdr->code )
		{
			case TTN_SHOW:
				TRACE( "CBalloonMsgWnd::OnNotify, TTN_SHOW\n" );
			break;
		
			case TTN_POP:
			{
				// The tooltip is closing - let's make sure our thread does likewise
				TRACE( "CBalloonMsgWnd: Closing due to TTN_POP\n" );
				::PostQuitMessage( 0 );
				*pResult = 0;
				return TRUE;
			}
			break;
			
			default:
			break;
		}// end of switch
	}

	return CWnd::OnNotify(wParam, lParam, pResult);
}

void CBalloonMsgWnd::OnTimer(UINT_PTR nIDEvent)
// The timer keeps events flowing, so that the thread's OnIdle() method
// is called frequently. Also, it gives us a chance to "autopop" if required.
{
	if ( CBalloonMsg::s_nAutoPop && ::IsWindow(m_wndToolTip.m_hWnd) && ::IsWindowVisible(m_wndToolTip.m_hWnd) )
	{
		// Count the number of timer events since the tooltip appeared
		m_nTimerCount++;
		
		// Have we out stayed our welcome?
		if ( (CBalloonMsg::s_nTimerStep * m_nTimerCount) >= CBalloonMsg::s_nAutoPop )
		{
			TRACE( "CBalloonMsgWnd::OnTimer, closing due to autopop time\n" );
			::PostQuitMessage( 0 );
		}
	}
	
	// Call the inherited method...
	CWnd::OnTimer(nIDEvent);
}

///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
// CBalloonMsgThread

///////////////////////////////////
// Initialize Statics
BOOL		CBalloonMsgThread::s_bExitAll = FALSE;
///////////////////////////////////

IMPLEMENT_DYNCREATE(CBalloonMsgThread, CWinThread)

CBalloonMsgThread::CBalloonMsgThread()
{
	m_dwIDPrimaryThread = 0;
	m_bExit = FALSE;
	::ZeroMemory( &m_GTI, sizeof(m_GTI) );
}

CBalloonMsgThread::~CBalloonMsgThread()
{
}

BOOL CBalloonMsgThread::InitInstance()
{
	m_pMainWnd = &m_Wnd;
	VERIFY( m_Wnd.Create(CWnd::GetActiveWindow()) );
	m_Wnd.ShowWindow( SW_SHOWNOACTIVATE );		// Don't activate - we don't want the orignal window to lose its state
	TRACE( "STARTING: CBalloonMsgThread\n" );
	return TRUE;
}

int CBalloonMsgThread::ExitInstance()
{
	TRACE( "TERMINATING: CBalloonMsgThread\n" );
	return CWinThread::ExitInstance();
}

BEGIN_MESSAGE_MAP(CBalloonMsgThread, CWinThread)
END_MESSAGE_MAP()

void CBalloonMsgThread::GetWindowStates( LPGUITHREADINFO pGTI )
{
	::ZeroMemory( pGTI, sizeof(GUITHREADINFO) );
	pGTI->cbSize = sizeof(GUITHREADINFO);
	VERIFY( ::GetGUIThreadInfo(m_dwIDPrimaryThread, pGTI) );
}

// CBalloonMsgThread message handlers

BOOL CBalloonMsgThread::OnIdle(LONG lCount)
{
	// If any of our exit flags are set, kick off a graceful exit.
	if ( m_bExit || s_bExitAll )
		::PostQuitMessage( 0 );
	
	// If the tooltip is up and running...
	if ( ::IsWindow(m_Wnd.m_wndToolTip.m_hWnd) && ::IsWindowVisible(m_Wnd.m_wndToolTip.m_hWnd) )
	{
		// If we've got a snapshot to compare with...
		if ( m_GTI.hwndFocus )
		{
			GUITHREADINFO gti;
			this->GetWindowStates( &gti );
			if ( (m_GTI.hwndFocus != gti.hwndFocus) || (m_GTI.hwndActive != gti.hwndActive) )
			{
				// A change in focus/activation is a good indication that the user is doing something new...
				TRACE( "CBalloonMsgThread: Focus/Activation change; Closing!\n" );
				::PostQuitMessage( 0 ); // We're done!
			}
			else if ( (m_GTI.hwndCapture != gti.hwndCapture) || (m_GTI.hwndCapture != gti.hwndCapture) )
			{
				// A change in mouse capture is another good indication that the user
				// is doing something new...
				TRACE( "CBalloonMsgThread: capture change; Closing!\n" );
				::PostQuitMessage( 0 ); // We're done!
			}
			else if ( (m_GTI.flags & ~GUI_CARETBLINKING) != (gti.flags & ~GUI_CARETBLINKING) )
			{
				// Other state changes can be initiated by keys etc.
				// Look for these (ignoring caret blinks!) and quit out...
				TRACE( "CBalloonMsgThread: Miscellaneous state change; Closing!\n" );
				::PostQuitMessage( 0 ); // We're done!
			}
		}
		else
		{
			// Take the initial snapshot now that the tooltip has appeared.
			// We wouldn't want to take it earlier because we'd see "false"
			// changes...
			this->GetWindowStates( &m_GTI );
		}
	}
	
	// Otherwise, make sure we're showing a neutral cursor...	
	::SetCursor( ::AfxGetApp()->LoadStandardCursor(IDC_ARROW) );
	
	// And it's business as usual
	return CWinThread::OnIdle(lCount);
}

