/*
 * Networking.h
 * ---------------
 * Purpose: Collaborative Composing implementation
 * Notes  : (currently none)
 * Authors: Johannes Schultz OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once
#include <asio.hpp>
#include <set>
#include "../common/mptMutex.h"

OPENMPT_NAMESPACE_BEGIN

class CModDoc;

namespace Networking
{

class CollabConnection
{
	asio::ip::tcp::socket m_socket;

public:
	CollabConnection();
	asio::ip::tcp::socket& GetSocket() { return m_socket; }
};

class CollabServer
{
	std::set<CModDoc *> m_documents;
	std::set<std::shared_ptr<CollabConnection>> m_connections;
	asio::ip::tcp::acceptor m_acceptor;
	mpt::mutex m_mutex;

public:
	CollabServer();
	void Run();
	void CloseDocument(CModDoc *modDoc);

protected:
	void StartAccept();
};


class CollabClient
{
	asio::ip::tcp::socket m_socket;
	asio::ip::tcp::resolver::iterator m_endpoint_iterator;

	CollabClient();

	void Close();
	void Write(const std::string &msg);
};

}

OPENMPT_NAMESPACE_END
