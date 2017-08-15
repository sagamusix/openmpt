#pragma once

OPENMPT_NAMESPACE_BEGIN

class SequenceTransaction
{
	//CriticalSection cs;	// TODO
	CSoundFile &m_sndFile;
	SEQUENCEINDEX m_seq;
	ModSequence m_data;

public:
	SequenceTransaction(CSoundFile &sndFile);
	~SequenceTransaction();
};

OPENMPT_NAMESPACE_END
