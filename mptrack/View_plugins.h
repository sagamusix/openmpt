// TODO add header

#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "Globals.h"

OPENMPT_NAMESPACE_BEGIN

class ModularNode;
class PluginNode;

class CRectEx : public CRect
{
public:
	CRectEx() = default;
	CRectEx(const RECT &other) : CRect{other} { }
	CRectEx(const CPoint pt, const CSize sz) : CRect{pt, sz} { }
	CRectEx(int l, int t, int r, int b) : CRect{l, t, r, b} { }

	void Scale(double scale)
	{
		if(scale != 1.0)
		{
			left = mpt::saturate_round<LONG>(left * scale);
			top = mpt::saturate_round<LONG>(top * scale);
			right = mpt::saturate_round<LONG>(right * scale);
			bottom = mpt::saturate_round<LONG>(bottom * scale);
		}
	}

	bool Intersects(RECT other) const
	{
		return ::IntersectRect(&other, this, &other) != FALSE;
	}
};

class CViewPlugins : public CModScrollView
{
	friend class ModularNode;
protected:
	using ModularNodePtr = std::shared_ptr<ModularNode>;
	using PluginNodePtr = std::shared_ptr<PluginNode>;

	std::vector<ModularNodePtr> m_nodes;
	std::weak_ptr<ModularNode> m_selectedNode;

	CDC m_offScreenDC;
	CBitmap m_offScreenBitmap;
	CRect m_rcClient;
	CPoint m_mouseDownPoint;
	CPoint m_graphOffset;
	CFont m_font, m_fontBold;
	CPen m_linePen, m_activeLinePen;

	double m_zoom = 1.0;
	bool m_mouseDragging = false;

public:
	CViewPlugins() = default;
	~CViewPlugins();

	void OnInitialUpdate() override;
	BOOL PreTranslateMessage(MSG *pMsg) override;
	LRESULT OnPlayerNotify(Notification *notify) override;
	LRESULT OnModViewMsg(WPARAM, LPARAM) override;
	void UpdateView(UpdateHint hint, CObject *pHint = nullptr) override;

	BOOL OnScroll(UINT nScrollCode, UINT nPos, BOOL bDoScroll = TRUE) override;
	BOOL OnScrollBy(CSize sizeScroll, BOOL bDoScroll = TRUE) override;

	void OnPaint();
	void OnSize(UINT nType, int cx, int cy);
	LRESULT OnMidiMsg(WPARAM midiData, LPARAM);
	LRESULT OnCustomKeyMsg(WPARAM, LPARAM);

	DECLARE_SERIAL(CViewPlugins)

protected:
	void UpdateScrollSize();
	void UpdateNodes();

	void SelectNode(ModularNodePtr node);

	PluginNodePtr GetPluginNode(PLUGINDEX plug) const;
	ModularNodePtr GetNodeFromPoint(CPoint point) const;
	void ScreenToGraph(CPoint &point) const;
	void ScreenToGraph(CRect &rect) const;
	void GraphToScreen(CPoint &point) const;
	void GraphToScreen(CRect &rect) const;
	CPoint ScrollPos() const { return CPoint(m_nScrollPosX, m_nScrollPosY); }

	afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
	afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);

	DECLARE_MESSAGE_MAP()
};

OPENMPT_NAMESPACE_END
