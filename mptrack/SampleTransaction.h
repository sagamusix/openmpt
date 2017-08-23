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


class SampleDataTransaction //: public SamplePropertyTransaction
{
	CSoundFile &m_sndFile;
	std::vector<int8> m_oldData;
	SAMPLEINDEX m_sample;

public:
	SampleDataTransaction(CSoundFile &sndFile, SAMPLEINDEX sample);
	~SampleDataTransaction();
};

OPENMPT_NAMESPACE_END
