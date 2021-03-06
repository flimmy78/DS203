#include "Wnd.h"

/*static*/ CWnd* 									CWnd::m_pTop = NULL;
/*static*/ ui16 									CWnd::m_nInstances = 0;
/*static*/ CWnd* 									CWnd::m_pFocus = NULL;
/*static*/ CWnd::CTimer 					CWnd::m_arrTimers_[16];
/*static*/ CArray<CWnd::CTimer> 	CWnd::m_arrTimers;
/*static*/ CWnd::CModal						CWnd::m_arrModals_[8];
/*static*/ CArray<CWnd::CModal> 	CWnd::m_arrModals;
/*static*/ CRect 									CWnd::m_rcOverlay;

CWnd::CWnd()
{
	m_nInstances++;
	m_pParent = NULL;
	if (!m_pTop)
	{
		m_pTop = this;
		m_arrTimers.Init( m_arrTimers_, COUNT(m_arrTimers_) );
		m_arrModals.Init( m_arrModals_, COUNT(m_arrModals_) );
		m_rcOverlay.Invalidate();
	}
}

CWnd* CWnd::GetParent()
{
	return m_pParent;
}

CWnd* CWnd::GetFocus()
{
	return m_pFocus;
}

CWnd* CWnd::GetLast()
{
	CWnd *pWnd = m_pFirst;
	while (pWnd && pWnd->m_pNext)
		pWnd = pWnd->m_pNext;
	return pWnd;
}

CWnd* CWnd::GetPrev()
{
	if (!m_pParent)
		return NULL; // top window
	_ASSERT( m_pParent && m_pParent->m_pFirst);
	CWnd *pWnd = m_pParent->m_pFirst;		
	if (pWnd == this)
		return NULL;
	_ASSERT(pWnd);
	while (pWnd->m_pNext)
	{
		if (pWnd->m_pNext == this)
			return pWnd;
		pWnd = pWnd->m_pNext;
	}
	_ASSERT(0);
	return NULL;
}

void CWnd::Destroy()
{
	// najprv znic childy
	CWnd *pChild = m_pFirst;
	while (pChild)
	{
		pChild->Destroy();
		pChild = pChild->m_pNext;
	}
	
	// odstran zo zoznamu
	if (m_pParent->m_pFirst == this)
		m_pParent->m_pFirst = this->m_pNext; 
	else
		GetPrev()->m_pNext = this->m_pNext;

	// a teraz mozes seba
	m_pParent = NULL;
	m_dwFlags = WsHidden;
}

void CWnd::Create( const char* pszId, ui16 dwFlags, const CRect& rc, CWnd* pParent )
{
	_ASSERT( m_pParent == NULL ); // Already created
	m_pszId = pszId;
	m_rcClient = rc;
	m_pParent = pParent;
	m_pFirst = NULL;
	m_pNext = NULL;
	m_dwFlags = dwFlags;

	if (!m_pParent)
		return;

	_ASSERT( m_pParent );
	if ( !m_pParent->GetLast() )
	{
		m_pParent->m_pFirst = this;
	} else
	{
		m_pParent->GetLast()->m_pNext = this;
	}
}

/*virtual*/ void CWnd::OnPaint()
{
}

/*virtual*/ void CWnd::OnTimer()
{
}

/*virtual*/ void CWnd::OnKey(ui16 nKey)
{
	if ( nKey & BIOS::KEY::KeyDown )
	{
		_ASSERT( m_pFocus == this ); // ja mam focus!
		CWnd *pFocus = _GetNextActiveWindow();
		if (pFocus)
		{
			// najdi prvy viditelny child ktore moze byt focusovatelny
			if (pFocus->m_pFirst)
				pFocus = pFocus->m_pFirst;	// staci !?

			pFocus->SetFocus();
			this->Invalidate();
			pFocus->Invalidate();
		}
	}

	if ( nKey & BIOS::KEY::KeyUp )
	{
		_ASSERT( m_pFocus == this ); // ja mam focus!
		CWnd *pFocus = _GetPrevActiveWindow();
		if (pFocus)
		{
			if (pFocus->GetLast())
				pFocus = pFocus->GetLast();	// staci !?

			pFocus->SetFocus();
			this->Invalidate();
			pFocus->Invalidate();
		}
	}
}

/*virtual*/ void CWnd::OnMessage(CWnd* pSender, ui16 code, ui32 data)
{
}

/*virtual*/ void CWnd::WindowMessage(int nMsg, int nParam /*=0*/)
{
	switch (nMsg)
	{
		case WmPaint:
		{
			OnPaint();

			CWnd *pChild = m_pFirst;
			while (pChild)
			{
				if ( pChild->m_dwFlags & WsVisible )
					pChild->WindowMessage( WmPaint );
				pChild = pChild->m_pNext;
			}
		}	
		break;
		case WmKey:
		{
			if ( GetActiveWindow() )
				GetActiveWindow()->OnKey( nParam );
		}
		break;
		case WmTick:
		{
			if ( m_pParent == NULL )
				_UpdateTimers();

			if ( m_dwFlags & WsTick )
				SendMessage( this, ToWord('t', 'i'), 0 );

			CWnd *pChild = m_pFirst;
			while (pChild)
			{
				if ( pChild->m_dwFlags & WsVisible )
					pChild->WindowMessage( WmTick );
				pChild = pChild->m_pNext;
			}
		}
		break;
		case WmBroadcast:
		{
			if ( m_dwFlags & WsListener )
				OnMessage( NULL, WmBroadcast, nParam );

			CWnd *pChild = m_pFirst;
			while (pChild)
			{
				if ( pChild->m_dwFlags & WsVisible )
					pChild->WindowMessage( WmBroadcast, nParam );
				pChild = pChild->m_pNext;
			}
		}
		break;
	}	
}

void CWnd::SetFocus()
{
	m_pFocus = this;
}

ui8 CWnd::HasFocus()
{
	return this == m_pFocus;
}

CWnd* CWnd::GetActiveWindow()
{
	return m_pFocus;
}
void CWnd::Invalidate()
{
	// TODO: najprv iba nastavit flag tomuto oknu a vsetkym childom, je nejake okno (dialog) nadomnou !?
	WindowMessage(WmPaint);
}
void CWnd::Update()
{
	// TODO: a potom rekurzivne prejst stromom a refreshnut potrebne	
	if ( m_dwFlags & WsModal )
		WindowMessage(WmPaint);
	else
	{
		CWnd *pChild = m_pFirst;
		while (pChild)
		{
			if ( pChild->m_dwFlags & WsVisible )
				pChild->Update();
			pChild = pChild->m_pNext;
		}
	}
}
void CWnd::SendMessage(CWnd* pTarget, ui16 code, ui32 data)
{
	pTarget->OnMessage(this, code, data);
}
void CWnd::ShowWindow(ui8 sh)
{
	if ( sh == SwShow )
		m_dwFlags |= WsVisible;
	else
		m_dwFlags &= ~WsVisible;
}

CWnd* CWnd::_GetNextActiveWindow()
{
	CWnd *pWnd = m_pNext;

	while ( pWnd )
	{
		if ( (pWnd->m_dwFlags & WsVisible) && (!(pWnd->m_dwFlags & WsNoActivate)) )
			break;
		pWnd = pWnd->m_pNext;
	}

	if (!pWnd && m_pParent && (m_pParent->m_dwFlags & WsModal))
		return NULL;

	if (!pWnd && m_pParent )
		return m_pParent->_GetNextActiveWindow();

	return pWnd;
}
CWnd* CWnd::_GetPrevActiveWindow()
{
	CWnd *pWnd = GetPrev();
	while ( pWnd && ((!pWnd->m_dwFlags & WsVisible) || (pWnd->m_dwFlags & WsNoActivate)) )
		pWnd = pWnd->GetPrev();

	if (!pWnd && m_pParent && (m_pParent->m_dwFlags & WsModal))
		return NULL;

	if (!pWnd && m_pParent)
		return m_pParent->_GetPrevActiveWindow();

	return pWnd;
}

void CWnd::SetTimer(ui32 nInterval)
{
	m_arrTimers.Add( CTimer(this, nInterval) ); 
}

void CWnd::KillTimer()
{
	for ( int i = 0; i < m_arrTimers.GetSize(); i++ )
		if ( m_arrTimers[i].m_pWnd == this )
		{
			m_arrTimers.RemoveAt(i); 
			break;
		}	
}

void CWnd::_UpdateTimers()
{
	ui32 nTick = BIOS::GetTick();

	for ( int i = 0; i < m_arrTimers.GetSize(); i++ )
	{
		CTimer& timer = m_arrTimers[i];
		if ( /*(si32)(nTick - timer.m_nLast)*/ nTick > timer.m_nNext )
		{
			_ASSERT( timer.m_pWnd->m_dwFlags & CWnd::WsVisible );
			timer.m_pWnd->OnTimer();
			timer.m_nNext = BIOS::GetTick() + timer.m_nInterval;
		}
	}
}

bool CWnd::IsWindow()
{
	return m_pParent ? true : false;
}

bool CWnd::IsVisible()
{
	CWnd* pWnd = this;
	while ( pWnd->m_dwFlags & WsVisible )
	{
		pWnd = pWnd->GetParent();
		if ( !pWnd )
			return true;
	}
	return false;
}

void CWnd::StartModal( CWnd* pwndChildFocus /*= NULL*/ )
{
	m_arrModals.Add( CModal() );

	if ( pwndChildFocus )
		pwndChildFocus->SetFocus();
	else
		SetFocus();

	// redraw with disabled focus
	m_arrModals.GetLast().m_pPrevFocus->Invalidate();

	m_rcOverlay |= m_rcClient;

	Invalidate();	
}

void CWnd::StopModal()
{
//	CWnd* pParent = m_pParent;

	Destroy();
	m_rcOverlay.Invalidate();
	m_arrModals.GetLast().m_pPrevFocus->SetFocus();
	// redraw everything without any limits
	m_pTop->Invalidate();
	m_rcOverlay = m_arrModals.GetLast().m_rcPrevOverlay;
	m_arrModals.RemoveLast();
}
