#include "stdafx.h"
#include "Moddoc.h"
#include "../soundlib/tuning.h"
#include "../soundlib/tuningcollection.h"
#include "Networking.h"
#include "InstrumentTransaction.h"
#include "NetworkTypes.h"

OPENMPT_NAMESPACE_BEGIN

InstrumentTransaction::InstrumentTransaction(const CSoundFile &sndFile, INSTRUMENTINDEX instr)
	: m_sndFile(sndFile)
	, m_origInstr(sndFile.Instruments[instr] != nullptr ? *sndFile.Instruments[instr] : ModInstrument(0))
	, m_instr(instr)
	, m_instrExisted(sndFile.Instruments[instr] != nullptr)
{
}


InstrumentTransaction::~InstrumentTransaction()
{
	const ModInstrument *instr = m_sndFile.Instruments[m_instr];
	bool modified = (m_instrExisted != (instr != nullptr)), volEnv = modified, panEnv = modified, pitchEnv = modified;
	if(!modified)
	{
		modified =
			   instr->dwFlags != m_origInstr.dwFlags
			|| instr->nFadeOut != m_origInstr.nFadeOut
			|| instr->nGlobalVol != m_origInstr.nGlobalVol
			|| instr->nPan != m_origInstr.nPan
			|| instr->nVolRampUp != m_origInstr.nVolRampUp
			|| instr->wMidiBank != m_origInstr.wMidiBank
			|| instr->nMidiProgram != m_origInstr.nMidiProgram
			|| instr->nMidiChannel != m_origInstr.nMidiChannel
			|| instr->nMidiDrumKey != m_origInstr.nMidiDrumKey
			|| instr->midiPWD != m_origInstr.midiPWD
			|| instr->nNNA != m_origInstr.nNNA
			|| instr->nDCT != m_origInstr.nDCT
			|| instr->nDNA != m_origInstr.nDNA
			|| instr->nPanSwing != m_origInstr.nPanSwing
			|| instr->nVolSwing != m_origInstr.nVolSwing
			|| instr->nIFC != m_origInstr.nIFC
			|| instr->nIFR != m_origInstr.nIFR
			|| instr->nPPS != m_origInstr.nPPS
			|| instr->nPPC != m_origInstr.nPPC
			|| instr->nMixPlug != m_origInstr.nMixPlug
			|| instr->nCutSwing != m_origInstr.nCutSwing
			|| instr->nResSwing != m_origInstr.nResSwing
			|| instr->nFilterMode != m_origInstr.nFilterMode
			|| instr->nPluginVelocityHandling != m_origInstr.nPluginVelocityHandling
			|| instr->nPluginVolumeHandling != m_origInstr.nPluginVolumeHandling
			|| instr->pitchToTempoLock != m_origInstr.pitchToTempoLock
			|| instr->nResampling != m_origInstr.nResampling
			|| instr->pTuning != m_origInstr.pTuning
			|| memcmp(instr->NoteMap, m_origInstr.NoteMap, sizeof(instr->NoteMap))
			|| memcmp(instr->Keyboard, m_origInstr.Keyboard, sizeof(instr->Keyboard))
			|| strcmp(instr->name, m_origInstr.name)
			|| strcmp(instr->filename, m_origInstr.filename);
		volEnv = instr->VolEnv != m_origInstr.VolEnv;
		panEnv = instr->PanEnv != m_origInstr.PanEnv;
		pitchEnv = instr->PitchEnv != m_origInstr.PitchEnv;
	}

	if(modified || volEnv || panEnv || pitchEnv)
	{
		auto *modDoc = m_sndFile.GetpModDoc();
		if(modDoc->m_collabClient)
		{
			std::ostringstream ss;
			cereal::BinaryOutputArchive ar(ss);
			if(!modified)
			{
				if(volEnv)
				{
					ar(Networking::EnvelopeTransactionMsg, m_instr, ENV_VOLUME, instr->VolEnv);
				}
				if(panEnv)
				{
					ar(Networking::EnvelopeTransactionMsg, m_instr, ENV_PANNING, instr->PanEnv);
				}
				if(pitchEnv)
				{
					ar(Networking::EnvelopeTransactionMsg, m_instr, ENV_PITCH, instr->PitchEnv);
				}
			} else
			{
				uint32 tuningID = 0;
				if(instr->pTuning)
				{
					for(const auto &tuning : m_sndFile.GetTuneSpecificTunings())
					{
						tuningID++;
						if(tuning.get() == instr->pTuning)
						{
							break;
						}
					}
				}

				Networking::InstrumentEditMsg msg{ m_instr, *instr, tuningID };
				ar(Networking::InstrumentTransactionMsg, msg);
			}
			modDoc->m_collabClient->Write(ss.str());
		}
	}
}


void InstrumentTransaction::SendTunings(const CSoundFile &sndFile)
{
	auto *modDoc = sndFile.GetpModDoc();
	if(modDoc->m_collabClient)
	{
		std::ostringstream ss;
		cereal::BinaryOutputArchive ar(ss);
		ar(Networking::TuningTransactionMsg, Networking::SerializeTunings(sndFile));
		modDoc->m_collabClient->Write(ss.str());
	}
}

OPENMPT_NAMESPACE_END
