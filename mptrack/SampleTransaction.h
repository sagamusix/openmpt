#pragma once
#include "../soundlib/Snd_defs.h"

OPENMPT_NAMESPACE_BEGIN

class CSoundFile;
struct ModSample;

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


class SampleDataTransaction
{
	CSoundFile &m_sndFile;
	std::vector<int8> m_oldData;
	SAMPLEINDEX m_sample;
	bool m_force;

public:
	SampleDataTransaction(CSoundFile &sndFile, SAMPLEINDEX sample, bool force = false);
	~SampleDataTransaction();
};

OPENMPT_NAMESPACE_END
