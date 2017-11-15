#include "stdafx.h"
#include "Moddoc.h"
#include "Networking.h"
#include "PatternTransaction.h"
#include "NetworkTypes.h"

OPENMPT_NAMESPACE_BEGIN

void PatternTransaction::Init(const char *description)
{
	CModDoc *pModDoc = m_sndFile.GetpModDoc();
	const auto &pat = m_sndFile.Patterns[m_pattern];
	m_rect.Sanitize(pat.GetNumRows(), pat.GetNumChannels());
	CHANNELINDEX chnBeg = m_rect.GetStartChannel(), chnEnd = m_rect.GetEndChannel();
	ROWINDEX rowBeg = m_rect.GetStartRow(), rowEnd = m_rect.GetEndRow();

	if(chnEnd < chnBeg || rowEnd < rowBeg || pModDoc == nullptr || !m_sndFile.Patterns.IsValidPat(m_pattern))
		return;

	if(description != nullptr)
	{
		pModDoc->GetPatternUndo().PrepareUndo(m_pattern, chnBeg, rowBeg, m_rect.GetNumChannels(), m_rect.GetNumRows(), description);
	}
	m_data.reserve(m_rect.GetNumChannels() * m_rect.GetNumRows());
	for(ROWINDEX r = rowBeg; r <= rowEnd; r++)
	{
		m_data.insert(m_data.end(), pat.GetpModCommand(r, chnBeg), pat.GetpModCommand(r, chnEnd) + 1);
	}
	m_name = pat.GetName();
}


PatternTransaction::PatternTransaction(CSoundFile &sndFile, PATTERNINDEX pattern, const PatternCursor &start, const PatternCursor &end, const char *description)
	: m_sndFile(sndFile)
	, m_rect(start, end)
	, m_pattern(pattern)
{
	Init(description);
}

PatternTransaction::PatternTransaction(CSoundFile &sndFile, PATTERNINDEX pattern, const PatternCursor &cursor, const char *description)
	: m_sndFile(sndFile)
	, m_rect(cursor, cursor)
	, m_pattern(pattern)
{
	Init(description);
}

PatternTransaction::PatternTransaction(CSoundFile &sndFile, PATTERNINDEX pattern, const PatternRect &rect, const char *description)
	: m_sndFile(sndFile)
	, m_rect(rect)
	, m_pattern(pattern)
{
	Init(description);
}

PatternTransaction::PatternTransaction(CSoundFile &sndFile, PATTERNINDEX pattern, const char *description)
	: m_sndFile(sndFile)
	, m_rect(PatternRect(PatternCursor(0, 0), PatternCursor(sndFile.Patterns[pattern].GetNumRows() - 1, sndFile.GetNumChannels() - 1, PatternCursor::lastColumn)))
	, m_pattern(pattern)
{
	Init(description);
}

PatternTransaction::~PatternTransaction()
{
	// Build the difference mask
	Networking::PatternEditMsg msg(m_pattern, m_rect.GetStartRow(), m_rect.GetStartChannel(), m_rect.GetNumRows(), m_rect.GetNumChannels());
	msg.commands.reserve(m_data.size());
	ROWINDEX rowBeg = m_rect.GetStartRow(), rowEnd = m_rect.GetEndRow();
	CHANNELINDEX chnBeg = m_rect.GetStartChannel(), chnEnd = m_rect.GetEndChannel();
	auto mOrig = m_data.cbegin();
	const auto &pat = m_sndFile.Patterns[m_pattern];
	bool anyChanges = msg.nameChanged = m_name != pat.GetName();
	if(msg.nameChanged)
	{
		msg.name = pat.GetName();
	}
	for(ROWINDEX r = rowBeg; r <= rowEnd; r++)
	{
		const ModCommand *m = pat.GetpModCommand(r, chnBeg);
		for(CHANNELINDEX c = chnBeg; c <= chnEnd; c++, mOrig++, m++)
		{
			Networking::ModCommandMask mask;
			mask.m = *m;
			mask.mask[Networking::ModCommandMask::kNote] = m->note != mOrig->note;
			mask.mask[Networking::ModCommandMask::kInstr] = m->instr != mOrig->instr;
			mask.mask[Networking::ModCommandMask::kVolCmd] = m->volcmd != mOrig->volcmd;
			mask.mask[Networking::ModCommandMask::kVol] = m->vol != mOrig->vol;
			mask.mask[Networking::ModCommandMask::kCommand] = m->command != mOrig->command;
			mask.mask[Networking::ModCommandMask::kParam] = m->param != mOrig->param;
			msg.commands.push_back(mask);
			if(mask.mask.any())
			{
				if(!anyChanges)
				{
					// Trim the transferred data
					msg.numRows -= (r - rowBeg);
					msg.row = r;
					msg.commands.erase(msg.commands.begin(), msg.commands.begin() + (r - rowBeg) * msg.numChannels);
					anyChanges = true;
				}
			}
		}
	}
	if(anyChanges)
	{
		auto *modDoc = m_sndFile.GetpModDoc();
		modDoc->SetModified();
		if(modDoc->m_collabClient)
		{
			std::ostringstream ss;
			cereal::BinaryOutputArchive ar(ss);
			ar(Networking::PatternTransactionMsg, msg);
			modDoc->m_collabClient->Write(ss.str());
		}
	}
}


MultiPatternTransaction::MultiPatternTransaction(CSoundFile &sndFile)
{
	m_transactions.reserve(sndFile.Patterns.GetNumPatterns());
	for(PATTERNINDEX pat = 0; pat < sndFile.Patterns.GetNumPatterns(); pat++)
	{
		m_transactions.push_back(PatternTransaction(sndFile, pat));
	}
}


PatternResizeTransaction::PatternResizeTransaction(CSoundFile &sndFile, PATTERNINDEX pattern, bool resizeAtEnd)
	: m_sndFile(sndFile)
	, m_pattern(pattern)
	, m_rows(0)
	, m_resizeAtEnd(resizeAtEnd)
{
	if(m_sndFile.Patterns.IsValidPat(m_pattern))
		m_rows = m_sndFile.Patterns[m_pattern].GetNumRows();
}


PatternResizeTransaction::~PatternResizeTransaction()
{
	ROWINDEX newRows = 0;
	if(m_sndFile.Patterns.IsValidPat(m_pattern))
		newRows = m_sndFile.Patterns[m_pattern].GetNumRows();

	if(newRows != m_rows)
	{
		auto *modDoc = m_sndFile.GetpModDoc();
		//modDoc->SetModified();
		if(modDoc->m_collabClient)
		{
			// TODO: we probably need to synchronize all clients here
			std::ostringstream ss;
			cereal::BinaryOutputArchive ar(ss);
			ar(Networking::PatternResizeMsg, m_pattern, newRows, m_resizeAtEnd);
			modDoc->m_collabClient->Write(ss.str());
		}
	}
}


OPENMPT_NAMESPACE_END
