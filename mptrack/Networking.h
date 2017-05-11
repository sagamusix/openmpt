/*
 * Networking.h
 * ---------------
 * Purpose: Collaborative Composing implementation
 * Notes  : (currently none)
 * Authors: Johannes Schultz
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

const int DEFAULT_PORT = 39999;

class IOService
{
public:
	mpt::thread m_thread;

	IOService();
	~IOService();

	static void Run();
};


class CollabConnection
{
	asio::ip::tcp::socket m_socket;
	asio::io_service::strand m_strand;
	std::deque<std::string> m_messages;

public:
	CollabConnection();
	~CollabConnection();

	asio::ip::tcp::socket& GetSocket() { return m_socket; }
	std::string Read();

	void Write(const std::string &message);
	void Close();

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
	asio::ip::tcp::resolver::iterator m_endpoint_iterator;
	CollabConnection m_connection;

public:
	CollabClient(const std::string &server, const std::string &port);

	void Close();
	void Write(const std::string &msg);
};

extern std::unique_ptr<Networking::CollabServer> collabServer;
extern std::unique_ptr<Networking::IOService> ioService;

}

OPENMPT_NAMESPACE_END
