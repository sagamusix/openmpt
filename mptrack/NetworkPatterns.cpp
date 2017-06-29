#include "stdafx.h"
#include "Moddoc.h"
#include "Networking.h"
#include "NetworkPatterns.h"
#include "NetworkTypes.h"

OPENMPT_NAMESPACE_BEGIN

void PatternTransaction::Init(const char *description)
{
	CModDoc *pModDoc = m_sndFile.GetpModDoc();
	CHANNELINDEX chnBeg = m_rect.GetStartChannel(), chnEnd = m_rect.GetEndChannel();
	ROWINDEX rowBeg = m_rect.GetStartRow(), rowEnd = m_rect.GetEndRow();

	if(chnEnd < chnBeg || rowEnd < rowBeg || pModDoc == nullptr || !m_sndFile.Patterns.IsValidPat(m_pattern))
		return;

	if(description != nullptr)
	{
		pModDoc->GetPatternUndo().PrepareUndo(m_pattern, chnBeg, rowBeg, m_rect.GetNumChannels(), m_rect.GetNumRows(), description);
	}
	m_data.reserve(m_rect.GetNumChannels() * m_rect.GetNumRows());
	const auto &pat = m_sndFile.Patterns[m_pattern];
	for(ROWINDEX r = rowBeg; r <= rowEnd; r++)
	{
		m_data.insert(m_data.end(), pat.GetpModCommand(r, chnBeg), pat.GetpModCommand(r, chnEnd + 1));
	}
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

PatternTransaction::~PatternTransaction()
{
	// Build the difference mask
	Networking::PatternEditMsg msg(m_pattern, m_rect.GetStartRow(), m_rect.GetStartChannel(), m_rect.GetNumRows(), m_rect.GetNumChannels());
	msg.commands.reserve(m_data.size());
	ROWINDEX rowBeg = m_rect.GetStartRow(), rowEnd = m_rect.GetEndRow();
	CHANNELINDEX chnBeg = m_rect.GetStartChannel(), chnEnd = m_rect.GetEndChannel();
	auto mOrig = m_data.cbegin();
	const auto &pat = m_sndFile.Patterns[m_pattern];
	bool anyChanges = false;
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
		if(Networking::collabServer != nullptr)
		{
			// TODO: Send over network
			//Networking::collabServer->SendMessage(m_sndFile.GetpModDoc(), )
		}
	}
}

OPENMPT_NAMESPACE_END
