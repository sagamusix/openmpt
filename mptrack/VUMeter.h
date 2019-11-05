// TODO add header

#pragma once

#include "openmpt/all/BuildSettings.hpp"

#include "../soundlib/Sndfile.h"
#include "../soundlib/Mixer.h"
#include "../src/openmpt/soundbase/SampleConvertFixedPoint.hpp"

OPENMPT_NAMESPACE_BEGIN

// Class for drawing generic VU meter in the UI

class VUMeterUI
{
public:
	int32 lastValue = 0;
	bool lastClip = false;
	bool horizontal = true;
	bool rtl = false;

public:
	VUMeterUI() = default;

	void Draw(CDC &dc, const CRect &rect, int32 vu, bool redraw);

	void SetOrientation(bool horz, bool rightToLeft)
	{
		horizontal = horz;
		rtl = rightToLeft;
	}
};

// Class for doing VU meter calculations

template<size_t maxChn>
class VUMeter
	: public IMonitorInput
	, public IMonitorOutput
{
public:
	static constexpr std::size_t maxChannels = maxChn;
	struct Channel
	{
		int32 peak = 0;
		bool clipped = false;

		int32 PeakToUIValue() const
		{
			return Clamp(peak >> 11, 0, 0x10000);
		}
	};
private:
	Channel channels[maxChannels];
	int32 decayParam = 0;
public:
	VUMeter() { SetDecaySpeedDecibelPerSecond(88.0f); }
public:
	const Channel & operator [] (std::size_t channel) const { return channels[channel]; }

	void Process(Channel &c, MixSampleInt sample)
	{
		c.peak = std::max(c.peak, std::abs(sample));
		if(sample < MixSampleIntTraits::mix_clip_min || MixSampleIntTraits::mix_clip_max < sample)
		{
			c.clipped = true;
		}
	}

	void Process(Channel &c, MixSampleFloat sample)
	{
		Process(c, SC::ConvertToFixedPoint<MixSampleInt, MixSampleFloat, MixSampleIntTraits::mix_fractional_bits>()(sample));
	}

	void Process(mpt::audio_span_interleaved<const MixSampleInt> buffer)
	{
		for(std::size_t frame = 0; frame < buffer.size_frames(); ++frame)
		{
			for(std::size_t channel = 0; channel < std::min(buffer.size_channels(), maxChannels); ++channel)
			{
				Process(channels[channel], buffer(channel, frame));
			}
		}
		for(std::size_t channel = std::min(buffer.size_channels(), maxChannels); channel < maxChannels; ++channel)
		{
			channels[channel] = Channel();
		}
	}

	void Process(mpt::audio_span_interleaved<const MixSampleFloat> buffer)
	{
		for(std::size_t frame = 0; frame < buffer.size_frames(); ++frame)
		{
			for(std::size_t channel = 0; channel < std::min(buffer.size_channels(), maxChannels); ++channel)
			{
				Process(channels[channel], buffer(channel, frame));
			}
		}
		for(std::size_t channel = std::min(buffer.size_channels(), maxChannels); channel < maxChannels; ++channel)
		{
			channels[channel] = Channel();
		}
	}

	void Process(mpt::audio_span_planar<const MixSampleInt> buffer)
	{
		for(std::size_t frame = 0; frame < buffer.size_frames(); ++frame)
		{
			for(std::size_t channel = 0; channel < std::min(buffer.size_channels(), maxChannels); ++channel)
			{
				Process(channels[channel], buffer(channel, frame));
			}
		}
		for(std::size_t channel = std::min(buffer.size_channels(), maxChannels); channel < maxChannels; ++channel)
		{
			channels[channel] = Channel();
		}
	}

	void Process(mpt::audio_span_planar<const MixSampleFloat> buffer)
	{
		for(std::size_t frame = 0; frame < buffer.size_frames(); ++frame)
		{
			for(std::size_t channel = 0; channel < std::min(buffer.size_channels(), maxChannels); ++channel)
			{
				Process(channels[channel], buffer(channel, frame));
			}
		}
		for(std::size_t channel = std::min(buffer.size_channels(), maxChannels); channel < maxChannels; ++channel)
		{
			channels[channel] = Channel();
		}
	}

	void Decay(int32 secondsNum, int32 secondsDen)
	{
		int32 decay = Util::muldivr(decayParam, secondsNum, secondsDen);
		for(auto &chn : channels)
		{
			chn.peak = std::max(chn.peak - decay, 0);
		}
	}

	void ResetClipped()
	{
		for(auto &chn : channels)
		{
			chn.clipped = false;
		}
	}

	void SetDecaySpeedDecibelPerSecond(float decibelPerSecond)
	{
		const float dynamicRange = 48.0f; // corresponds to the current implementation of the UI widget displaying the result
		float linearDecayRate = decibelPerSecond / dynamicRange;
		decayParam = mpt::saturate_round<int32>(linearDecayRate * static_cast<float>(MixSampleIntTraits::mix_clip_max));
	}
};


// Default VU meter type for OpenMPT's mix buffer
using VUMeterMix = VUMeter<4>;

OPENMPT_NAMESPACE_END
