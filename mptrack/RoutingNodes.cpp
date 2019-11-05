#include "stdafx.h"
#include "RoutingNodes.h"
#include "Globals.h"
#include "Moddoc.h"
#include "TrackerSettings.h"
#include "View_plugins.h"
#include "../soundlib/plugins/PlugInterface.h"

OPENMPT_NAMESPACE_BEGIN


void ModularNode::Position(const CPoint &pos)
{
	Invalidate();
	InvalidateCache();
	m_position = pos;
	Invalidate();
}


void ModularNode::AddOutput(std::weak_ptr<ModularNode> output)
{
	if(auto node = output.lock())
		node->m_inputs.push_back(weak_from_this());
	m_outputs.push_back(std::move(output));
}


bool ModularNode::IsSelected() const
{
	return m_owner.m_selectedNode.lock().get() == this;
}


void ModularNode::Invalidate(bool invalidateLines)
{
	m_owner.InvalidateRect(ScreenRect(), FALSE);
	if(invalidateLines)
	{
		m_owner.InvalidateRect(ScreenRectLines(), FALSE);
		for(const auto &inNode : m_inputs)
		{
			if(auto node = inNode.lock())
				m_owner.InvalidateRect(node->ScreenRectLines(), FALSE);
		}
	}
}


void ModularNode::InvalidateCache()
{
	m_screenRect = {};
	m_screenRectLines = {};
	for(const auto &inNode : m_inputs)
	{
		if(auto node = inNode.lock())
			node->m_screenRectLines = {};
	}
}


CRect ModularNode::ScreenRect() const
{
	if(!m_screenRect.IsRectEmpty())
		return m_screenRect;

	m_screenRect = Rect();
	m_owner.GraphToScreen(m_screenRect);
	return m_screenRect;
}


CRect ModularNode::ScreenRectLines() const
{
	if(!m_screenRectLines.IsRectEmpty())
		return m_screenRectLines;

	m_screenRectLines = {};
	const auto inRect = ScreenRect();
	for(const auto &outNode : m_outputs)
	{
		if(auto node = outNode.lock())
		{
			const auto outRect = node->ScreenRect();
			const auto inY = inRect.top + inRect.Height() / 2, outY = outRect.top + outRect.Height() / 2;
			const auto inW = inRect.Width() / 5, outW = outRect.Width() / 5;
			CRect rect{std::min(inRect.right, outRect.left - outW), std::min(inY, outY), std::max(inRect.right + inW, outRect.left), std::max(inY, outY)};
			rect.InflateRect(2, 2);  // TODO how much is enough?
			m_screenRectLines |= rect;
		}
	}
	return m_screenRectLines;
}


void ModularNode::GraphToScreen(CRect &rect) const
{
	m_owner.GraphToScreen(rect);
}


double ModularNode::Zoom() const
{
	return m_owner.m_zoom;
}


CFont *ModularNode::Font() const
{
	return &m_owner.m_font;
}


CFont *ModularNode::BoldFont() const
{
	return &m_owner.m_fontBold;
}


CPen *ModularNode::LinePen() const
{
	return &m_owner.m_linePen;
}


CPen *ModularNode::ActiveLinePen() const
{
	return &m_owner.m_activeLinePen;
}


void ModularNode::DrawOutputs(CDC &dc) const
{
	auto *oldPen = dc.SelectObject(IsSelected() ? ActiveLinePen() : LinePen());
	const auto inRect = ScreenRect();
	for(const auto &outNode : m_outputs)
	{
		if(auto node = outNode.lock())
		{
			const auto outRect = node->ScreenRect();
			const auto inY = inRect.top + inRect.Height() / 2, outY = outRect.top + outRect.Height() / 2;
			const auto inW = inRect.Width() / 5, outW = outRect.Width() / 5;
			const POINT points[] =
			{
				{inRect.right, inY},
				{inRect.right + inW, inY},
				//{inRect.right + inW, inY < outY ? inRect.bottom : inRect.top },
				//(inRect | outRect).CenterPoint(),
				//{outRect.left - outW, inY < outY ? outRect.top : outRect.bottom },
				{outRect.left - outW, outY},
				{outRect.left, outY},
			};
			dc.PolyBezier(points, static_cast<int>(std::size(points)));
		}
	}
	dc.SelectObject(oldPen);
}


void ModularNode::CreateColorVariations(COLORREF col)
{
#define MIXCOLOR(factor) RGB(mpt::saturate_round<uint8>(factor * r), mpt::saturate_round<uint8>(factor * g), mpt::saturate_round<uint8>(factor * b))
	int r = GetRValue(col), g = GetGValue(col), b = GetBValue(col);

	m_brushCol[NORMAL] = col;
	m_penCol[NORMAL] = MIXCOLOR(0.5);

	m_brushCol[NORMAL | SELECTED] = MIXCOLOR(1.15);
	m_penCol[NORMAL | SELECTED] =  MIXCOLOR(0.575);

	m_brushCol[BYPASS] = MIXCOLOR(0.7);
	m_penCol[BYPASS] = MIXCOLOR(0.35);

	m_brushCol[BYPASS | SELECTED] = MIXCOLOR(0.805);
	m_penCol[BYPASS | SELECTED] = MIXCOLOR(0.4025);

#undef MIXCOLOR
}


PluginNode::PluginNode(CViewPlugins &owner, CModDoc &modDoc, PLUGINDEX plug)
    : ModularNode(owner), m_modDoc(modDoc), m_plug(plug), m_vuOutputs(2, 0)
{
	m_mutePenCol = RGB(0, 0, 0);
	m_muteBrushCol[0] = RGB(80, 255, 80);  // Unmuted
	m_muteBrushCol[1] = RGB(20, 120, 20);  // Muted
	m_muteBrushCol[2] = RGB(40, 200, 40);  // Auto-Suspended
}


void PluginNode::Render(CDC &dc)
{
	DrawOutputs(dc);

	const auto &mixPlug = m_modDoc.GetSoundFile().m_MixPlugins[m_plug];

	IMixPlugin *plugin = mixPlug.pMixPlugin;
	if(plugin != nullptr && plugin->IsInstrument())
		CreateColorVariations(RGB(170, 170, 200));
	else if(mixPlug.IsMasterEffect())
		CreateColorVariations(RGB(200, 200, 170));
	else
		CreateColorVariations(RGB(200, 170, 170));

	int flags = NORMAL;
	if(IsSelected())
		flags |= SELECTED;
	if(mixPlug.IsBypassed())
		flags |= BYPASS;
	dc.SetDCBrushColor(m_brushCol[flags]);
	dc.SetDCPenColor(m_penCol[flags]);
	auto *oldBrush = dc.SelectStockObject(DC_BRUSH);
	auto *oldPen = dc.SelectStockObject(DC_PEN);
	auto *oldFont = dc.SelectObject(BoldFont());

	CRect rect(ScreenRect());
	dc.Rectangle(rect);
	dc.SetBkMode(TRANSPARENT);
	dc.SetTextColor(RGB(0, 0, 0));

	rect.DeflateRect(1, 1, 1, 1);
	CString s;
	s.Format(_T("FX%u"), m_plug + 1);
	dc.DrawText(s, rect, DT_SINGLELINE);
	dc.SelectObject(Font());

	const CSize size = Size();
	const CPoint position = Position();
	rect.SetRect(position, CPoint(position.x + size.cx, position.y + 32));
	GraphToScreen(rect);
	dc.DrawText(mpt::ToCString(m_modDoc.GetSoundFile().GetCharsetInternal(), mixPlug.Info.szName), rect, DT_SINGLELINE | DT_CENTER | DT_BOTTOM | DT_END_ELLIPSIS);

	if(plugin != nullptr)
	{
		CRect vuRect(position.x + 10, position.y + 36, position.x + 190, position.y + 40);
		GraphToScreen(vuRect);
		auto numSamples = plugin->GetOutputVUMeters(m_vuOutputs);
		m_vuMeters.resize(m_vuOutputs.size());
		const auto decay = TrackerSettings::Instance().VuMeterDecaySpeedDecibelPerSecond.Get();  // TODO
		for(size_t i = 0; i < m_vuOutputs.size(); i++)
		{
			if(numSamples)
			{
				m_vuMeters[i].SetDecaySpeedDecibelPerSecond(decay);
				m_vuMeters[i].Process(mpt::audio_span_interleaved<const MixSampleFloat>(&m_vuOutputs[i], 1, 1));
			}
			VUMeterUI vu;
			vu.SetOrientation(true, false);
			dc.FillSolidRect(vuRect, RGB(0, 0, 0));
			vu.Draw(dc, vuRect, m_vuMeters[i][0].PeakToUIValue(), true);
			vuRect.OffsetRect(0, mpt::saturate_round<int>(6 * Zoom())); // TODO when zooming out very far, this sometimes overflows the node size!
			if(numSamples)
				m_vuMeters[i].Decay(numSamples, m_modDoc.GetSoundFile().GetSampleRate());
		}
	}

	int muteStatus = (plugin != nullptr && !mixPlug.IsBypassed()) ? 0 : 1;
	if(plugin != nullptr && (plugin->m_MixState.dwFlags & SNDMIXPLUGINSTATE::psfSilenceBypass))
	{
		muteStatus = 2;
	}

	CRect circle(position, Size());
	circle.SetRect(circle.right - 9, circle.top + 2, circle.right - 2, circle.top + 9);
	GraphToScreen(circle);
	dc.SetDCBrushColor(m_muteBrushCol[muteStatus]);
	dc.SetDCPenColor(m_mutePenCol);
	dc.Ellipse(circle);

	dc.SelectObject(oldFont);
	dc.SelectObject(oldPen);
	dc.SelectObject(oldBrush);
}


CSize PluginNode::Size() const
{
	int numChannels = 0;
	const IMixPlugin *plugin = m_modDoc.GetSoundFile().m_MixPlugins[m_plug].pMixPlugin;
	if(plugin != 0)
	{
		numChannels = plugin->GetNumOutputChannels();
	}
	return CSize(200, 40 + numChannels * 6);
}


void PluginNode::DoubleClick(CPoint /*pt*/)
{
	m_modDoc.TogglePluginEditor(m_plug);
}


void PluginNode::RightClick(CPoint /*pt*/)
{
	// TODO context menu (e.g. bypass?)
	// Left-Click on green LED for toggling?
}


bool PluginNode::IsInstrumentPlugin() const
{
	const IMixPlugin *plugin = m_modDoc.GetSoundFile().m_MixPlugins[m_plug].pMixPlugin;
	return plugin != nullptr && plugin->IsInstrument();
}


bool PluginNode::IsMasterEffect() const
{
	return m_modDoc.GetSoundFile().m_MixPlugins[m_plug].IsMasterEffect();
}


InstrumentNode::InstrumentNode(CViewPlugins &owner, CModDoc &modDoc, INSTRUMENTINDEX instr)
    : ModularNode(owner), m_modDoc(modDoc), m_instr(instr)
{
	CreateColorVariations(RGB(170, 200, 170));
}


void InstrumentNode::Render(CDC &dc)
{
	DrawOutputs(dc);

	const ModInstrument *ins = m_modDoc.GetSoundFile().Instruments[m_instr];
	int flags = NORMAL;
	if(IsSelected())
		flags |= SELECTED;
	if(ins->dwFlags[INS_MUTE])
		flags |= BYPASS;
	dc.SetDCBrushColor(m_brushCol[flags]);
	dc.SetDCPenColor(m_penCol[flags]);
	auto *oldBrush = dc.SelectStockObject(DC_BRUSH);
	auto *oldPen = dc.SelectStockObject(DC_PEN);
	auto *oldFont = dc.SelectObject(BoldFont());

	CRect rect(ScreenRect());
	dc.Rectangle(rect);
	dc.SetBkMode(TRANSPARENT);
	dc.SetTextColor(RGB(0, 0, 0));

	rect.DeflateRect(1, 1, 1, 1);
	TCHAR s[32];
	wsprintf(s, _T("Instrument %u"), m_instr);
	dc.DrawText(s, rect, DT_SINGLELINE);
	dc.SelectObject(Font());
	if(ins != nullptr)
	{
		dc.DrawText(mpt::ToCString(m_modDoc.GetSoundFile().GetCharsetInternal(), ins->name), rect, DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_END_ELLIPSIS);
	}

	dc.SelectObject(oldFont);
	dc.SelectObject(oldPen);
	dc.SelectObject(oldBrush);
}


void InstrumentNode::DoubleClick(CPoint /*pt*/)
{
	m_modDoc.ViewInstrument(m_instr);
}


MasterNode::MasterNode(CViewPlugins &owner)
    : ModularNode(owner)
{
	CreateColorVariations(RGB(200, 200, 170));
}


void MasterNode::Render(CDC &dc)
{
	int flags = NORMAL;
	if(IsSelected()) flags |= SELECTED;
	dc.SetDCBrushColor(m_brushCol[flags]);
	dc.SetDCPenColor(m_penCol[flags]);
	auto *oldBrush = dc.SelectStockObject(DC_BRUSH);
	auto *oldPen = dc.SelectStockObject(DC_PEN);
	auto *oldFont = dc.SelectObject(BoldFont());

	CRect rect(ScreenRect());
	dc.Rectangle(rect);
	dc.SetBkMode(TRANSPARENT);
	dc.SetTextColor(RGB(0, 0, 0));
	dc.DrawText(_T("Master"), rect, DT_SINGLELINE | DT_CENTER | DT_VCENTER);

	dc.SelectObject(oldFont);
	dc.SelectObject(oldPen);
	dc.SelectObject(oldBrush);
}

OPENMPT_NAMESPACE_END
