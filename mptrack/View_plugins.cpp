#include "stdafx.h"
#include "View_plugins.h"
#include "Childfrm.h"
#include "Globals.h"
#include "HighDPISupport.h"
#include "InputHandler.h"
#include "Mainfrm.h"
#include "Moddoc.h"
#include "MPTrackUtil.h"
#include "RoutingNodes.h"
#include "SelectPluginDialog.h"
#include "WindowMessages.h"

// Keyboard shortcuts
// VU meters
// audio in/out, midi in/out
// Saving UI state (pos/zoom) to song window settings
// Multi-selections (current drag option would be moved to rightclick)
// Maybe create CPoint type that is in graph space and not implicitly convertible to/from CPoint to avoid coordinate system confusion

OPENMPT_NAMESPACE_BEGIN

IMPLEMENT_SERIAL(CViewPlugins, CFormView, 0)

BEGIN_MESSAGE_MAP(CViewPlugins, CModScrollView)
	//{{AFX_MSG_MAP(CViewPlugins)
	ON_WM_PAINT()
	ON_WM_SIZE()
	ON_WM_LBUTTONDBLCLK()
	ON_WM_LBUTTONDOWN()
	ON_WM_MOUSEMOVE()
	ON_WM_LBUTTONUP()
	ON_WM_RBUTTONDOWN()
	ON_WM_MOUSEWHEEL()
	ON_MESSAGE(WM_MOD_KEYCOMMAND, &CViewPlugins::OnCustomKeyMsg)
	ON_MESSAGE(WM_MOD_MIDIMSG,    &CViewPlugins::OnMidiMsg)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


CViewPlugins::~CViewPlugins()
{
	m_font.DeleteObject();
	m_fontBold.DeleteObject();
	m_offScreenDC.DeleteDC();
	m_offScreenBitmap.DeleteObject();
}


void CViewPlugins::OnInitialUpdate()
{
	CModScrollView::OnInitialUpdate();

	m_zoom = m_dpi / 96.0;
	if(m_zoom <= 0.1)
		m_zoom = 0.1;

	UpdateNodes();
	m_selectedNode = m_nodes[0];

	GetClientRect(&m_rcClient);
	UpdateScrollSize();

	GetDocument()->SetNotifications(Notification::Default);
	GetDocument()->SetFollowWnd(m_hWnd);
}


BOOL CViewPlugins::PreTranslateMessage(MSG *pMsg)
{
	if(pMsg)
	{
		//We handle keypresses before Windows has a chance to handle them (for alt etc..)
		if((pMsg->message == WM_SYSKEYUP)   || (pMsg->message == WM_KEYUP) ||
			(pMsg->message == WM_SYSKEYDOWN) || (pMsg->message == WM_KEYDOWN))
		{
			CInputHandler *ih = (CMainFrame::GetMainFrame())->GetInputHandler();
			const auto event = ih->Translate(*pMsg);
			if(ih->KeyEvent(kCtxViewPlugins, event) != kcNull)
				return true; // Mapped to a command, no need to pass message on.

			// Handle Application (menu) key
			if(pMsg->message == WM_KEYDOWN && event.key == VK_APPS)
			{
				CPoint pt{16, 16};
				if(auto selectedNode = m_selectedNode.lock(); selectedNode != nullptr)
				{
					pt = selectedNode->Position();
				}
				OnRButtonDown(0, pt);
			}
		}
	}

	return CModScrollView::PreTranslateMessage(pMsg);
}


LRESULT CViewPlugins::OnPlayerNotify(Notification * /*notify*/)
{
	for(auto &node : m_nodes)
	{
		node->Invalidate(false);
	}
	return 0;
}


LRESULT CViewPlugins::OnMidiMsg(WPARAM midiData, LPARAM)
{
	// Handle MIDI messages assigned to shortcuts
	CInputHandler *ih = CMainFrame::GetMainFrame()->GetInputHandler();
	ih->HandleMIDIMessage(kCtxViewPlugins, midiData) != kcNull
		|| ih->HandleMIDIMessage(kCtxAllContexts, midiData);
	return 1;
}


LRESULT CViewPlugins::OnCustomKeyMsg(WPARAM wParam, LPARAM /*lParam*/)
{
	switch(wParam)
	{
	case kcPluginGraphZoomIn:
	case kcPluginGraphZoomOut:
		m_zoom *= (wParam == kcPluginGraphZoomIn ? 1.1 : 0.9);
		UpdateScrollSize();
		Invalidate(FALSE);
		break;
	}

	return NULL;
}


LRESULT CViewPlugins::OnModViewMsg(WPARAM wParam, LPARAM lParam)
{
	switch(wParam)
	{
	case VIEWMSG_SETCURRENTPLUGIN:
		// TODO: Ensure that it's visible
		if(auto selectedNode = m_selectedNode.lock(); selectedNode != nullptr)
		{
			selectedNode->Invalidate();
		}
		for(auto &node : m_nodes)
		{
			PluginNode *plugNode = dynamic_cast<PluginNode *>(node.get());
			if(plugNode != nullptr)
			{
				if(plugNode->m_plug == static_cast<PLUGINDEX>(lParam))
				{
					m_selectedNode = node;
					plugNode->Invalidate();
					break;
				}
			}
		}
		break;

	case VIEWMSG_LOADSTATE:
		if(lParam)
		{
			auto &state = *reinterpret_cast<const PluginViewState *>(lParam);
			m_graphOffset = state.position;
			m_zoom = state.zoom;
			UpdateScrollSize();
		}
		break;

	case VIEWMSG_SAVESTATE:
		if(lParam)
		{
			auto &state = *reinterpret_cast<PluginViewState *>(lParam);
			state.zoom = m_zoom;
			state.position = m_graphOffset;
		}
		break;

	default:
		return CModScrollView::OnModViewMsg(wParam, lParam);
	}
	return 0;
}


void CViewPlugins::UpdateView(UpdateHint hint, CObject *pHint)
{
	if(pHint == this)
		return;

	if(hint.ToType<PluginHint>().GetType()[HINT_MODTYPE | HINT_MIXPLUGINS | HINT_PLUGINNAMES]
		|| hint.ToType<InstrumentHint>().GetType()[HINT_INSNAMES])
	{
		Invalidate(FALSE);
	}
}


BOOL CViewPlugins::OnScroll(UINT nScrollCode, UINT nPos, BOOL bDoScroll)
{
	if(bDoScroll)
	{
		for(auto &node : m_nodes)
		{
			node->InvalidateCache();
		}
	}
	return CModScrollView::OnScroll(nScrollCode, nPos, bDoScroll);
}


BOOL CViewPlugins::OnScrollBy(CSize sizeScroll, BOOL bDoScroll)
{
	if(bDoScroll)
	{
		for(auto &node : m_nodes)
		{
			node->InvalidateCache();
		}
	}
	return CModScrollView::OnScrollBy(sizeScroll, bDoScroll);
}


void CViewPlugins::UpdateScrollSize()
{
	CRectEx graph;
	for(const auto &node : m_nodes)
	{
		node->InvalidateCache();
		graph |= node->Rect();
	}
	CRect clientArea = m_rcClient;
	ScreenToGraph(clientArea);
	graph |= clientArea;
	graph.InflateRect(100, 100, 100, 100);
	graph.Scale(m_zoom);

	// Scroll sizes start at 0, so virtually shift our graph around.
	m_nScrollPosX -= m_graphOffset.x;
	m_nScrollPosY -= m_graphOffset.y;
	m_graphOffset = CPoint(-std::min<LONG>(graph.left, m_nScrollPosX), -std::min<LONG>(graph.top, m_nScrollPosY));
	m_nScrollPosX += m_graphOffset.x;
	m_nScrollPosY += m_graphOffset.y;
	SetScrollPos(SB_HORZ, m_nScrollPosX, FALSE);
	SetScrollPos(SB_VERT, m_nScrollPosY, FALSE);

	CSize sizeTotal, sizePage, sizeLine;
	sizeTotal = graph.Size();
	sizeTotal.cx += m_graphOffset.x;
	sizeTotal.cy += m_graphOffset.y;
	sizeLine = sizeTotal;
	sizePage = sizeLine;

	m_font.DeleteObject();
	m_fontBold.DeleteObject();
	LOGFONT lf;
	MemsetZero(lf);
	lf.lfHeight = mpt::saturate_round<int>(100 * m_zoom);
	lf.lfWeight = FW_NORMAL;
	mpt::String::WriteWinBuf(lf.lfFaceName) = _T("MS Shell Dlg");
	m_font.CreatePointFontIndirect(&lf, GetDC());

	lf.lfHeight = mpt::saturate_round<int>(90 * m_zoom);
	lf.lfWeight = FW_BOLD;
	m_fontBold.CreatePointFontIndirect(&lf, GetDC());

	m_linePen.DeleteObject();
	m_linePen.CreatePen(PS_SOLID, HighDPISupport::ScalePixels(1, m_hWnd), RGB(40, 40, 40));  // TODO
	m_activeLinePen.DeleteObject();
	m_activeLinePen.CreatePen(PS_SOLID, HighDPISupport::ScalePixels(2, m_hWnd), RGB(40, 40, 40));  // TODO

	SetScrollSizes(MM_TEXT, sizeTotal, sizePage, sizeLine);
}


void CViewPlugins::UpdateNodes()
{
	m_nodes.clear();
	m_nodes.push_back(std::make_shared<MasterNode>(*this));
	m_nodes.back()->Position(1040, 10);
	ModularNodePtr master = m_nodes.back();
	CSoundFile &sndFile = GetDocument()->GetSoundFile();
	int i = 0;
	for(PLUGINDEX p = 0; p < MAX_MIXPLUGINS; p++)
	{
		PLUGINDEX plug = MAX_MIXPLUGINS - p - 1;
		SNDMIXPLUGIN &plugin = sndFile.m_MixPlugins[plug];
		if(plugin.IsValidPlugin())
		{
			m_nodes.push_back(std::make_shared<PluginNode>(*this, *GetDocument(), plug));
			auto *node = static_cast<PluginNode *>(m_nodes.back().get());

			if(plugin.graphX != int32_min)
				node->Position(plugin.graphX, plugin.graphY);
			else
				node->Position(260 + (node->IsMasterEffect() ? 520 : (node->IsInstrumentPlugin() ? 0 : 260)), i * 60);
			i++;

			if(plugin.IsOutputToMaster())
				node->AddOutput(master);
			else
			{
				for(const auto &output : plugin.outputs)
				{
					if(auto plugNode = GetPluginNode(output.plugin); plugNode != nullptr)
					{
						node->AddOutput(plugNode);
					}
				}
			}
			if(plugin.IsMasterEffect())
				master = m_nodes.back();
		}
	}
	for(INSTRUMENTINDEX instr = 1; instr <= sndFile.GetNumInstruments(); instr++)
	{
		ModInstrument *ins = sndFile.Instruments[instr];
		if(ins != nullptr && ins->nMixPlug != 0)
		{
			m_nodes.push_back(std::make_shared<InstrumentNode>(*this, *GetDocument(), instr));
			m_nodes.back()->Position(0, i * 60);
			i--;
			if(auto plugNode = GetPluginNode(ins->nMixPlug - 1u); plugNode != nullptr)
			{
				m_nodes.back()->AddOutput(plugNode);
			}
		}
	}
}


CViewPlugins::PluginNodePtr CViewPlugins::GetPluginNode(PLUGINDEX plug) const
{
	for(const auto& node : m_nodes)
	{
		auto pluginNode = std::dynamic_pointer_cast<PluginNode>(node);
		if(pluginNode != nullptr && pluginNode->m_plug == plug)
			return pluginNode;
	}
	return nullptr;
}


CViewPlugins::ModularNodePtr CViewPlugins::GetNodeFromPoint(CPoint point) const
{
	if(auto selectedNode = m_selectedNode.lock(); selectedNode != nullptr && selectedNode->Rect().PtInRect(point))
	{
		return selectedNode;
	}
	for(auto i = m_nodes.rbegin(); i != m_nodes.rend(); i++)
	{
		if((**i).Rect().PtInRect(point))
		{
			return *i;
		}
	}
	return nullptr;
}


void CViewPlugins::ScreenToGraph(CPoint &point) const
{
	point += ScrollPos() - m_graphOffset;
	if(m_zoom != 1.0)
	{
		point.x = mpt::saturate_round<LONG>(point.x / m_zoom);
		point.y = mpt::saturate_round<LONG>(point.y / m_zoom);
	}
}


void CViewPlugins::ScreenToGraph(CRect &rect) const
{
	ScreenToGraph(rect.TopLeft());
	ScreenToGraph(rect.BottomRight());
}


void CViewPlugins::GraphToScreen(CPoint &point) const
{
	if(m_zoom != 1.0)
	{
		point.x = mpt::saturate_round<LONG>(point.x * m_zoom);
		point.y = mpt::saturate_round<LONG>(point.y * m_zoom);
	}
	point -= (ScrollPos() - m_graphOffset);
}


void CViewPlugins::GraphToScreen(CRect &rect) const
{
	GraphToScreen(rect.TopLeft());
	GraphToScreen(rect.BottomRight());
}


void CViewPlugins::OnLButtonDblClk(UINT nFlags, CPoint point)
{
	ScreenToGraph(point);
	ModularNodePtr node = GetNodeFromPoint(point);
	SelectNode(node);
	if(!(nFlags & (MK_CONTROL | MK_SHIFT)))
	{
		if(node != nullptr)
		{
			node->DoubleClick(point);
		} else
		{
#if 0
			CMainFrame::GetMainFrame()->OnPluginManager();
#else
			//Find empty plugin slot
			CSoundFile &sndFile = GetDocument()->GetSoundFile();
			PLUGINDEX plugSlot = PLUGINDEX_INVALID;
			for(PLUGINDEX plug = 0; plug < MAX_MIXPLUGINS; plug++)
			{
				if(sndFile.m_MixPlugins[plug].pMixPlugin == nullptr)
				{
					plugSlot = plug;
					break;
				}
			}
			if(plugSlot == PLUGINDEX_INVALID)
				return;

			CSelectPluginDlg dlg(GetDocument(), plugSlot, this);
			if(dlg.DoModal() == IDOK)
			{
				GetDocument()->UpdateAllViews(nullptr, PluginHint().Info().Names());
				UpdateNodes();
			}
#endif
		}
	}

}


void CViewPlugins::OnMouseMove(UINT nFlags, CPoint point)
{
	CModScrollView::OnMouseMove(nFlags, point);

	ScreenToGraph(point);
	if(nFlags & MK_LBUTTON)
	{
		if(auto selectedNode = m_selectedNode.lock(); selectedNode != nullptr)
		{
			CPoint newPos = selectedNode->Position() + (point - m_mouseDownPoint);
			selectedNode->Position(newPos);
			if(auto pluginNode = std::dynamic_pointer_cast<PluginNode>(selectedNode); pluginNode != nullptr)
			{
				CSoundFile &sndFile = GetDocument()->GetSoundFile();
				sndFile.m_MixPlugins[pluginNode->m_plug].graphX = newPos.x;
				sndFile.m_MixPlugins[pluginNode->m_plug].graphY = newPos.y;
			}
		} else
		{
			// Drag view
			CSize scroll(-point + m_mouseDownPoint);
			scroll.cx = mpt::saturate_round<int>(scroll.cx * m_zoom * 0.75);
			scroll.cy = mpt::saturate_round<int>(scroll.cy * m_zoom * 0.75);
			OnScrollBy(scroll);
		}
		m_mouseDragging = true;
		m_mouseDownPoint = point;
	}
}


void CViewPlugins::OnLButtonDown(UINT nFlags, CPoint point)
{
	CModScrollView::OnLButtonDown(nFlags, point);
	
	ScreenToGraph(point);
	m_mouseDownPoint = point;
	m_mouseDragging = false;

	ModularNodePtr node = GetNodeFromPoint(point);
	SelectNode(node);
	if(node != nullptr && !(nFlags & (MK_CONTROL | MK_SHIFT)))
	{
		//SendCtrlMessage(CTRLMSG_PLUG_SETCURRENT, 1);
	}
}


void CViewPlugins::OnLButtonUp(UINT nFlags, CPoint point)
{
	CModScrollView::OnLButtonUp(nFlags, point);
	m_mouseDragging = false;

	UpdateScrollSize();
}


void CViewPlugins::OnRButtonDown(UINT nFlags, CPoint point)
{
	CModScrollView::OnRButtonDown(nFlags, point);

	CPoint graphPoint = point;
	ScreenToGraph(graphPoint);
	m_mouseDownPoint = graphPoint;
	m_mouseDragging = false;

	ModularNodePtr node = GetNodeFromPoint(graphPoint);
	SelectNode(node);
	if(node != nullptr)
	{
		if(!(nFlags & (MK_CONTROL | MK_SHIFT)))
		{
			node->RightClick(graphPoint);
		}
	} else
	{
		//TODO: If no node is selected, show context menu with favourite (most recently used) plugins
		ClientToScreen(&point);
		CMenu menu;
		menu.CreatePopupMenu();
		menu.AppendMenu(MF_STRING | MF_GRAYED, 0, _T("Recent Plugins"));
		menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, point.x, point.y, this);
		menu.DestroyMenu();
	}
}


BOOL CViewPlugins::OnMouseWheel(UINT /*nFlags*/, short zDelta, CPoint pt)
{
	// Zoom into point
	ScreenToClient(&pt);
	const CPoint oldClientPt = pt;
	ScreenToGraph(pt);

	if(zDelta > 0)
		m_zoom *= 1.1;
	else
		m_zoom *= 0.909;
	if(m_zoom < 0.1)
		m_zoom = 0.1;
	UpdateScrollSize();

	// Translate the point back
	GraphToScreen(pt);
	pt -= oldClientPt;
	pt.x += m_nScrollPosX;
	pt.y += m_nScrollPosY;

	Limit(pt.x, 0, GetScrollLimit(SB_HORZ));
	Limit(pt.y, 0, GetScrollLimit(SB_VERT));
	SetScrollPos(SB_HORZ, pt.x);
	SetScrollPos(SB_VERT, pt.y);

	Invalidate(FALSE);
	return TRUE;
}


void CViewPlugins::OnPaint()
{
	CPaintDC dc(this);

	if(!m_offScreenDC.m_hDC)
	{
		m_offScreenDC.CreateCompatibleDC(&dc);
		m_offScreenBitmap.CreateCompatibleBitmap(&dc, m_rcClient.Width(), m_rcClient.Height());
	}

	CBitmap *oldBitmap = m_offScreenDC.SelectObject(&m_offScreenBitmap);

	const CRectEx &dirty = dc.m_ps.rcPaint;
	m_offScreenDC.FillSolidRect(&dirty, GetSysColor(COLOR_WINDOW));

	auto selectedNode = m_selectedNode.lock();
	for(auto &node : m_nodes)
	{
		if(node != selectedNode && (dirty.Intersects(node->ScreenRect()) || dirty.Intersects(node->ScreenRectLines())))
		{
			node->Render(m_offScreenDC);
		}
	}
	if(selectedNode != nullptr)
	{
		if(dirty.Intersects(selectedNode->ScreenRect()) || dirty.Intersects(selectedNode->ScreenRectLines()))
		{
			selectedNode->Render(m_offScreenDC);
		}
	}

	dc.BitBlt(dirty.left, dirty.top, dirty.Width(), dirty.Height(), &m_offScreenDC, dirty.left, dirty.top, SRCCOPY);

	m_offScreenDC.SelectObject(oldBitmap);
}


void CViewPlugins::OnSize(UINT nType, int cx, int cy)
{
	CModScrollView::OnSize(nType, cx, cy);

	m_offScreenDC.DeleteDC();
	m_offScreenBitmap.DeleteObject();
	GetClientRect(&m_rcClient);
	UpdateScrollSize();
	Invalidate(FALSE);
}


void CViewPlugins::SelectNode(ModularNodePtr node)
{
	if(auto selectedNode = m_selectedNode.lock(); selectedNode != nullptr)
	{
		selectedNode->Invalidate();
	}
	if(node != nullptr)
	{
		node->Invalidate();

		PluginNode *plugNode = dynamic_cast<PluginNode *>(node.get());
		if(plugNode != nullptr)
		{
			SendCtrlMessage(CTRLMSG_PLUG_SETCURRENT, plugNode->m_plug);
		}
	}
	m_selectedNode = node;
}

OPENMPT_NAMESPACE_END
