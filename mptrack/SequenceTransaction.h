#pragma once

OPENMPT_NAMESPACE_BEGIN

class SequenceTransaction
{
	CSoundFile &m_sndFile;
	SEQUENCEINDEX m_seq;
	ModSequence m_data;

public:
	SequenceTransaction(CSoundFile &sndFile, SEQUENCEINDEX seq = SEQUENCEINDEX_INVALID);
	~SequenceTransaction();
};

OPENMPT_NAMESPACE_END
