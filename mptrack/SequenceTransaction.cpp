#include "stdafx.h"
#include "Moddoc.h"
#include "Networking.h"
#include "SequenceTransaction.h"
#include "NetworkTypes.h"

OPENMPT_NAMESPACE_BEGIN


SequenceTransaction::SequenceTransaction(CSoundFile &sndFile, SEQUENCEINDEX seq)
	: m_sndFile(sndFile)
	, m_seq(seq != SEQUENCEINDEX_INVALID ? seq : sndFile.Order.GetCurrentSequenceIndex())
	, m_data(sndFile.Order())
{
}


SequenceTransaction::~SequenceTransaction()
{
	// TODO: Deleted sequence
	if(m_seq < m_sndFile.Order.GetNumSequences())
	{
		const ModSequence &seq = m_sndFile.Order(m_seq);
		if(seq != m_data)
		{
			auto *modDoc = m_sndFile.GetpModDoc();
			//modDoc->SetModified();
			if(modDoc->m_collabClient)
			{
				std::ostringstream ss;
				cereal::BinaryOutputArchive ar(ss);
				ar(Networking::SequenceTransactionMsg);
				Networking::SequenceMsg msg;
				msg.seq = m_seq;
				msg.nameChanged = seq.GetName() != m_data.GetName();
				if(msg.nameChanged)
				{
					msg.name = seq.GetName();
					msg.nameChanged = true;
				}
				msg.length = seq.GetLength();
				msg.pat = seq;
				msg.patChanged.resize(msg.pat.size());
				ORDERINDEX lastChanged = 0;
				for(ORDERINDEX ord = 0; ord < msg.length; ord++)
				{
					bool changed = (ord >= m_data.GetLength()) || seq[ord] != m_data[ord];
					msg.patChanged[ord] = changed;
					if(changed) lastChanged = ord;
				}
				msg.pat.resize(lastChanged + 1);
				msg.patChanged.resize(msg.pat.size());

				ar(msg);
				modDoc->m_collabClient->Write(ss.str());
			}
		}
	}

}

OPENMPT_NAMESPACE_END
