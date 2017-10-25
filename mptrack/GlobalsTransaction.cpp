#include "stdafx.h"
#include "GlobalsTransaction.h"
#include "Moddoc.h"
#include "Networking.h"
#include "NetworkTypes.h"

OPENMPT_NAMESPACE_BEGIN

ChannelSettingsTransaction::ChannelSettingsTransaction(CSoundFile &sndFile, CHANNELINDEX chn, CHANNELINDEX sourceChn)
	: m_sndFile(sndFile)
	, m_settings(sndFile.ChnSettings[chn])
	, m_channel(chn)
	, m_sourceChannel(sourceChn)
{ }

ChannelSettingsTransaction::~ChannelSettingsTransaction()
{
	const ModChannelSettings &settings = m_sndFile.ChnSettings[m_channel];
	if((settings.dwFlags & ~CHN_MUTE) != (m_settings.dwFlags & ~CHN_MUTE)
		|| settings.nMixPlugin != m_settings.nMixPlugin
		|| settings.nPan != m_settings.nPan
		|| settings.nVolume != m_settings.nVolume
		|| strcmp(settings.szName, m_settings.szName))
	{
		auto *modDoc = m_sndFile.GetpModDoc();
		if(modDoc->m_collabClient)
		{
			std::ostringstream ss;
			cereal::BinaryOutputArchive ar(ss);
			ar(Networking::ChannelSettingsMsg);
			ar(m_channel);
			ar(m_sourceChannel);
			ar(settings);
			modDoc->m_collabClient->Write(ss.str());
		}
	}
}

OPENMPT_NAMESPACE_END
