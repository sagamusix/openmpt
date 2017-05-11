/*
 * Networking.cpp
 * ---------------
 * Purpose: Collaborative Composing implementation
 * Notes  : (currently none)
 * Authors: Johannes Schultz
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "stdafx.h"
#include "Networking.h"
#include "Moddoc.h"
#include "../common/version.h"
#include <picojson/picojson.h>
#include <zlib/zlib.h>
#include <iostream>

OPENMPT_NAMESPACE_BEGIN

namespace Networking
{

asio::io_service io_service;
std::unique_ptr<Networking::CollabServer> collabServer;
std::unique_ptr<Networking::IOService> ioService;


IOService::IOService()
{
	m_thread = std::move(mpt::thread([this]()
	{
		io_service.run();
	}));
}


IOService::~IOService()
{
	io_service.stop();
	if(m_thread.joinable())
		m_thread.join();
}


void IOService::Run()
{
	if(ioService == nullptr)
		ioService = mpt::make_unique<IOService>();
}


CollabConnection::CollabConnection()
	: m_socket(io_service)
	, m_strand(io_service)
{
	//asio::ip::v6_only option(false);
	//m_socket.set_option(option);
}


CollabConnection::~CollabConnection()
{
	Close();
}


std::string CollabConnection::Read()
{
	/*asio::async_read(m_socket,
		asio::buffer(read_msg_.body(), read_msg_.body_length()),
		[this](boost::system::error_code ec, std::size_t length)
	{
		if(!ec)
		{
			std::cout.write(read_msg_.body(), read_msg_.body_length());
			std::cout << "\n";
			//do_read_header();
		} else
		{
			m_socket.close();
		}
	});*/
	return "";
}

void CollabConnection::Write(const std::string &message)
{
	// First, try to compress the message
	std::string compressedMessage(4, 0);
	z_stream strm;
	strm.zalloc = Z_NULL;
	strm.zfree = Z_NULL;
	strm.opaque = Z_NULL;
	auto ret = deflateInit2(&strm, /*Z_DEFAULT_COMPRESSION*/ Z_BEST_COMPRESSION, Z_DEFLATED, 15, 8, Z_DEFAULT_STRATEGY);
	strm.avail_in = message.size();
	strm.next_in = (Bytef *)message.data();
	char outBuf[4096];
	do
	{
		strm.avail_out = 4096;
		strm.next_out = reinterpret_cast<Bytef *>(outBuf);
		ret = deflate(&strm, Z_FINISH);
		compressedMessage.insert(compressedMessage.size(), outBuf, 4096 - strm.avail_out);
	} while(strm.avail_out == 0);
	deflateEnd(&strm);
	int32le size;
	if(compressedMessage.size() - 4 > message.size())
	{
		// Too short to compress - denoted by negative length
		std::copy(message.begin(), message.end(), compressedMessage.begin() + 4);
		compressedMessage.resize(message.size() + 4);
		size = -mpt::saturate_cast<int32>(message.size());
	} else
	{
		size = mpt::saturate_cast<int32>(compressedMessage.size() - 4);
	}
	std::memcpy(&compressedMessage[0], &size, 4);

	m_strand.dispatch([this, compressedMessage]()
	{
		m_messages.push_back(compressedMessage);
		if(m_messages.size() > 1)
		{
			// Outstanding async_write
			return;
		}
		WriteImpl();
	});
}


void CollabConnection::Close()
{
	io_service.post([this]() { m_socket.close(); });
}


void CollabConnection::WriteImpl()
{
	const std::string &message = m_messages.front();
	asio::async_write(m_socket, asio::buffer(message.c_str(), message.size()),
		m_strand.wrap([this](std::error_code error, const size_t /*bytesTransferred*/)
	{
		m_messages.pop_front();
		if(error)
		{
			std::cerr << "Write Error: " << std::system_error(error).what() << std::endl;
			return;
		}

		if(!m_messages.empty())
		{
			WriteImpl();
		}
	}));
}


CollabServer::CollabServer()
	: m_acceptor(io_service, asio::ip::tcp::endpoint(asio::ip::tcp::v6(), DEFAULT_PORT))
{
	StartAccept();
	IOService::Run();
}


CollabServer::~CollabServer()
{
}


void CollabServer::AddDocument(CModDoc *modDoc)
{
	MPT_LOCK_GUARD<mpt::mutex> lock(m_mutex);
	m_documents.insert(modDoc);
}


void CollabServer::CloseDocument(CModDoc *modDoc)
{
	MPT_LOCK_GUARD<mpt::mutex> lock(m_mutex);
	// TODO: Close connections for all clients that belong to this document
	m_documents.erase(modDoc);
}


void CollabServer::StartAccept()
{
	auto conn = std::make_shared<CollabConnection>();
	{
		MPT_LOCK_GUARD<mpt::mutex> lock(m_mutex);
		m_connections.insert(conn);
	}
	m_acceptor.async_accept(conn->GetSocket(),
		[this, conn](std::error_code ec)
	{
		if(!ec)
		{
			picojson::object welcome;
			welcome["version"] = picojson::value(MptVersion::str);
			picojson::array docs;
			MPT_LOCK_GUARD<mpt::mutex> lock(m_mutex);
			docs.reserve(m_documents.size());
			for(auto &doc : m_documents)
			{
				picojson::object docObj;
				docObj["name"] = picojson::value(mpt::ToCharset(mpt::CharsetUTF8, doc.GetDocument()->GetTitle()));
				docObj["collaborators"] = picojson::value(1.0);
				docObj["collaborators_max"] = picojson::value(4.0);
				docs.push_back(picojson::value(docObj));
			}
			welcome["docs"] = picojson::value(docs);
			conn->Write(picojson::value(welcome).serialize());
		} else
		{
			MPT_LOCK_GUARD<mpt::mutex> lock(m_mutex);
			m_connections.erase(conn);
		}
		StartAccept();
	});
}


CollabClient::CollabClient(const std::string &server, const std::string &port)
{
	asio::ip::tcp::resolver resolver(io_service);
	m_endpoint_iterator = resolver.resolve({ server, port });

	asio::async_connect(m_connection.GetSocket(), m_endpoint_iterator,
		[this](std::error_code ec, asio::ip::tcp::resolver::iterator)
	{
		if(!ec)
		{
			//do_read_header();
		}
	});
	
	IOService::Run();
	/*chat_client c(io_service, endpoint_iterator);

	std::thread t([&io_service]() { io_service.run(); });

	char line[chat_message::max_body_length + 1];
	while(std::cin.getline(line, chat_message::max_body_length + 1))
	{
		chat_message msg;
		msg.body_length(std::strlen(line));
		std::memcpy(msg.body(), line, msg.body_length());
		msg.encode_header();
		c.write(msg);
	}

	c.close();
	t.join();*/
}


void CollabClient::Write(const std::string &msg)
{
	io_service.post([this, msg]()
	{
		/*bool write_in_progress = !write_msgs_.empty();
		write_msgs_.push_back(msg);
		if(!write_in_progress)
		{
			do_write();
		}*/
	});
}

}

OPENMPT_NAMESPACE_END
