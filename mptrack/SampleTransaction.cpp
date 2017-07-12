#include "stdafx.h"
#include "Moddoc.h"
#include "Networking.h"
#include "SampleTransaction.h"
#include "NetworkTypes.h"

OPENMPT_NAMESPACE_BEGIN

SamplePropertyTransaction::SamplePropertyTransaction(CSoundFile &sndFile, SAMPLEINDEX sample)
	: m_sndFile(sndFile)
	, m_origSample(sndFile.GetSample(sample))
	, m_origName(sndFile.m_szNames[sample])
	, m_sample(sample)
{
}


SamplePropertyTransaction::~SamplePropertyTransaction()
{
	const ModSample &sample = m_sndFile.GetSample(m_sample);

	bool modified =
		   sample.nLength != m_origSample.nLength
		|| sample.nLoopStart != m_origSample.nLoopStart
		|| sample.nLoopEnd != m_origSample.nLoopEnd
		|| sample.nSustainStart != m_origSample.nSustainStart
		|| sample.nSustainEnd != m_origSample.nSustainEnd
		|| sample.nC5Speed != m_origSample.nC5Speed
		|| sample.nPan != m_origSample.nPan
		|| sample.nVolume != m_origSample.nVolume
		|| sample.nGlobalVol != m_origSample.nGlobalVol
		|| sample.uFlags != m_origSample.uFlags
		|| sample.RelativeTone != m_origSample.RelativeTone
		|| sample.nFineTune != m_origSample.nFineTune
		|| sample.nVibType != m_origSample.nVibType
		|| sample.nVibSweep != m_origSample.nVibSweep
		|| sample.nVibDepth != m_origSample.nVibDepth
		|| sample.nVibRate != m_origSample.nVibRate
		|| sample.rootNote != m_origSample.rootNote
		|| strcmp(sample.filename, m_origSample.filename)
		|| m_origName != m_sndFile.m_szNames[m_sample]
		|| memcmp(sample.cues, m_origSample.cues, sizeof(sample.cues));

	if(modified)
	{
		auto *modDoc = m_sndFile.GetpModDoc();
		if(modDoc->m_collabClient)
		{
			std::ostringstream ss;
			cereal::BinaryOutputArchive ar(ss);
			ar(Networking::SamplePropertyTransactionMsg);
			ar(m_sample);
			ar(sample);
			modDoc->m_collabClient->Write(ss.str());
		}
	}
}

OPENMPT_NAMESPACE_END
