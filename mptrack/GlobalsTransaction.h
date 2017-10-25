#pragma once

#include "../soundlib/Snd_defs.h"
#include "../soundlib/Mixer.h"
#include "../soundlib/ModChannel.h"

OPENMPT_NAMESPACE_BEGIN

class CSoundFile;

class ChannelSettingsTransaction
{
	CSoundFile &m_sndFile;
	const ModChannelSettings m_settings;
	CHANNELINDEX m_channel, m_sourceChannel;

public:
	ChannelSettingsTransaction(CSoundFile &sndFile, CHANNELINDEX chn, CHANNELINDEX sourceChn = CHANNELINDEX_INVALID);
	~ChannelSettingsTransaction();
};

OPENMPT_NAMESPACE_END

