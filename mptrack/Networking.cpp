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
#include "Mainfrm.h"
#include "TrackerSettings.h"
#include "../soundlib/plugins/PlugInterface.h"
#include "AudioCriticalSection.h"
#include "../common/version.h"
#include <cereal/cereal.hpp>
#include <iostream>

#define MPT_SIMULATE_NETWORK_LAG

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


void IOService::Stop()
{
	ioService = nullptr;
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


RemoteCollabConnection::RemoteCollabConnection(asio::ip::tcp::socket socket, std::shared_ptr<Listener> listener, bool isOnServer)
	: CollabConnection(listener)
	, m_socket(std::move(socket))
	, m_strand(io_service)
	, m_isOnServer(isOnServer)
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
				break;
			}

			MsgHeader header;
			MPT_ASSERT(length == sizeof(MsgHeader));
			std::memcpy(&header, that->m_inMessage.data(), sizeof(MsgHeader));
			that->m_inMessage.resize(header.compressedSize);
			that->m_origSize = header.origSize;

			//Log("Trying to read %d (%d available)", header.compressedSize, that->m_socket.available());
			asio::read(that->m_socket, asio::buffer(&that->m_inMessage[0], header.compressedSize), ec);
			if(ec)
			{
				break;
			}
			if(!that->Parse())
			{
				break;
			}
		}
		if(that->m_isOnServer && collabServer)
		{
			std::stringstream sso;
			cereal::BinaryOutputArchive ar(sso);
			ar(UserQuitMsg);
			collabServer->Receive(that, sso);
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
		decompressedStream.write(outBuf, sizeof(outBuf) - strm.avail_out);
	} while(strm.avail_out == 0);
	if(auto listener = m_listener.lock())
	{
		return listener->Receive(shared_from_this(), decompressedStream);
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
	//m_strand.post(bind(&RemoteCollabConnection::WriteImpl, this, ))
	MPT_LOCK_GUARD<mpt::mutex> lock(m_mutex);
	asio::write(m_socket, asio::buffer(message.c_str(), message.size()));
	/*auto that = std::static_pointer_cast<RemoteCollabConnection>(shared_from_this());
	m_strand.post([that, message]()
	{
		that->m_outMessages.push_back({ message, message.size(), 0 });
		if(that->m_outMessages.size() > 1)
		{
			// Outstanding async_write
			return;
		}
		that->WriteImpl();
	});*/
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
	const Message &message = that->m_outMessages.front();
	asio::async_write(m_socket, asio::buffer(message.msg.data(), message.remain),
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
		modDoc.m_collabNames[client->GetConnection()->m_id] = client->GetConnection()->m_userName;
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
			that->m_connections.push_back(std::make_shared<RemoteCollabConnection>(std::move(that->m_socket), that, true));
			that->m_connections.back()->Read();
			that->StartAccept();
		}
	});
	IOService::Run();
}


bool CollabServer::Receive(std::shared_ptr<CollabConnection> source, std::stringstream &inMsg)
{
	// Handle messages being received by the server
	cereal::BinaryInputArchive inArchive(inMsg);
	NetworkMessage type;

	inArchive(type);
	//Log(std::string((char*)&type, ((char*)&type)+4).c_str());

	std::ostringstream sso;
	cereal::BinaryOutputArchive ar(sso);

	switch(type)
	{
	case ListMsg:
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
		break;
	}

	case ConnectMsg:
	{
		JoinMsg join;
		inArchive(join);
		CModDoc *modDoc = reinterpret_cast<CModDoc *>(join.id);
		if(m_documents.count(modDoc))
		{
			auto &doc = m_documents.at(modDoc);
			if(mpt::ToUnicode(mpt::CharsetUTF8, join.password) == doc.m_password)
			{
				bool isCollaborator = join.accessType == JoinMsg::ACCESS_COLLABORATOR;
				int &current = isCollaborator ? doc.m_collaborators : doc.m_spectators;
				int &maximum = isCollaborator ? doc.m_maxCollaborators : doc.m_maxSpectators;

				if(current < maximum)
				{
					CSoundFile &sndFile = modDoc->GetSoundFile();
					sndFile.SaveMixPlugins();
					ar(ConnectOKMsg, join.accessType, sndFile, mpt::ToCharset(mpt::CharsetUTF8, modDoc->GetTitle()), source->m_id);
					doc.m_connections.push_back(source);
					current++;
					source->m_modDoc = modDoc;
					source->m_userName = mpt::ToUnicode(mpt::CharsetUTF8, join.userName);
					source->m_accessType = join.accessType;

					ar(modDoc->m_collabEditPositions);

					uint32 numAnnotations = mpt::saturate_cast<uint32>(modDoc->m_collabAnnotations.size());
					ar(numAnnotations);
					for(const auto &anno : modDoc->m_collabAnnotations)
					{
						AnnotationMsg msg{ anno.first.pattern, anno.first.row, anno.first.channel, anno.first.column, mpt::ToCharset(mpt::CharsetUTF8, anno.second) };
						ar(msg);
					}
					uint32 numNames = mpt::saturate_cast<uint32>(doc.m_connections.size());
					ar(numNames);
					for(const auto &conn : doc.m_connections)
					{
						ar(conn->m_id, conn->m_userName);
					}

					ar(modDoc->m_collabLockedPatterns);

					{
						// Tell everyone that this user joined
						std::ostringstream ssoj;
						cereal::BinaryOutputArchive arj(ssoj);
						arj(UserJoinedMsg, source->m_id, join.userName);
						SendToAll(doc, ssoj);
					}
				} else
				{
					ar(NoMoreClientsMsg, join.accessType);
				}
			}
			else
			{
				ar(WrongPasswordMsg);
			}
		}
		else
		{
			ar(DocNotFoundMsg);
		}
		source->Write(sso.str());
		break;
	}

	case ChatMsg:
	{
		auto *modDoc = source->m_modDoc;
		if(m_documents.count(modDoc))
		{
			mpt::ustring message;
			inArchive(message);
			// Send back to all clients
			ar(type, source->m_id, message);
			SendToAll(m_documents.at(modDoc), sso);
		}
		break;
	}

	case UserQuitMsg:
	{
		// A collaborator or spectator leaves the document
		auto *modDoc = source->m_modDoc;
		if(!m_documents.count(modDoc))
		{
			break;
		}
		auto &connections = m_documents.at(modDoc).m_connections;
		auto conn = std::find(connections.begin(), connections.end(), source);
		if(conn != connections.end())
		{
			connections.erase(conn);

			if(m_documents.count(modDoc))
			{
				auto &doc = m_documents.at(modDoc);
				switch(source->m_accessType)
				{
				case JoinMsg::ACCESS_COLLABORATOR:
					if(doc.m_collaborators > 0) doc.m_collaborators--;
					break;
				case JoinMsg::ACCESS_SPECTATOR:
					if(doc.m_spectators > 0) doc.m_spectators--;
					break;
				}
				ar(type, source->m_id);
				SendToAll(m_documents.at(modDoc), sso);
			}
		}
		conn = std::find(m_connections.begin(), m_connections.end(), source);
		if(conn != m_connections.end())
		{
			m_connections.erase(conn);
		}
		return false;
	}

	default:
	{
		// Document operation
		if(source->m_accessType != JoinMsg::ACCESS_COLLABORATOR)
		{
			break;
		}

		auto *modDoc = source->m_modDoc;
		if(m_documents.count(modDoc))
		{
			ar(type, source->m_id);
			auto &doc = m_documents.at(modDoc);
			auto &sndFile = modDoc->GetSoundFile();
			switch(type)
			{
			case PatternTransactionMsg:
			{
				PatternEditMsg patMsg;
				inArchive(patMsg);
				CriticalSection cs;
				if(sndFile.Patterns.IsValidPat(patMsg.pattern))
				{
					patMsg.Apply(sndFile.Patterns[patMsg.pattern]);
					// Send back to all clients
					ar(patMsg);
					SendToAll(doc, sso);
				}
				break;
			}

			case SamplePropertyTransactionMsg:
			{
				SamplePropertyEditMsg msg;
				inArchive(msg);
				if(msg.id > 0 && msg.id <= sndFile.GetNumSamples())
				{
					CriticalSection cs;
					msg.Apply(sndFile, msg.id);
					// Send back to all clients
					ar(msg);
					SendToAll(doc, sso);
				}
				break;
			}

			case SampleDataTransactionMsg:
			{
				SamplePropertyEditMsg msg;
				cereal::size_type modificationStart, modificationLength;
				inArchive(msg, modificationStart, modificationLength);
				std::vector<int8> data(static_cast<size_t>(modificationLength));
				inArchive(cereal::binary_data(data.data(), static_cast<size_t>(modificationLength)));

				// Send back to all clients
				ar(msg, modificationStart, modificationLength, cereal::binary_data(data.data(), data.size()));
				SendToAll(doc, sso);
				break;
			}

			case InstrumentTransactionMsg:
			{
				InstrumentEditMsg msg;
				inArchive(msg);
				// Send back to all clients
				ar(msg);
				SendToAll(doc, sso);
				break;
			}

			case EnvelopeTransactionMsg:
			{
				INSTRUMENTINDEX id;
				EnvelopeType envType;
				InstrumentEnvelope env;
				inArchive(id, envType, env);
				if(id > 0)
				{
					// Send back to all clients
					ar(id, envType, env);
					SendToAll(doc, sso);
				}
				break;
			}

			case PluginDataTransactionMsg:
			{
				PluginEditMsg plugMsg;
				inArchive(plugMsg);
				CriticalSection  cs;
				if(sndFile.m_MixPlugins[plugMsg.plugin].pMixPlugin)
				{
					for(auto &param : plugMsg.params)
					{
						sndFile.m_MixPlugins[plugMsg.plugin].pMixPlugin->SetParameter(param.param, param.value);
					}
				}
				ar(plugMsg);
				SendToAll(doc, sso);
				break;
			}

			case PatternResizeMsg:
			{
				PATTERNINDEX pat;
				ROWINDEX rows;
				bool atEnd;
				inArchive(pat, rows, atEnd);
				ar(pat, rows, atEnd);
				SendToAll(doc, sso);
				break;
			}

			case SequenceTransactionMsg:
			{
				// Modify order list
				SequenceMsg msg;
				inArchive(msg);
				while(msg.seq >= sndFile.Order.GetNumSequences() && msg.seq < MAX_SEQUENCES)
					sndFile.Order.AddSequence(false);
				if(msg.seq < sndFile.Order.GetNumSequences())
					msg.Apply(sndFile.Order(msg.seq));
				// Send back to clients
				ar(msg);
				SendToAll(doc, sso);
				break;
			}

			case EditPosMsg:
			{
				// Update edit pos of this client
				Networking::SetCursorPosMsg msg;
				inArchive(msg);
				//source->m_editPos = msg;
				// Send back to clients
				ar(source->m_id, msg);
				SendToAll(doc, sso);
				break;
			}

			case KeyCommandMsg:
			{
				// Send key command like "play song" to spectators
				int32 cmd;
				inArchive(cmd);
				ar(cmd);
				SendToAll(doc, sso, true);
				break;
			}

			case SendAnnotationMsg:
			{
				Networking::AnnotationMsg msg;
				inArchive(msg);
				// Send back to clients
				ar(msg);
				SendToAll(doc, sso);
				break;
			}

			case GlobalSettingsMsg:
			{
				GlobalEditMsg msg;
				inArchive(msg);
				// Send back to clients
				ar(msg);
				SendToAll(doc, sso);
				break;
			}

			case ChannelSettingsMsg:
			{
				CHANNELINDEX chn, sourceChn;
				ModChannelSettings settings;
				inArchive(chn, sourceChn, settings);
				// Send back to clients
				ar(chn, sourceChn, settings);
				SendToAll(doc, sso);
				break;
			}

			case PatternLockMsg:
			{
				// Lock or unlock kpattern
				PATTERNINDEX pat;
				bool enable;
				inArchive(pat, enable);

				if(modDoc->m_collabLockedPatterns.count(pat) && enable)
				{
					// Ignore, this pattern is already locked
					break;
				}

				// Not locked yet, send back to clients
				ar(pat, enable);
				SendToAll(doc, sso);
				break;
			}

			case TuningTransactionMsg:
			{
				std::string s;
				inArchive(s);
				// Send back to clients
				ar(s);
				SendToAll(doc, sso);
				break;
			}

			case ReturnValTransactionMsg:
			{
				uint64 handle;
				std::string msg;
				inArchive(handle, msg);
				std::istringstream retMsg(msg);
				cereal::BinaryInputArchive inArchiveRet(retMsg);
				NetworkMessage retType;
				inArchiveRet(retType);
				ar(handle);
				std::string retVal;
				// Request to insert pattern
				switch(retType)
				{
				case ExpandOrShrinkPatternMsg:
				{
					PATTERNINDEX pat;
					bool expand;
					inArchiveRet(pat, expand);
					if(sndFile.Patterns.IsValidPat(pat))
					{
						std::ostringstream ssoRet;
						cereal::BinaryOutputArchive arRet(ssoRet);
						arRet(ExpandOrShrinkPatternMsg, source->m_id, pat, expand);
						SendToAll(doc, ssoRet);
						retVal = mpt::ToString(true);
					} else
					{
						retVal = mpt::ToString(false);

					}
					break;
				}

				case InsertPatternMsg:
				{
					ROWINDEX rows;
					inArchiveRet(rows);
					PATTERNINDEX pat = sndFile.Patterns.InsertAny(rows, true);
					if(pat != PATTERNINDEX_INVALID)
					{
						std::ostringstream ssoRet;
						cereal::BinaryOutputArchive arRet(ssoRet);
						arRet(InsertPatternMsg, source->m_id, pat, rows);
						SendToAll(doc, ssoRet);
					}
					retVal = mpt::ToString(pat);
					break;
				}

				case InsertSampleMsg:
				{
					SAMPLEINDEX smp = modDoc->InsertSample(true);
					retVal = mpt::ToString(smp);
					break;
				}

				case InsertInstrumentMsg:
				{
					INSTRUMENTINDEX ins = modDoc->GetSoundFile().GetNextFreeInstrument();
					if(ins != INSTRUMENTINDEX_INVALID)
					{
						std::ostringstream ssoRet;
						cereal::BinaryOutputArchive arRet(ssoRet);
						arRet(InsertInstrumentMsg, source->m_id, ins);
						SendToAll(doc, ssoRet);
					}
					retVal = mpt::ToString(ins);
					break;
				}

				case ConvertInstrumentsMsg:
				{
					retVal = mpt::ToString(modDoc->ConvertSamplesToInstruments());
					modDoc->UpdateAllViews(UpdateHint().ModType());
					break;
				}

				case RearrangeChannelsMsg:
				{
					std::vector<CHANNELINDEX> newChannels;
					inArchive(newChannels);
					//retVal = modDoc->ReArrangeChannels()
					break;
				}

				}
				// Notify the blocked caller
				ar(std::move(retVal));
				source->Write(sso.str());
				break;
			}
			}
			break;
		}
	}
	}
	return true;
}


void CollabServer::SendToAll(NetworkedDocument &doc, const std::ostringstream &sso, bool onlySpectators)
{
	const std::string s = sso.str();
	for(auto &c : doc.m_connections)
	{
		if(!onlySpectators || c->m_accessType == JoinMsg::ACCESS_SPECTATOR)
		{
			c->Write(s);
		}
	}
}


void CollabClient::Quit()
{
	std::ostringstream sso;
	cereal::BinaryOutputArchive ar(sso);
	ar(UserQuitMsg);
	Write(sso.str());
	m_connection->Close();
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
	std::error_code ec;
	asio::connect(m_socket, m_endpoint_iterator, ec);
	if(ec)
	{
		return false;
	}
	m_connection = std::make_shared<RemoteCollabConnection>(std::move(m_socket), shared_from_this(), false);
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


bool CollabClient::Receive(std::shared_ptr<CollabConnection> source, std::stringstream &msg)
{
#ifdef MPT_SIMULATE_NETWORK_LAG
	Sleep(30);	// simulate lag
#endif // MPT_SIMULATE_NETWORK_LAG
	if(auto listener = m_listener.lock())
	{
		return listener->Receive(source, msg);
	}
	return false;
}


LocalCollabClient::LocalCollabClient(CModDoc &modDoc)
	: CollabClient(modDoc.m_listener, std::make_shared<LocalCollabConnection>(modDoc.m_listener))
{
	m_connection->m_accessType = JoinMsg::ACCESS_COLLABORATOR;
	m_connection->m_modDoc = &modDoc;
}


void LocalCollabClient::Write(const std::string &msg)
{
	std::stringstream ss(msg);
	collabServer->Receive(m_connection, ss);
}


std::string LocalCollabClient::WriteWithResult(const std::string &msg)
{
	m_connection->m_promise = std::promise<std::string>();

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
