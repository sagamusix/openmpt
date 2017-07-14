/*
* NetworkTypes.cpp
* ----------------
* Purpose: Collaborative Composing implementation
* Notes  : (currently none)
* Authors: Johannes Schultz
* The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
*/


#include "stdafx.h"
#include "NetworkTypes.h"

OPENMPT_NAMESPACE_BEGIN

namespace Networking
{

void PatternEditMsg::Apply(CPattern &pat)
{
	auto mask = commands.begin();
	for(ROWINDEX r = row; row < row + numRows; row++)
	{
		auto m = pat.GetpModCommand(r, channel);
		for(CHANNELINDEX c = channel; c < channel + numChannels; c++, mask++, m++)
		{
			if(mask->mask[ModCommandMask::kNote])
				m->note = mask->m.note;
			if(mask->mask[ModCommandMask::kInstr])
				m->instr = mask->m.instr;
			if(mask->mask[ModCommandMask::kVolCmd])
				m->volcmd = mask->m.volcmd;
			if(mask->mask[ModCommandMask::kVol])
				m->vol = mask->m.vol;
			if(mask->mask[ModCommandMask::kCommand])
				m->command = mask->m.command;
			if(mask->mask[ModCommandMask::kParam])
				m->param = mask->m.param;

			// For sending back new state to all clients
			mask->mask.set();
			mask->m = *m;
		}
	}
}

}

OPENMPT_NAMESPACE_END