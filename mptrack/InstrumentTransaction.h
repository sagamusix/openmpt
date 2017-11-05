#pragma once
#include "SampleTransaction.h"

OPENMPT_NAMESPACE_BEGIN

class CSoundFile;
struct ModInstrument;

class InstrumentTransaction
{
	//CriticalSection cs;	// TODO
	const CSoundFile &m_sndFile;
	const ModInstrument m_origInstr;
	INSTRUMENTINDEX m_instr;
	bool m_instrExisted;

public:
	InstrumentTransaction(const CSoundFile &sndFile, INSTRUMENTINDEX instr);
	~InstrumentTransaction();

	static void SendTunings(const CSoundFile &sndFile);
};

// Transaction for replacing entire instrument including sample slots
struct InstrumentReplaceTransaction
{
	InstrumentTransaction m_instrTransation;
	std::vector<SampleDataTransaction> m_sampleDataTransactions;
	std::vector<SamplePropertyTransaction> m_samplePropTransactions;

	InstrumentReplaceTransaction(CSoundFile &sndFile, INSTRUMENTINDEX instr);
	~InstrumentReplaceTransaction();
};

OPENMPT_NAMESPACE_END
