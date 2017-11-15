#include "stdafx.h"
#include "GlobalsTransaction.h"
#include "Moddoc.h"
#include "Networking.h"
#include "NetworkTypes.h"

OPENMPT_NAMESPACE_BEGIN

GlobalSettingsTransaction::GlobalSettingsTransaction(CSoundFile &sndFile)
	: m_sndFile(sndFile)
	, m_state(mpt::make_unique<Networking::GlobalEditMsg>())
{
	m_state->name = sndFile.m_songName;
	m_state->artist = mpt::ToCharset(mpt::CharsetUTF8, sndFile.m_songArtist);
	m_state->type = sndFile.m_nType;
	m_state->tempo = sndFile.m_nDefaultTempo;
	m_state->speed = sndFile.m_nDefaultSpeed;
	m_state->globalVol = sndFile.m_nDefaultGlobalVolume;
	m_state->sampleVol = sndFile.m_nSamplePreAmp;
	m_state->pluginVol = sndFile.m_nVSTiVolume;
	m_state->resampling = sndFile.m_nResampling;
	m_state->mixLevels = sndFile.GetMixLevels();
	m_state->flags = sndFile.m_SongFlags;
	m_state->tempoMode = sndFile.m_nTempoMode;
	m_state->rpb = sndFile.m_nDefaultRowsPerBeat;
	m_state->rpm = sndFile.m_nDefaultRowsPerMeasure;
	m_state->swing = sndFile.m_tempoSwing;
	m_state->playBehaviour = sndFile.m_playBehaviour;
}

GlobalSettingsTransaction::~GlobalSettingsTransaction()
{
	Networking::GlobalEditMsg msg;
	if(m_state->name != m_sndFile.m_songName)
		msg.name = m_sndFile.m_songName;
	std::string artist = mpt::ToCharset(mpt::CharsetUTF8, m_sndFile.m_songArtist);
	if(m_state->artist != artist)
		msg.artist = artist;
	if(m_state->type != m_sndFile.m_nType)
		msg.type = m_sndFile.m_nType;
	if(m_state->tempo != m_sndFile.m_nDefaultTempo)
		msg.tempo = m_sndFile.m_nDefaultTempo;
	if(m_state->speed != m_sndFile.m_nDefaultSpeed)
		msg.speed = m_sndFile.m_nDefaultSpeed;
	if(m_state->globalVol != m_sndFile.m_nDefaultGlobalVolume)
		msg.globalVol = m_sndFile.m_nDefaultGlobalVolume;
	if(m_state->sampleVol != m_sndFile.m_nSamplePreAmp)
		msg.sampleVol = m_sndFile.m_nSamplePreAmp;
	if(m_state->pluginVol != m_sndFile.m_nVSTiVolume)
		msg.pluginVol = m_sndFile.m_nVSTiVolume;
	if(m_state->resampling != static_cast<uint32>(m_sndFile.m_nResampling))
		msg.resampling = m_sndFile.m_nResampling;
	if(m_state->mixLevels != static_cast<uint32>(m_sndFile.GetMixLevels()))
		msg.mixLevels = m_sndFile.GetMixLevels();
	if(m_state->flags != m_sndFile.m_SongFlags)
		msg.flags = m_sndFile.m_SongFlags;
	if(m_state->tempoMode != static_cast<uint32>(m_sndFile.m_nTempoMode))
		msg.tempoMode = m_sndFile.m_nTempoMode;
	if(m_state->rpb != m_sndFile.m_nDefaultRowsPerBeat)
		msg.rpb = m_sndFile.m_nDefaultRowsPerBeat;
	if(m_state->rpm != m_sndFile.m_nDefaultRowsPerMeasure)
		msg.rpm = m_sndFile.m_nDefaultRowsPerMeasure;
	if(m_state->swing != m_sndFile.m_tempoSwing)
		msg.swing = m_sndFile.m_tempoSwing;
	if(m_state->playBehaviour != m_sndFile.m_playBehaviour)
		msg.playBehaviour = m_sndFile.m_playBehaviour;

	auto *modDoc = m_sndFile.GetpModDoc();
	if(modDoc->m_collabClient)
	{
		std::ostringstream ss;
		cereal::BinaryOutputArchive ar(ss);
		ar(Networking::GlobalSettingsMsg, msg);
		modDoc->m_collabClient->Write(ss.str());
	}
}


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
			ar(Networking::ChannelSettingsMsg, m_channel, m_sourceChannel, settings);
			modDoc->m_collabClient->Write(ss.str());
		}
	}
}

OPENMPT_NAMESPACE_END
