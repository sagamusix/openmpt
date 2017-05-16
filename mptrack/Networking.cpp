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
#include <iostream>

OPENMPT_NAMESPACE_BEGIN

namespace Networking
{

asio::io_service io_service;
std::vector<std::shared_ptr<CollabClient>> collabClients;
std::shared_ptr<CollabServer> collabServer;
std::unique_ptr<IOService> ioService;


struct MsgHeader
{
	uint16le compressedSize, origSize;
};


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
	if(ioService == nullptr || !ioService->m_thread.joinable())
		ioService = mpt::make_unique<IOService>();
}


CollabConnection::CollabConnection(asio::ip::tcp::socket socket)
	: m_socket(std::move(socket))
	, m_strand(io_service)
{
	//asio::ip::v6_only option(false);
	//m_socket.set_option(option);

	m_strmIn.zalloc = Z_NULL;
	m_strmIn.zfree = Z_NULL;
	m_strmIn.opaque = Z_NULL;
	inflateInit2(&m_strmIn, MAX_WBITS);

	m_strmOut.zalloc = Z_NULL;
	m_strmOut.zfree = Z_NULL;
	m_strmOut.opaque = Z_NULL;
	deflateInit2(&m_strmOut, Z_DEFAULT_COMPRESSION /*Z_BEST_COMPRESSION*/, Z_DEFLATED, MAX_WBITS, MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY);
}


CollabConnection::~CollabConnection()
{
	Close();
	inflateEnd(&m_strmIn);
	deflateEnd(&m_strmOut);
}


std::string CollabConnection::Read()
{
	auto that = shared_from_this();
	m_inMessage.resize(sizeof(MsgHeader));
	asio::async_read(m_socket,
		asio::buffer(&m_inMessage[0], sizeof(MsgHeader)),
		[that](std::error_code ec, std::size_t /*length*/)
	{
		if(!ec)
		{
			MsgHeader header;
			std::memcpy(&header, that->m_inMessage.data(), sizeof(MsgHeader));
			that->m_inMessage.resize(header.compressedSize);
			asio::async_read(that->m_socket,
				asio::buffer(&that->m_inMessage[0], that->m_inMessage.size()),
				[that, header](std::error_code ec, std::size_t /*length*/)
			{
				if(!ec)
				{
					std::string decompressedMessage;
					decompressedMessage.reserve(header.origSize);
					auto &strm = that->m_strmIn;
					strm.avail_in = that->m_inMessage.size();
					strm.next_in = (Bytef *)that->m_inMessage.data();
					char outBuf[4096];
					do
					{
						strm.avail_out = sizeof(outBuf);
						strm.next_out = reinterpret_cast<Bytef *>(outBuf);
						inflate(&strm, Z_FINISH);
						decompressedMessage.insert(decompressedMessage.size(), outBuf, sizeof(outBuf) - strm.avail_out);
					} while(strm.avail_out == 0);
					//m_inMessage = std::move(decompressedMessage);

					that->Read();
				}
			});
		}
	});


	/*m_strand.dispatch([this]()
	{
		m_messages.push_back(compressedMessage);
		if(m_messages.size() > 1)
		{
			// Outstanding async_write
			return;
		}
		WriteImpl();
	});*/
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
	if(message.empty())
		return;

	// First, we compress the message. We flush the results but keep the state for exploiting similarities between messages.
	MsgHeader header;
	MPT_ASSERT(message.size() < size_t(Util::MaxValueOfType(header.origSize.get())));

	std::string compressedMessage(sizeof(header), 0);
	m_strmOut.avail_in = message.size();
	m_strmOut.next_in = (Bytef *)message.data();
	char outBuf[4096];
	do
	{
		m_strmOut.avail_out = sizeof(outBuf);
		m_strmOut.next_out = reinterpret_cast<Bytef *>(outBuf);
		deflate(&m_strmOut, Z_PARTIAL_FLUSH);
		compressedMessage.insert(compressedMessage.size(), outBuf, sizeof(outBuf) - m_strmOut.avail_out);
	} while(m_strmOut.avail_out == 0);
	header.compressedSize = mpt::saturate_cast<decltype(header.compressedSize)::base_type>(compressedMessage.size() - sizeof(header));
	header.origSize = mpt::saturate_cast<decltype(header.compressedSize)::base_type>(message.size());
	std::memcpy(&compressedMessage[0], &header, sizeof(header));

	auto that = shared_from_this();
	m_strand.dispatch([that, compressedMessage]()
	{
		that->m_outMessages.push_back(compressedMessage);
		if(that->m_outMessages.size() > 1)
		{
			// Outstanding async_write
			return;
		}
		that->WriteImpl();
	});
}


void CollabConnection::Close()
{
	m_socket.close();
}


void CollabConnection::WriteImpl()
{
	const std::string &message = m_outMessages.front();
	auto that = shared_from_this();
	asio::async_write(m_socket, asio::buffer(message.c_str(), message.size()),
		m_strand.wrap([that](std::error_code error, const size_t /*bytesTransferred*/)
	{
		that->m_outMessages.pop_front();
		if(error)
		{
			std::cerr << "Write Error: " << std::system_error(error).what() << std::endl;
			return;
		}

		if(!that->m_outMessages.empty())
		{
			that->WriteImpl();
		}
	}));
}


CollabServer::CollabServer()
	: m_acceptor(io_service, asio::ip::tcp::endpoint(asio::ip::tcp::v6(), DEFAULT_PORT))
	, m_socket(io_service)
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
	m_acceptor.async_accept(m_socket,
		[this](std::error_code ec)
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
			auto conn = std::make_shared<CollabConnection>(std::move(m_socket));
			m_connections.insert(conn);
			conn->Write(picojson::value(welcome).serialize());
			StartAccept();
		}
	});
}


CollabClient::CollabClient(const std::string &server, const std::string &port)
	: m_socket(io_service)
{
	asio::ip::tcp::resolver resolver(io_service);
	m_endpoint_iterator = resolver.resolve({ server, port });

	asio::async_connect(m_socket, m_endpoint_iterator,
		[this](std::error_code ec, asio::ip::tcp::resolver::iterator)
	{
		if(!ec)
		{
			m_connection = std::make_shared<CollabConnection>(std::move(m_socket));
			m_connection->Read();
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
	m_connection->Write(msg);
}

}

OPENMPT_NAMESPACE_END
