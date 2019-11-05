#include "stdafx.h"
#include "VUMeter.h"
#include "Notification.h"
#include "Mainfrm.h"

OPENMPT_NAMESPACE_BEGIN

// Draw a single VU Meter. vu ranges from 0 to 65536.
void VUMeterUI::Draw(CDC &dc, const CRect &rect, int32 vu, bool redraw)
{
	if(CMainFrame::GetMainFrame()->GetSoundFilePlaying() == nullptr)
	{
		vu = 0;
	}

	const bool clip = (vu & Notification::ClipVU) != 0;
	vu = (vu & (~Notification::ClipVU)) >> 8;

	if(horizontal)
	{
		const int cx = std::max(1, rect.Width());
		int v = (vu * cx) >> 8;

		for(int x = 0; x <= cx; x += 2)
		{
			int pen = Clamp((x * NUM_VUMETER_PENS) / cx, 0, NUM_VUMETER_PENS - 1);
			const bool last = (x == (cx & ~0x1));

			// Darken everything above volume, unless it's the clip indicator
			if(v <= x && (!last || !clip))
				pen += NUM_VUMETER_PENS;

			bool draw = redraw || (v < lastValue && v <= x && x <= lastValue) || (lastValue < v && lastValue <= x && x <= v);
			draw = draw || (last && clip != lastClip);
			if(draw) dc.FillSolidRect(
				((!rtl) ? (rect.left + x) : (rect.right - x)),
				rect.top, 1, rect.Height(), CMainFrame::gcolrefVuMeter[pen]);
			if(last) lastClip = clip;
		}
		lastValue = v;
	} else
	{
		const int cy = std::max(1, rect.Height());
		int v = (vu * cy) >> 8;

		for(int ry = rect.bottom - 1; ry > rect.top; ry -= 2)
		{
			const int y0 = rect.bottom - ry;
			int pen = Clamp((y0 * NUM_VUMETER_PENS) / cy, 0, NUM_VUMETER_PENS - 1);
			const bool last = (ry == rect.top + 1);

			// Darken everything above volume, unless it's the clip indicator
			if(v <= y0 && (!last || !clip))
				pen += NUM_VUMETER_PENS;

			bool draw = redraw || (v < lastValue && v <= ry && ry <= lastValue) || (lastValue < v && lastValue <= ry && ry <= v);
			draw = draw || (last && clip != lastClip);
			if(draw) dc.FillSolidRect(rect.left, ry, rect.Width(), 1, CMainFrame::gcolrefVuMeter[pen]);
			if(last) lastClip = clip;
		}
		lastValue = v;
	}
}

OPENMPT_NAMESPACE_END