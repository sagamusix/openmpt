#include "stdafx.h"
#include "Moddoc.h"
#include "Networking.h"
#include "SampleTransaction.h"
#include "NetworkTypes.h"
#include "../common/StringFixer.h"

OPENMPT_NAMESPACE_BEGIN

SamplePropertyTransaction::SamplePropertyTransaction(CSoundFile &sndFile, SAMPLEINDEX sample)
	: m_sndFile(sndFile)
	, m_origSample(sample <= sndFile.GetNumSamples() ? sndFile.GetSample(sample) : ModSample())
	, m_origName(sndFile.m_szNames[sample])
	, m_sample(sample)
{
}


SamplePropertyTransaction::~SamplePropertyTransaction()
{
	static const ModSample defaultSample = ModSample();
	const ModSample &sample = m_sample <= m_sndFile.GetNumSamples() ? m_sndFile.GetSample(m_sample) : defaultSample;

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
			Networking::SamplePropertyEditMsg msg{ m_sample, sample };
			mpt::String::Copy(msg.name, m_sndFile.m_szNames[m_sample]);
			ar(msg);
			modDoc->m_collabClient->Write(ss.str());
		}
	}
}


SampleDataTransaction::SampleDataTransaction(CSoundFile &sndFile, SAMPLEINDEX sample, bool force)
	: m_sndFile(sndFile)
	, m_sample(sample)
	, m_force(force)
{
	static const ModSample defaultSample = ModSample();
	const ModSample &smp = m_sample <= m_sndFile.GetNumSamples() ? m_sndFile.GetSample(m_sample) : defaultSample;
	if(smp.HasSampleData() && !force)
	{
		m_oldData.assign(smp.pSample8, smp.pSample8 + smp.GetSampleSizeInBytes());
	}
}


SampleDataTransaction::~SampleDataTransaction()
{
	static const ModSample defaultSample = ModSample();
	const ModSample &smp = m_sample <= m_sndFile.GetNumSamples() ? m_sndFile.GetSample(m_sample) : defaultSample;
	auto newData = mpt::as_span(smp.pSample8, smp.pSample8 + smp.GetSampleSizeInBytes());
	cereal::size_type modificationStart = 0, modificationLength = newData.size();
	bool modified = m_force;
	if(!modified)
	{
		if(newData.size() != m_oldData.size())
		{
			modified = true;
		} else
		{
			auto p1 = newData.cbegin();
			auto p2 = m_oldData.cbegin();
			while(p1 != newData.cend())
			{
				if(*p1 != *p2)
				{
					modificationStart = std::distance(newData.cbegin(), p1);
					modified = true;
					break;
				}
				p1++;
				p2++;
			}

			auto p3 = newData.cend();
			auto p4 = m_oldData.cend();
			while(p1 != p3)
			{
				p3--;
				p4--;
				if(*p3 != *p4)
				{
					modificationLength = std::distance(p1, p3) + 1;
					break;
				}
			}

		}
	}

	if(modified)
	{
		auto *modDoc = m_sndFile.GetpModDoc();
		if(modDoc->m_collabClient)
		{
			std::ostringstream ss;
			cereal::BinaryOutputArchive ar(ss);
			Networking::SamplePropertyEditMsg msg{ m_sample, smp };
			mpt::String::Copy(msg.name, m_sndFile.m_szNames[m_sample]);
			ar(Networking::SampleDataTransactionMsg, msg);
			ar(modificationStart, modificationLength);
			ar(cereal::binary_data(newData.data() + modificationStart, static_cast<size_t>(modificationLength)));
			modDoc->m_collabClient->Write(ss.str());
		}
	}
}


OPENMPT_NAMESPACE_END
