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
#include <zlib/zlib.h>
#include "../common/mptMutex.h"
#include "../common/mptThread.h"
#include "NetworkListener.h"

OPENMPT_NAMESPACE_BEGIN

class CModDoc;

namespace Networking
{

const int DEFAULT_PORT = 44100;

class IOService
{
public:
	mpt::thread m_thread;

	IOService();
	~IOService();

	static void Run();
};


class CollabConnection : public std::enable_shared_from_this<CollabConnection>
{
public:
	z_stream m_strmIn, m_strmOut;
	std::string m_inMessage;
	std::weak_ptr<Listener> m_listener;
	CModDoc *m_modDoc;

public:
	CollabConnection(std::shared_ptr<Listener> listener);
	virtual ~CollabConnection();

	void Write(const std::string &message);
	virtual void Read() = 0;
	virtual void Send(const std::string &message) = 0;

	void SetListener(std::shared_ptr<Listener> listener)
	{
		m_listener = listener;
	}

protected:
	bool Parse();
};


class RemoteCollabConnection : public CollabConnection, public std::enable_shared_from_this<RemoteCollabConnection>
{
	std::deque<std::string> m_outMessages;
	asio::ip::tcp::socket m_socket;
	asio::io_service::strand m_strand;

public:
	RemoteCollabConnection(asio::ip::tcp::socket socket, std::shared_ptr<Listener> listener);
	RemoteCollabConnection(const RemoteCollabConnection &) = delete;
	RemoteCollabConnection(RemoteCollabConnection &&) = default;
	~RemoteCollabConnection();

	void Read() override;
	void Send(const std::string &message) override;
	void Close();

protected:
	void WriteImpl();
};


class LocalCollabConnection : public CollabConnection//, public std::enable_shared_from_this<LocalCollabConnection>
{
public:
	LocalCollabConnection(std::shared_ptr<Listener> listener);
	LocalCollabConnection(const LocalCollabConnection &) = delete;
	LocalCollabConnection(LocalCollabConnection &&) = default;

	void Send(const std::string &message) override;
	void Read() override { }
};


class CollabClient : public Listener
{
protected:
	std::weak_ptr<Listener> m_listener;
	std::shared_ptr<CollabConnection> m_connection;

public:
	CollabClient(std::shared_ptr<Listener> listener, std::shared_ptr<CollabConnection> connection = nullptr)
		: m_listener(listener)
		, m_connection(connection)
	{ }

	virtual void Write(const std::string &msg) = 0;

	void Receive(std::shared_ptr<CollabConnection> source, std::stringstream &msg) override;

	void SetListener(std::shared_ptr<Listener> listener)
	{
		m_listener = listener;
	}
};


class RemoteCollabClient : public CollabClient, public std::enable_shared_from_this<RemoteCollabClient>
{
private:
	asio::ip::tcp::resolver::iterator m_endpoint_iterator;
	asio::ip::tcp::socket m_socket;
public:
	RemoteCollabClient(const std::string &server, const std::string &port, std::shared_ptr<Listener> listener);
	bool Connect();

	void Write(const std::string &msg) override;
};


class LocalCollabClient : public CollabClient, public std::enable_shared_from_this<LocalCollabClient>
{
public:
	LocalCollabClient(CModDoc &modDoc);

	void Write(const std::string &msg) override;
};


class NetworkedDocument
{
public:
	CModDoc &m_modDoc;
	mpt::ustring m_password;
	int m_collaborators, m_maxCollaborators;
	int m_spectators, m_maxSpectators;
	std::vector<std::shared_ptr<CollabConnection>> m_connections;
	// TODO: add connections here

	NetworkedDocument(CModDoc &modDoc, int collaborators = 0, int spectators = 0, const mpt::ustring &password = mpt::ustring())
		: m_modDoc(modDoc)
		, m_password(password)
		, m_collaborators(0)
		, m_maxCollaborators(collaborators)
		, m_spectators(0)
		, m_maxSpectators(spectators)
	{ }

	operator CModDoc& () { return m_modDoc; }
	operator const CModDoc& () const { return m_modDoc; }
	bool operator< (const NetworkedDocument &other) const { return &m_modDoc < &other.m_modDoc; }
};


class CollabServer : public Listener, public std::enable_shared_from_this<CollabServer>
{
	std::set<NetworkedDocument> m_documents;
	std::vector<std::shared_ptr<CollabConnection>> m_connections;
	asio::ip::tcp::acceptor m_acceptor;
	asio::ip::tcp::socket m_socket;
	mpt::mutex m_mutex;
	mpt::thread m_thread;
	const int m_port;

public:
	CollabServer();
	~CollabServer();

	void AddDocument(CModDoc &modDoc, int collaborators, int spectators, const mpt::ustring &password);
	void CloseDocument(CModDoc &modDoc);

	void SendMessage(CModDoc &modDoc, const std::string msg);

	void Receive(std::shared_ptr<CollabConnection> source, std::stringstream &msg) override;

	void StartAccept();

	int Port() const { return m_port; }
};


extern std::vector<std::shared_ptr<CollabClient>> collabClients;
extern std::shared_ptr<CollabServer> collabServer;
extern std::unique_ptr<IOService> ioService;

}

OPENMPT_NAMESPACE_END
