/*
 * Networking.cpp
 * ---------------
 * Purpose: Collaborative Composing implementation
 * Notes  : (currently none)
 * Authors: Johannes Schultz OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "stdafx.h"
#include "Networking.h"
#include "Moddoc.h"
#include "../common/version.h"
#include <picojson/picojson.h>
#include <iostream>

std::unique_ptr<Networking::CollabServer> collabServer;
asio::io_service io_service;

OPENMPT_NAMESPACE_BEGIN

namespace Networking
{

CollabConnection::CollabConnection()
	: m_socket(io_service)
	, m_strand(io_service)
{
	//asio::ip::v6_only option(false);
	//m_socket.set_option(option);
}


void CollabConnection::Write(const std::string &message)
{
	m_strand.dispatch([this, message]()
	{
		m_messages.push_back(message);
		if(m_messages.size() > 1)
		{
			// Outstanding async_write
			return;
		}
		WriteImpl();
	});
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
	: m_acceptor(io_service, asio::ip::tcp::endpoint(asio::ip::tcp::v6(), 39999))
{
	m_thread = std::move(mpt::thread([this]()
	{
		StartAccept();
		io_service.run();
	}));
}


CollabServer::~CollabServer()
{
	io_service.stop();
	if(m_thread.joinable())
		m_thread.join();
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
			welcome["id"] = picojson::value(MptVersion::str);
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

			//std::make_shared<chat_session>(std::move(m_socket), room_)->start();
			std::cout << "accept" << std::endl;
		} else
		{
			MPT_LOCK_GUARD<mpt::mutex> lock(m_mutex);
			m_connections.erase(conn);
		}
		StartAccept();
	});
}


CollabClient::CollabClient()
	: m_socket(io_service)
{
	asio::ip::v6_only option(false);
	m_socket.set_option(option);

	asio::ip::tcp::resolver resolver(io_service);
	m_endpoint_iterator = resolver.resolve({ "localhost", "39999" });
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


void CollabClient::Close()
{
	io_service.post([this]() { m_socket.close(); });
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
