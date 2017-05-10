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
#include <deque>
#include "../common/mptMutex.h"
#include "../common/mptThread.h"

OPENMPT_NAMESPACE_BEGIN

class CModDoc;

namespace Networking
{

class CollabConnection
{
	asio::ip::tcp::socket m_socket;
	asio::io_service::strand m_strand;
	std::deque<std::string> m_messages;

public:
	CollabConnection();
	asio::ip::tcp::socket& GetSocket() { return m_socket; }
	void Write(const std::string &message);

protected:
	void WriteImpl();
};


class NetworkedDocument
{
	CModDoc *m_modDoc;

public:
	NetworkedDocument(CModDoc *modDoc)
		: m_modDoc(modDoc)
	{ }

	operator CModDoc* () { return m_modDoc; }
	operator const CModDoc* () const { return m_modDoc; }
	CModDoc* GetDocument() { return m_modDoc; }
	const CModDoc* GetDocument() const { return m_modDoc; }
};


class CollabServer
{
	std::set<NetworkedDocument> m_documents;
	std::set<std::shared_ptr<CollabConnection>> m_connections;
	asio::ip::tcp::acceptor m_acceptor;
	mpt::mutex m_mutex;
	mpt::thread m_thread;

public:
	CollabServer();
	~CollabServer();

	void AddDocument(CModDoc *modDoc);
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

extern std::unique_ptr<Networking::CollabServer> collabServer;

OPENMPT_NAMESPACE_END
