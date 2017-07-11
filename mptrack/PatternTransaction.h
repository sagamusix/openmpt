#pragma once

#include "PatternCursor.h"

OPENMPT_NAMESPACE_BEGIN

class PatternTransaction
{
	CSoundFile &m_sndFile;
	PatternRect m_rect;
	PATTERNINDEX m_pattern;
	std::vector<ModCommand> m_data;

	void Init(const char *description);

public:
	PatternTransaction(CSoundFile &sndFile, PATTERNINDEX pattern, const PatternCursor &start, const PatternCursor &end, const char *description = nullptr);
	PatternTransaction(CSoundFile &sndFile, PATTERNINDEX pattern, const PatternCursor &cursor, const char *description = nullptr);
	PatternTransaction(CSoundFile &sndFile, PATTERNINDEX pattern, const PatternRect &rect, const char *description = nullptr);
	~PatternTransaction();
};

OPENMPT_NAMESPACE_END
