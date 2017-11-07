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
#include <atomic>
#include <deque>
#include <future>
#include <map>
#include <zlib/zlib.h>
#include "../common/mptMutex.h"
#include "../common/mptThread.h"
#include "NetworkListener.h"

OPENMPT_NAMESPACE_BEGIN

class CModDoc;

namespace Networking
{

const int DEFAULT_PORT = 44100;
const int MAX_CLIENTS = 64;

typedef uint32 ClientID;

class IOService
{
public:
	mpt::thread m_thread;

	IOService();
	~IOService();

	static void Run();
	static void Stop();
};


class CollabConnection : public std::enable_shared_from_this<CollabConnection>
{
public:
	z_stream m_strmIn, m_strmOut;
	std::string m_inMessage;
	std::weak_ptr<Listener> m_listener;
	CModDoc *m_modDoc;
	mpt::ustring m_userName;
	uint32 m_accessType;
	uint32 m_origSize;
	std::promise<std::string> m_promise;
	ClientID m_id;
	static ClientID m_nextId;

public:
	CollabConnection(std::shared_ptr<Listener> listener);
	virtual ~CollabConnection();

	void Write(const std::string &message);
	std::string WriteWithResult(const std::string &message);
	virtual void Read() = 0;
	virtual void Send(const std::string &message) = 0;
	virtual void Close() = 0;

	void SetListener(std::shared_ptr<Listener> listener)
	{
		m_listener = listener;
	}

protected:
	bool Parse();
};


class RemoteCollabConnection : public CollabConnection
{
	struct Message
	{
		std::string msg;
		size_t remain, written;
	};
	std::deque<Message> m_outMessages;
	asio::ip::tcp::socket m_socket;
	asio::io_service::strand m_strand;
	mpt::thread m_thread;
	std::atomic<bool> m_threadRunning = true;

	mutable mpt::mutex m_mutex;

public:
	RemoteCollabConnection(asio::ip::tcp::socket socket, std::shared_ptr<Listener> listener);
	RemoteCollabConnection(const RemoteCollabConnection &) = delete;
	RemoteCollabConnection(RemoteCollabConnection &&) = default;
	~RemoteCollabConnection();

	void Read() override;
	void Send(const std::string &message) override;
	void Close() override;

protected:
	void WriteImpl();
};


class LocalCollabConnection : public CollabConnection
{
public:
	LocalCollabConnection(std::shared_ptr<Listener> listener);
	LocalCollabConnection(const LocalCollabConnection &) = delete;
	LocalCollabConnection(LocalCollabConnection &&) = default;

	void Send(const std::string &message) override;
	void Read() override { }
	void Close() override { }
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
	virtual std::string WriteWithResult(const std::string &msg) = 0;

	bool Receive(std::shared_ptr<CollabConnection> source, std::stringstream &msg) override;

	void Quit();

	void SetListener(std::shared_ptr<Listener> listener)
	{
		m_listener = listener;
	}

	std::shared_ptr<CollabConnection> GetConnection()
	{
		return m_connection;
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
	std::string WriteWithResult(const std::string &msg) override;
};


class LocalCollabClient : public CollabClient
{
public:
	LocalCollabClient(CModDoc &modDoc);

	void Write(const std::string &msg) override;
	std::string WriteWithResult(const std::string &msg) override;
};


class NetworkedDocument
{
public:
	mpt::ustring m_password;
	int m_collaborators, m_maxCollaborators;
	int m_spectators, m_maxSpectators;
	std::vector<std::shared_ptr<CollabConnection>> m_connections;

	NetworkedDocument(int collaborators = 1, int spectators = 0, const mpt::ustring &password = mpt::ustring(), std::shared_ptr<CollabConnection> connection = nullptr)
		: m_password(password)
		, m_collaborators(0)
		, m_maxCollaborators(collaborators)
		, m_spectators(0)
		, m_maxSpectators(spectators)
	{
		if(connection)
		{
			m_connections.push_back(connection);
		}
	}
};


class CollabServer : public Listener, public std::enable_shared_from_this<CollabServer>
{
	std::map<CModDoc *, NetworkedDocument> m_documents;
	std::vector<std::shared_ptr<CollabConnection>> m_connections;
	asio::ip::tcp::acceptor m_acceptor;
	asio::ip::tcp::socket m_socket;
	mutable mpt::mutex m_mutex;
	mpt::thread m_thread;
	const int m_port;

public:
	CollabServer();
	~CollabServer();

	std::shared_ptr<LocalCollabClient> AddDocument(CModDoc &modDoc, int collaborators, int spectators, const mpt::ustring &password);
	// Return the document's sharing properties, or default properties if document is not shared yet.
	NetworkedDocument GetDocument(CModDoc &modDoc) const;
	void CloseDocument(CModDoc &modDoc);

	bool Receive(std::shared_ptr<CollabConnection> source, std::stringstream &msg) override;

	void StartAccept();

	int Port() const { return m_port; }

protected:
	static void SendToAll(NetworkedDocument &doc, const std::ostringstream &sso);
};


extern std::vector<std::shared_ptr<CollabClient>> collabClients;
extern std::shared_ptr<CollabServer> collabServer;
extern std::unique_ptr<IOService> ioService;

}

OPENMPT_NAMESPACE_END
