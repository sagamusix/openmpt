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
#include "../soundlib/plugins/PlugInterface.h"
#include "AudioCriticalSection.h"
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


CollabConnection::CollabConnection(std::shared_ptr<Listener> listener)
	: m_listener(listener)
	, m_modDoc(nullptr)
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


RemoteCollabConnection::RemoteCollabConnection(asio::ip::tcp::socket socket, std::shared_ptr<Listener> listener)
	: CollabConnection(listener)
	, m_socket(std::move(socket))
	, m_strand(io_service)
{
}


LocalCollabConnection::LocalCollabConnection(std::shared_ptr<Listener> listener)
	: CollabConnection(listener)
{
}


CollabConnection::~CollabConnection()
{
	inflateEnd(&m_strmIn);
	deflateEnd(&m_strmOut);
}


void RemoteCollabConnection::Read()
{
	auto that = std::static_pointer_cast<RemoteCollabConnection>(shared_from_this());
	m_inMessage.resize(sizeof(MsgHeader));
	asio::async_read(m_socket,
		asio::buffer(&m_inMessage[0], sizeof(MsgHeader)),
		[that](std::error_code ec, std::size_t length)
	{
		if(!ec)
		{
			MsgHeader header;
			MPT_ASSERT(length == sizeof(MsgHeader));
			std::memcpy(&header, that->m_inMessage.data(), sizeof(MsgHeader));
			that->m_inMessage.resize(header.compressedSize);
			that->m_origSize = header.origSize;

			if(asio::read(that->m_socket, asio::buffer(&that->m_inMessage[0], header.compressedSize)) >= header.compressedSize)
			{
				if(that->Parse())
				{
					that->Read();
				}
			}
		}
	});
}


bool CollabConnection::Parse()
{
	std::string decompressedMessage;
	decompressedMessage.reserve(m_origSize);
	std::stringstream decompressedStream(decompressedMessage);
	auto &strm = m_strmIn;
	strm.avail_in = m_inMessage.size();
	strm.next_in = (Bytef *)m_inMessage.data();
	char outBuf[4096];
	do
	{
		strm.avail_out = sizeof(outBuf);
		strm.next_out = reinterpret_cast<Bytef *>(outBuf);
		inflate(&strm, Z_FINISH);
		//decompressedMessage.insert(decompressedMessage.size(), outBuf, sizeof(outBuf) - strm.avail_out);
		decompressedStream.write(outBuf, sizeof(outBuf) - strm.avail_out);
	} while(strm.avail_out == 0);
	if(auto listener = m_listener.lock())
	{
		listener->Receive(shared_from_this(), decompressedStream);
		return true;
	}
	return false;
}


void LocalCollabConnection::Send(const std::string &msg)
{
	m_inMessage = msg.substr(sizeof(MsgHeader));
	m_origSize = reinterpret_cast<const MsgHeader *>(msg.data())->origSize;
	Parse();
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

	Send(compressedMessage);
}


void RemoteCollabConnection::Send(const std::string &message)
{
	auto that = std::static_pointer_cast<RemoteCollabConnection>(shared_from_this());
	m_strand.dispatch([that, message]()
	{
		that->m_outMessages.push_back(message);
		if(that->m_outMessages.size() > 1)
		{
			// Outstanding async_write
			return;
		}
		that->WriteImpl();
	});
}


RemoteCollabConnection::~RemoteCollabConnection()
{
	Close();
}


void RemoteCollabConnection::Close()
{
	m_socket.close();
}


void RemoteCollabConnection::WriteImpl()
{
	const std::string &message = m_outMessages.front();
	auto that = std::static_pointer_cast<RemoteCollabConnection>(shared_from_this());
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
	, m_port(DEFAULT_PORT)
{
}


CollabServer::~CollabServer()
{
}


std::shared_ptr<LocalCollabClient> CollabServer::AddDocument(CModDoc &modDoc, int collaborators, int spectators, const mpt::ustring &password)
{
	MPT_LOCK_GUARD<mpt::mutex> lock(m_mutex);
	auto client = std::make_shared<LocalCollabClient>(modDoc);
	m_documents.insert(NetworkedDocument(modDoc, collaborators, spectators, password, client->GetConnection()));
	return client;
}


void CollabServer::CloseDocument(CModDoc &modDoc)
{
	MPT_LOCK_GUARD<mpt::mutex> lock(m_mutex);
	// TODO: Close connections for all clients that belong to this document
	m_documents.erase(NetworkedDocument(modDoc));
}


void CollabServer::SendMessage(CModDoc &modDoc, const std::string msg)
{
	auto doc = m_documents.find(NetworkedDocument(modDoc));
	if(doc != m_documents.end())
	{
		// TODO
	}
}


void CollabServer::StartAccept()
{
	auto that = shared_from_this();
	m_acceptor.async_accept(m_socket,
		[that](std::error_code ec)
	{
		if(!ec)
		{
			that->m_connections.push_back(std::make_shared<RemoteCollabConnection>(std::move(that->m_socket), that));
			that->m_connections.back()->Read();
			that->StartAccept();
		}
	});
	IOService::Run();
}


void CollabServer::Receive(std::shared_ptr<CollabConnection> source, std::stringstream &inMsg)
{
	cereal::BinaryInputArchive inArchive(inMsg);
	NetworkMessage type;
	inArchive >> type;
	OutputDebugStringA(std::string(type.type, 4).c_str());

	std::ostringstream sso;
	cereal::BinaryOutputArchive ar(sso);

	if(type == ListMsg)
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

		ar(ListMsg);
		ar(welcome);
		source->Write(sso.str());
	} else if(type == ConnectMsg)
	{
		JoinMsg join;
		inArchive >> join;
		CModDoc *modDoc = reinterpret_cast<CModDoc *>(join.id);
		auto doc = m_documents.find(NetworkedDocument(*modDoc));

		if(doc != m_documents.end())
		{
			if(mpt::ToUnicode(mpt::CharsetUTF8, join.password) == doc->m_password)
			{
				if(doc->m_collaborators < doc->m_maxCollaborators)
				{
					CSoundFile &sndFile = doc->m_modDoc.GetrSoundFile();
					sndFile.SaveMixPlugins();
					ar(ConnectOKMsg);
					ar(sndFile);
					ar(mpt::ToCharset(mpt::CharsetUTF8, doc->m_modDoc.GetTitle()));
					m_connections.push_back(source);
					doc->m_connections.push_back(source);
					doc->m_collaborators++;
					source->m_modDoc = modDoc;
				} else
				{
					ar(NoMoreClientsMsg);
				}
			} else
			{
				ar(WrongPasswordMsg);
			}
		} else
		{
			ar(DocNotFoundMsg);
		}
		source->Write(sso.str());
	} else
	{
		// Document operation
		auto *modDoc = source->m_modDoc;
		auto doc = m_documents.find(NetworkedDocument(*source->m_modDoc));
		if(doc != m_documents.end())
		{
			auto &sndFile = modDoc->GetrSoundFile();
			if(type == PatternTransactionMsg)
			{
				PatternEditMsg patMsg;
				inArchive >> patMsg;
				CriticalSection cs;
				if(sndFile.Patterns.IsValidPat(patMsg.pattern))
				{
					patMsg.Apply(sndFile.Patterns[patMsg.pattern]);
					// Send back to all clients
					ar(type);
					ar(patMsg);
					const std::string s = sso.str();
					for(auto &c : doc->m_connections)
					{
						c->Write(s);
					}
				}
			} else if(type == SamplePropertyTransactionMsg)
			{
				SamplePropertyEditMsg msg;
				inArchive >> msg;
				if(msg.id > 0 && msg.id <= sndFile.GetNumSamples())
				{
					CriticalSection cs;
					msg.Apply(sndFile, msg.id);
					// Send back to all clients
					ar(type);
					ar(msg);
					const std::string s = sso.str();
					for(auto &c : doc->m_connections)
					{
						c->Write(s);
					}
				}
			} else if(type == InstrumentTransactionMsg)
			{
				INSTRUMENTINDEX id;
				ModInstrument instr;
				inArchive >> id;
				inArchive >> instr;
				if(id > 0 && id <= sndFile.GetNumInstruments())
				{

				}
			} else if(type == VolEnvTransactioMsg)
			{
				INSTRUMENTINDEX id;
				InstrumentEnvelope env;
				inArchive >> id;
				inArchive >> env;
				if(id > 0 && id <= sndFile.GetNumInstruments())
				{

				}
			} else if(type == PanEnvTransactioMsg)
			{
				INSTRUMENTINDEX id;
				InstrumentEnvelope env;
				inArchive >> id;
				inArchive >> env;
				if(id > 0 && id <= sndFile.GetNumInstruments())
				{

				}
			} else if(type == PitchEnvTransactioMsg)
			{
				INSTRUMENTINDEX id;
				InstrumentEnvelope env;
				inArchive >> id;
				inArchive >> env;
				if(id > 0 && id <= sndFile.GetNumInstruments())
				{

				}
			} else if(type == PluginDataTransactionMsg)
			{
				PluginEditMsg plugMsg;
				inArchive >> plugMsg;
				CriticalSection  cs;
				if(sndFile.m_MixPlugins[plugMsg.plugin].pMixPlugin)
				{
					for(auto &param : plugMsg.params)
					{
						sndFile.m_MixPlugins[plugMsg.plugin].pMixPlugin->SetParameter(param.param, param.value);
					}
				}
				ar(plugMsg);
				const std::string s = sso.str();
				for(auto &c : doc->m_connections)
				{
					c->Write(s);
				}
			}
		}
	}
}


RemoteCollabClient::RemoteCollabClient(const std::string &server, const std::string &port, std::shared_ptr<Listener> listener)
	: CollabClient(listener)
	, m_socket(io_service)
{
	asio::ip::tcp::resolver resolver(io_service);
	m_endpoint_iterator = resolver.resolve({ server, port });
}


bool RemoteCollabClient::Connect()
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
	if(ec)
	{
		return false;
	}
	m_connection = std::make_shared<RemoteCollabConnection>(std::move(m_socket), shared_from_this());
	m_connection->Read();

	IOService::Run();
	return true;
}


void RemoteCollabClient::Write(const std::string &msg)
{
	m_connection->Write(msg);
}


void CollabClient::Receive(std::shared_ptr<CollabConnection> source, std::stringstream &msg)
{
	if(auto listener = m_listener.lock())
	{
		listener->Receive(source, msg);
	}
}


LocalCollabClient::LocalCollabClient(CModDoc &modDoc)
	: CollabClient(modDoc.m_listener, std::make_shared<LocalCollabConnection>(modDoc.m_listener))
{
	m_connection->m_modDoc = &modDoc;
}


void LocalCollabClient::Write(const std::string &msg)
{
	std::stringstream ss(msg);
	collabServer->Receive(m_connection, ss);
}



}

OPENMPT_NAMESPACE_END
