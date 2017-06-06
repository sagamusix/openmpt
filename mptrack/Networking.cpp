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
#include "NetworkTypes.h"
#include "Moddoc.h"
#include "../common/version.h"
#include <cereal/cereal.hpp>
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
	uint32le compressedSize, origSize;
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


CollabConnection::CollabConnection(asio::ip::tcp::socket socket, std::shared_ptr<Listener> listener)
	: m_socket(std::move(socket))
	, m_strand(io_service)
	, m_listener(listener)
{
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


void CollabConnection::Read()
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

			if(asio::read(that->m_socket, asio::buffer(&that->m_inMessage[0], header.compressedSize)) >= header.compressedSize)
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
				if(auto listener = that->m_listener.lock())
				{
					listener->Receive(that.get(), decompressedMessage);
					that->Read();
				}
			}
		}
	});
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
}


CollabServer::~CollabServer()
{
}


void CollabServer::AddDocument(CModDoc &modDoc, int collaborators, int spectators, const mpt::ustring &password)
{
	MPT_LOCK_GUARD<mpt::mutex> lock(m_mutex);
	m_documents.insert(NetworkedDocument(modDoc, collaborators, spectators, password));
}


void CollabServer::CloseDocument(CModDoc &modDoc)
{
	MPT_LOCK_GUARD<mpt::mutex> lock(m_mutex);
	// TODO: Close connections for all clients that belong to this document
	m_documents.erase(NetworkedDocument(modDoc));
}


void CollabServer::StartAccept()
{
	auto that = shared_from_this();
	m_acceptor.async_accept(m_socket,
		[that](std::error_code ec)
	{
		if(!ec)
		{
			that->m_connections.push_back(std::make_shared<CollabConnection>(std::move(that->m_socket), that));
			that->m_connections.back()->Read();
			that->StartAccept();
		}
	});
	IOService::Run();
}


void CollabServer::Receive(CollabConnection *source, const std::string &msg)
{
	std::string type = msg.substr(0, 4);
	if(type == "LIST")
	{
		WelcomeMsg welcome;
		welcome.version = MptVersion::str;
		welcome.documents.reserve(m_documents.size());
		for(auto &doc : m_documents)
		{
			DocumentInfo info;
			info.id = reinterpret_cast<uint64>(&doc.m_modDoc);
			info.name = mpt::ToCharset(mpt::CharsetUTF8, doc.m_modDoc.GetTitle());
			info.collaborators = doc.m_collaborators;
			info.maxCollaboratos = doc.m_maxCollaborators;
			info.spectators = doc.m_spectators;
			info.maxSpectators = doc.m_maxSpectators;
			info.password = !doc.m_password.empty();
			welcome.documents.push_back(info);
		}

		std::ostringstream ss;
		cereal::BinaryOutputArchive ar(ss);
		ar(welcome);
		source->Write("LIST");
		source->Write(ss.str());
	} else if(type == "CONN")
	{
		std::istringstream ssi(msg.substr(4));
		JoinMsg join;
		cereal::BinaryInputArchive inArchive(ssi);
		inArchive >> join;
		CModDoc *modDoc = reinterpret_cast<CModDoc *>(join.id);
		auto doc = m_documents.find(NetworkedDocument(*modDoc));

		std::ostringstream sso;
		cereal::BinaryOutputArchive ar(sso);

		if(doc != m_documents.end())
		{
			if(mpt::ToUnicode(mpt::CharsetUTF8, join.password) == doc->m_password)
			{
				const CSoundFile &sf = doc->m_modDoc.GetrSoundFile();
				ar(sf);
				for(INSTRUMENTINDEX i = 0; i <= sf.GetNumInstruments(); i++)
				{
					if(sf.Instruments[i])
						ar(*sf.Instruments[i]);
					else
						ar(ModInstrument());
				}
				source->Write("!OK!");
				source->Write(sso.str());
			} else
			{
				source->Write("403!");
			}
		} else
		{
			source->Write("404!");
		}
		source->Write(sso.str());
	}
	OutputDebugStringA(msg.c_str());
}


CollabClient::CollabClient(const std::string &server, const std::string &port, std::shared_ptr<Listener> listener)
	: m_socket(io_service)
	, m_listener(listener)
{
	asio::ip::tcp::resolver resolver(io_service);
	m_endpoint_iterator = resolver.resolve({ server, port });
}


void CollabClient::Connect()
{
	/*auto that = shared_from_this();
	asio::async_connect(m_socket, m_endpoint_iterator,
		[that](std::error_code ec, asio::ip::tcp::resolver::iterator)
	{
		if(!ec)
		{
			that->m_connection = std::make_shared<CollabConnection>(std::move(that->m_socket), that);
			that->m_connection->Read();
		}
	});*/
	std::error_code ec;
	asio::connect(m_socket, m_endpoint_iterator, ec);
	if(!ec)
	{
		m_connection = std::make_shared<CollabConnection>(std::move(m_socket), shared_from_this());
		m_connection->Read();
	}

	IOService::Run();
}


void CollabClient::Write(const std::string &msg)
{
	m_connection->Write(msg);
}


void CollabClient::Receive(CollabConnection *source, const std::string &msg)
{
	if(auto listener = m_listener.lock())
	{
		listener->Receive(source, msg);
	}
}


}

OPENMPT_NAMESPACE_END
