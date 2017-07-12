#pragma once

OPENMPT_NAMESPACE_BEGIN

class InstrumentTransaction
{
	//CriticalSection cs;	// TODO
	CSoundFile &m_sndFile;
	const ModInstrument m_origInstr;
	INSTRUMENTINDEX m_instr;
	bool m_instrExisted;

public:
	InstrumentTransaction(CSoundFile &sndFile, INSTRUMENTINDEX instr);
	~InstrumentTransaction();
};

OPENMPT_NAMESPACE_END
