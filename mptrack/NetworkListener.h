#pragma once

OPENMPT_NAMESPACE_BEGIN

namespace Networking
{

class CollabConnection;

class Listener
{
public:
	virtual ~Listener() { }
	virtual void Receive(std::shared_ptr<CollabConnection> source, std::stringstream &msg) = 0;
};

}

OPENMPT_NAMESPACE_END
