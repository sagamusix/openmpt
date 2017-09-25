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
#include "TrackerSettings.h"
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


ClientID CollabConnection::m_nextId = 0;

CollabConnection::CollabConnection(std::shared_ptr<Listener> listener)
	: m_listener(listener)
	, m_modDoc(nullptr)
	, m_id(m_nextId++)
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
	m_userName = TrackerSettings::Instance().defaultArtist;
}


CollabConnection::~CollabConnection()
{
	inflateEnd(&m_strmIn);
	deflateEnd(&m_strmOut);
}


void RemoteCollabConnection::Read()
{
	auto that = std::static_pointer_cast<RemoteCollabConnection>(shared_from_this());
	m_thread = mpt::thread([that]()
	{
		while(that->m_threadRunning)
		{
			that->m_inMessage.resize(sizeof(MsgHeader));
			asio::error_code ec;
			auto length = asio::read(that->m_socket, asio::buffer(&that->m_inMessage[0], sizeof(MsgHeader)), ec);
			if(ec)
			{
				return;
			}

			MsgHeader header;
			MPT_ASSERT(length == sizeof(MsgHeader));
			std::memcpy(&header, that->m_inMessage.data(), sizeof(MsgHeader));
			that->m_inMessage.resize(header.compressedSize);
			that->m_origSize = header.origSize;

			Log("Trying to read %d (%d available)", header.compressedSize, that->m_socket.available());
			asio::read(that->m_socket, asio::buffer(&that->m_inMessage[0], header.compressedSize), ec);
			if(ec)
			{
				return;
			}
			that->Parse();
		}
	});
	return;

/*	m_inMessage.resize(sizeof(MsgHeader));
	m_strand.dispatch([that]()
	{
		asio::async_read(that->m_socket,
			asio::buffer(&that->m_inMessage[0], sizeof(MsgHeader)),
			[that](std::error_code ec, std::size_t length)
		{
			if(!ec)
			{
				MsgHeader header;
				MPT_ASSERT(length == sizeof(MsgHeader));
				std::memcpy(&header, that->m_inMessage.data(), sizeof(MsgHeader));
				that->m_inMessage.resize(header.compressedSize);
				that->m_origSize = header.origSize;

				auto remain = header.compressedSize.get();
				size_t offset = 0;
				while(remain)
				{
					Log("Trying to read %d (%d available)", remain, that->m_socket.available());
					auto read = remain;
					LimitMax(read, that->m_socket.available());
					auto actualRead = asio::read(that->m_socket, asio::buffer(&that->m_inMessage[offset], read));
					remain -= actualRead;
					offset += actualRead;
					MPT_ASSERT(read == actualRead);
				}
				if(that->Parse())
				{
					that->Read();
				}
			}
		});
	});*/
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
	//Log("Writing %d / %d", header.compressedSize.get(), compressedMessage.size());
	std::memcpy(&compressedMessage[0], &header, sizeof(header));

	Send(compressedMessage);
	//Log("Writing done.");
}


std::string CollabConnection::WriteWithResult(const std::string &message)
{
	m_promise = std::promise<std::string>();
	
	std::ostringstream ss;
	cereal::BinaryOutputArchive ar(ss);
	ar(ReturnValTransactionMsg);
	ar(reinterpret_cast<uint64>(&m_promise));
	ar(message);

	auto future = m_promise.get_future();
	Write(ss.str());
	future.wait();
	return future.get();
}


void RemoteCollabConnection::Send(const std::string &message)
{
	//MPT_LOCK_GUARD<mpt::mutex> lock(m_mutex);
	//asio::write(m_socket, asio::buffer(message.c_str(), message.size()));
	auto that = std::static_pointer_cast<RemoteCollabConnection>(shared_from_this());
	m_strand.dispatch([that, message]()
	{
		that->m_outMessages.push_back({ message, message.size(), 0 });
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
	m_threadRunning = false;
	if(m_thread.joinable())
	{
		m_thread.join();
	}
}


void RemoteCollabConnection::WriteImpl()
{
	auto that = std::static_pointer_cast<RemoteCollabConnection>(shared_from_this());
#if 1
	const Message &message = that->m_outMessages.front();
	asio::async_write(m_socket, asio::buffer(message.msg.data() + message.written, std::min(size_t(8192), message.remain)),
		m_strand.wrap([that](std::error_code error, const size_t written)
	{
		if(error)
		{
			std::cerr << "Write Error: " << std::system_error(error).what() << std::endl;
			return;
		}

		Message &message = that->m_outMessages.front();
		message.written += written;
		message.remain -= written;
		if(!message.remain)
		{
			that->m_outMessages.pop_front();
			if(!that->m_outMessages.empty())
			{
				that->WriteImpl();
			}
		} else
		{
			that->WriteImpl();
		}
	}));
#else
	m_strand.dispatch([that]()
	{
		while(!that->m_outMessages.empty())
		{
			const std::string &message = that->m_outMessages.front();

			std::error_code error;
			size_t remain = message.size(), pos = 0;
			while(remain)
			{
				size_t toWrite = std::min(remain, size_t(8192));
				auto written = asio::write(that->m_socket, asio::buffer(message.data() + pos, toWrite), error);
				remain -= written;
				pos += written;
				Log("Remains to write: %d / %d", remain, message.size());
				if(error)
				{
					std::cerr << "Write Error: " << std::system_error(error).what() << std::endl;
					return;
				}
			}
			that->m_outMessages.pop_front();
		}
	});
#endif
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
	auto doc = m_documents.find(&modDoc);
	if(doc == m_documents.end())
	{
		// New document
		auto client = std::make_shared<LocalCollabClient>(modDoc);
		m_documents[&modDoc] = NetworkedDocument(collaborators, spectators, password, client->GetConnection());
		return client;
	} else
	{
		// Update
		doc->second.m_maxCollaborators = collaborators;
		doc->second.m_maxSpectators = spectators;
		doc->second.m_password = password;
		return nullptr;
	}
}


NetworkedDocument CollabServer::GetDocument(CModDoc &modDoc) const
{
	MPT_LOCK_GUARD<mpt::mutex> lock(m_mutex);
	auto doc = m_documents.find(&modDoc);
	if(doc == m_documents.end())
	{
		return NetworkedDocument();
	} else
	{
		return doc->second;
	}
}


void CollabServer::CloseDocument(CModDoc &modDoc)
{
	MPT_LOCK_GUARD<mpt::mutex> lock(m_mutex);
	auto doc = m_documents.find(&modDoc);
	if(doc != m_documents.end())
	{
		// TODO: Close connections for all clients that belong to this document
		for(auto &conn : doc->second.m_connections)
		{
			std::ostringstream sso;
			cereal::BinaryOutputArchive ar(sso);
			ar(QuitMsg);
			conn->Write(sso.str());
		}
	}
	m_documents.erase(&modDoc);
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
	Log(std::string(type.type, 4).c_str());

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
			info.id = reinterpret_cast<uint64>(doc.first);
			info.name = mpt::ToCharset(mpt::CharsetUTF8, doc.first->GetTitle());
			info.collaborators = doc.second.m_collaborators;
			info.maxCollaboratos = doc.second.m_maxCollaborators;
			info.spectators = doc.second.m_spectators;
			info.maxSpectators = doc.second.m_maxSpectators;
			info.password = !doc.second.m_password.empty();
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
		if(m_documents.count(modDoc))
		{
			auto &doc = m_documents.at(modDoc);
			if(mpt::ToUnicode(mpt::CharsetUTF8, join.password) == doc.m_password)
			{
				if(doc.m_collaborators < doc.m_maxCollaborators)
				{
					CSoundFile &sndFile = modDoc->GetrSoundFile();
					sndFile.SaveMixPlugins();
					ar(ConnectOKMsg);
					ar(sndFile);
					ar(mpt::ToCharset(mpt::CharsetUTF8, modDoc->GetTitle()));
					//m_connections.push_back(source);
					doc.m_connections.push_back(source);
					doc.m_collaborators++;
					source->m_modDoc = modDoc;
					source->m_userName = mpt::ToUnicode(mpt::CharsetUTF8, join.userName);

					uint32 numPositions = modDoc->m_collabEditPositions.size();
					ar(numPositions);
					for(const auto &editPos : modDoc->m_collabEditPositions)
					{
						//ar(EditPosMsg);
						ar(editPos.first);
						SetCursorPosMsg msg{ editPos.second.sequence, editPos.second.order, editPos.second.pattern, editPos.second.row, editPos.second.channel, editPos.second.column };
						ar(msg);
					}
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
		// TODO: Check here if the source is a collaborator or spectator?
		auto *modDoc = source->m_modDoc;
		if(m_documents.count(modDoc))
		{
			ar(type);
			auto &doc = m_documents.at(modDoc);
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
					ar(patMsg);
					const std::string s = sso.str();
					for(auto &c : doc.m_connections)
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
					ar(msg);
					const std::string s = sso.str();
					for(auto &c : doc.m_connections)
					{
						c->Write(s);
					}
				}
			} else if(type == SampleDataTransactionMsg)
			{
				cereal::size_type size;
				inArchive >> size;
				std::vector<int8> data(static_cast<size_t>(size));
				inArchive(cereal::binary_data(data.data(), static_cast<size_t>(size)));
				//ar(make_size_tag(size));
				//str.resize(static_cast<std::size_t>(size));
				//ar(cereal::binary_data(const_cast<CharT *>(str.data()), static_cast<std::size_t>(size) * sizeof(CharT)));
			} else if(type == InstrumentTransactionMsg)
			{
				INSTRUMENTINDEX id;
				ModInstrument instr;
				inArchive >> id;
				inArchive >> instr;
				if(id > 0)
				{
					// Send back to all clients
					ar(id);
					ar(instr);
					const std::string s = sso.str();
					for(auto &c : doc.m_connections)
					{
						c->Write(s);
					}
				}
			} else if(type == EnvelopeTransactionMsg)
			{
				INSTRUMENTINDEX id;
				EnvelopeType envType;
				InstrumentEnvelope env;
				inArchive >> id;
				inArchive >> envType;
				inArchive >> env;
				if(id > 0)
				{
					// Send back to all clients
					ar(id);
					ar(envType);
					ar(env);
					const std::string s = sso.str();
					for(auto &c : doc.m_connections)
					{
						c->Write(s);
					}
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
				for(auto &c : doc.m_connections)
				{
					c->Write(s);
				}
			} else if(type == PatternResizeMsg)
			{
				PATTERNINDEX pat;
				ROWINDEX rows;
				bool atEnd;
				inArchive >> pat;
				inArchive >> rows;
				inArchive >> atEnd;
				ar(pat);
				ar(rows);
				ar(atEnd);
				const std::string s = sso.str();
				for(auto &c : doc.m_connections)
				{
					c->Write(s);
				}
			}
			else if(type == SequenceTransactionMsg)
			{
				SequenceMsg msg;
				inArchive >> msg;
				while(msg.seq >= sndFile.Order.GetNumSequences() && msg.seq < MAX_SEQUENCES)
					sndFile.Order.AddSequence(false);
				if(msg.seq < sndFile.Order.GetNumSequences())
					msg.Apply(sndFile.Order(msg.seq));
				// Send back to clients
				ar(msg);
				const std::string s = sso.str();
				for(auto &c : doc.m_connections)
				{
					c->Write(s);
				}
			} else if(type == EditPosMsg)
			{
				// Update edit pos of this client
				Networking::SetCursorPosMsg msg;
				inArchive >> msg;
				//source->m_editPos = msg;
				// Send back to clients
				ar(source->m_id);
				ar(msg);
				const std::string s = sso.str();
				for(auto &c : doc.m_connections)
				{
					c->Write(s);
				}
			} else if(type == ReturnValTransactionMsg)
			{
				uint64 handle;
				std::string msg;
				inArchive >> handle;
				inArchive >> msg;
				std::istringstream retMsg(msg);
				cereal::BinaryInputArchive inArchiveRet(retMsg);
				NetworkMessage retType;
				inArchiveRet >> retType;
				ar(handle);
				std::string retVal;
				// Request to insert pattern
				if(retType == InsertPatternMsg)
				{
					ROWINDEX rows;
					inArchiveRet >> rows;
					PATTERNINDEX pat = sndFile.Patterns.InsertAny(rows, true);
					if(pat != PATTERNINDEX_INVALID)
					{
						std::ostringstream ssoRet;
						cereal::BinaryOutputArchive arRet(ssoRet);
						arRet(InsertPatternMsg);
						arRet(pat);
						arRet(rows);
						const std::string s = ssoRet.str();
						for(auto &c : doc.m_connections)
						{
							c->Write(s);
						}
					}
					retVal = mpt::ToString(pat);
				} else if(retType == InsertSampleMsg)
				{
					SAMPLEINDEX smp = modDoc->InsertSample(true);
					retVal = mpt::ToString(smp);
				} else if(retType == InsertInstrumentMsg)
				{
					INSTRUMENTINDEX ins = modDoc->GetrSoundFile().GetNextFreeInstrument();
					if(ins != INSTRUMENTINDEX_INVALID)
					{
						std::ostringstream ssoRet;
						cereal::BinaryOutputArchive arRet(ssoRet);
						arRet(InsertInstrumentMsg);
						arRet(ins);
						const std::string s = ssoRet.str();
						for(auto &c : doc.m_connections)
						{
							c->Write(s);
						}
					}
					retVal = mpt::ToString(ins);
				}
				// Notify the blocked caller
				ar(std::move(retVal));
				source->Write(sso.str());
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


std::string RemoteCollabClient::WriteWithResult(const std::string &msg)
{
	return m_connection->WriteWithResult(msg);
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


std::string LocalCollabClient::WriteWithResult(const std::string &msg)
{
	std::stringstream ss;
	cereal::BinaryOutputArchive ar(ss);
	ar(ReturnValTransactionMsg);
	ar(reinterpret_cast<uint64>(&m_connection->m_promise));
	ar(msg);

	std::string result;
	auto future = m_connection->m_promise.get_future();
	collabServer->Receive(m_connection, ss);
	//future.wait();
	return future.get();
}



}

OPENMPT_NAMESPACE_END
