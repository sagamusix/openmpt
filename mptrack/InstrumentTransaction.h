#pragma once

OPENMPT_NAMESPACE_BEGIN

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

OPENMPT_NAMESPACE_END
