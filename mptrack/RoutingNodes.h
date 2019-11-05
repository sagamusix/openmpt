// TODO header

#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "VUMeter.h"

#include <memory>

OPENMPT_NAMESPACE_BEGIN

class CViewPlugins;
class CModDoc;

class ModularNode : public std::enable_shared_from_this<ModularNode>
{
protected:
	CViewPlugins &m_owner;

	enum BrushFlags
	{
		NORMAL		= 0x00,
		SELECTED	= 0x01,
		BYPASS		= 0x02,
	};

	std::array<COLORREF, 4> m_brushCol= {{}};
	std::array<COLORREF, 4> m_penCol = {{}};

private:
	CPoint m_position;
	mutable CRect m_screenRect, m_screenRectLines;
	std::vector<std::weak_ptr<ModularNode>> m_inputs, m_outputs;

public:
	ModularNode(CViewPlugins &owner) : m_owner{owner} { }
	virtual ~ModularNode() { }

	virtual void Render(CDC &dc) = 0;
	virtual CSize Size() const = 0;
	virtual void DoubleClick(CPoint /*pt*/) { }
	virtual void RightClick(CPoint /*pt*/) { }

	// Position in modular routing graph
	CPoint Position() const { return m_position; }
	void Position(const CPoint &pos);
	void Position(int x, int y) { Position(CPoint(x, y)); }

	void AddOutput(std::weak_ptr<ModularNode> output);

	bool IsSelected() const;

	// Position and size in modular routing graph
	CRect Rect() const { return CRect(m_position, Size()); }
	// Position and size in modular routing graph, including the scroll position of the owner view
	CRect ScreenRect() const;
	CRect ScreenRectLines() const;

	// Schedule this node for redrawing
	void Invalidate(bool invalidateLines = true);
	// Invalidate the geometry cache of this node
	void InvalidateCache();

	void DrawOutputs(CDC &dc) const;

protected:
	void GraphToScreen(CRect &rect) const;
	double Zoom() const;
	CFont *Font() const;
	CFont *BoldFont() const;
	CPen *LinePen() const;
	CPen* ActiveLinePen() const;

	void CreateColorVariations(COLORREF col);
};

class PluginNode : public ModularNode
{
protected:
	std::array<COLORREF, 3> m_muteBrushCol = {{}};
	COLORREF m_mutePenCol = {};

	using VUMeterPlugin = VUMeter<1>;
	std::vector<VUMeterPlugin> m_vuMeters;
	std::vector<float> m_vuOutputs;
public:
	CModDoc &m_modDoc;
	const PLUGINDEX m_plug;

public:
	PluginNode(CViewPlugins &owner, CModDoc &modDoc, PLUGINDEX plug);

	void Render(CDC &dc) override;
	CSize Size() const override;
	void DoubleClick(CPoint pt) override;
	void RightClick(CPoint pt) override;

	bool IsInstrumentPlugin() const;
	bool IsMasterEffect() const;
};

class InstrumentNode : public ModularNode
{
protected:
	CModDoc &m_modDoc;
	const INSTRUMENTINDEX m_instr;

public:
	InstrumentNode(CViewPlugins &owner, CModDoc &modDoc, INSTRUMENTINDEX instr);

	void Render(CDC &dc) override;
	CSize Size() const override { return CSize(200, 50); }
	void DoubleClick(CPoint pt) override;
};

class MasterNode : public ModularNode
{
public:
	MasterNode(CViewPlugins &owner);

	void Render(CDC &dc) override;
	CSize Size() const override { return CSize(80, 50); }
};

OPENMPT_NAMESPACE_END
