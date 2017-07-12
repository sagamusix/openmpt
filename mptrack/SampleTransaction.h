#pragma once

OPENMPT_NAMESPACE_BEGIN

class SamplePropertyTransaction
{
	//CriticalSection cs;	// TODO
	CSoundFile &m_sndFile;
	const ModSample m_origSample;
	const std::string m_origName;
	SAMPLEINDEX m_sample;

public:
	SamplePropertyTransaction(CSoundFile &sndFile, SAMPLEINDEX sample);
	~SamplePropertyTransaction();
};

OPENMPT_NAMESPACE_END
