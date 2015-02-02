#include "Channel.hpp"

Channel::Channel(unsigned int channelId, Peer* serverPeer)
: channel_Sub_List_Mutex (new boost::mutex()),
  channel_Sub_Candidates_Mutex (new boost::mutex())
{
    if (channelId != 0 || serverPeer != NULL) //Avoid creation by map[]
    {
        this->channelId = channelId;
        this->serverPeer = serverPeer;
        if(serverPeer)
            AddPeer(serverPeer);
        serverEstimatedStreamRate = 0;
        this->channelMode = MODE_NORMAL;
        this->maxPeer_ChannelSub = MAXPEER_CHANNELSUB;

        //Logging
        struct tm * timeinfo;
        char timestr[20];
        time(&creationTime);
        timeinfo = localtime(&creationTime);
        strftime (timestr,20,"%Y%m%d%H%M",timeinfo);
        string logFilename = "log-";
        logFilename += boost::lexical_cast<string>(channelId) + "-";
        logFilename += serverPeer->GetIP() + "_" + serverPeer->GetPort() + "-";
        logFilename += timestr;
        logFilename += "-";
        string logFilenamePerf = logFilename + "perf.txt";
        string logFilenameOverlay = logFilename + "overlay.txt";
        performanceFile = fopen(logFilenamePerf.c_str(),"w");
        overlayFile = fopen(logFilenameOverlay.c_str(),"w");
    } 
}

void Channel::SetServerNewestChunkID(ChunkUniqueID serverNewestChunkID)
{
    this->serverNewestChunkID = serverNewestChunkID;
}

ChunkUniqueID Channel::GetServerNewestChunkID()
{
    return serverNewestChunkID;
}

void Channel::SetServerEstimatedStreamRate(int serverEstimatedStreamRate)
{
    this->serverEstimatedStreamRate = serverEstimatedStreamRate;
}

int Channel::GetServerEstimatedStreamRate()
{
    return serverEstimatedStreamRate;
}

Peer* Channel::GetServer()
{
    return serverPeer;
}

Peer* Channel::GetPeer(Peer* peer)
{
    if (peerList.count(peer->GetID()) > 0)
        return peerList[peer->GetID()].GetPeer();
    return NULL;
}

bool Channel::HasPeer(Peer* peer)
{
    if (GetPeer(peer))
        return true;
    else
        return false;
}

void Channel::AddPeer(Peer* peer)
{
	if (!this->AddPeerChannel(peer))                    // tenta inserir enquanto sub canais permitire
	{
        if(Creat_New_ChannelSub())                      // tenta criar novo sub canal
        	this->AddPeerChannel(peer);
        else
        {
        	cout<<"Subcanais Esgotados. Inserindo ["<<peer->GetID()<<"] no canal principal"<<endl;
        	peerList[peer->GetID()] = PeerData(peer);    // insere no canal prinipal
        }
	}
}

bool Channel::AddPeerChannel(Peer* peer)
{
	if (channelMode == MODE_NORMAL)
	{
		peerList[peer->GetID()] = PeerData(peer);
		return true;
	}
	else
	{
		//tenta inserir em sub canal
		boost::mutex::scoped_lock channelSubListLock(*channel_Sub_List_Mutex);

		for (map<string, SubChannelData>::iterator i = channel_Sub_List.begin(); i != channel_Sub_List.end(); i++)
			if (this->GetPeerListSizeChannel_Sub(i->second.GetchannelId_Sub())< this->maxPeer_ChannelSub)
			{
				peerList[peer->GetID()] = PeerData(peer);
				peerList[peer->GetID()].SetChannelId_Sub(i->second.GetchannelId_Sub());
				channelSubListLock.unlock();
				return true;
	        }
		channelSubListLock.unlock();
	}
	return false;
}


void Channel::RemovePeer(Peer* peer)
{
    peerList.erase(peer->GetID());
}

void Channel::RemovePeer(string peerId)
{
    peerList.erase(peerId);
}

PeerData& Channel::GetPeerData(Peer* peer)
{
    return peerList[peer->GetID()];
}

void Channel::SetChannelMode(ChannelModes channelMode)
{
	this->channelMode = channelMode;
}
ChannelModes Channel::GetChannelMode()
{
	return channelMode;
}

void Channel::SetmaxPeer_ChannelSub(int unsigned maxpeerChannelSub)
{
	maxPeer_ChannelSub = maxpeerChannelSub;
}


//***************************************************************************
bool Channel::Creat_New_ChannelSub()
{
	//obtem novo servidor auxiliar
	string* peerServerAuxNew = NULL;
	boost::mutex::scoped_lock channelSubCandidatesLock(*channel_Sub_Candidates_Mutex);
	if (server_Sub_Candidates.size() > 0)
		peerServerAuxNew =  new string(*(server_Sub_Candidates.begin()));
	server_Sub_Candidates.erase(server_Sub_Candidates.begin());
	channelSubCandidatesLock.unlock();

	if (peerServerAuxNew)
	{
		unsigned channelID_New = 0;
		boost::mutex::scoped_lock channelSubListLock(*channel_Sub_List_Mutex);
		for (map<string, SubChannelData>::iterator i = channel_Sub_List.begin(); i != channel_Sub_List.end(); i++)
			if (channelID_New < this->GetPeerListSizeChannel_Sub(i->second.GetchannelId_Sub()))
				channelID_New = this->GetPeerListSizeChannel_Sub(i->second.GetchannelId_Sub());
			channelID_New ++;
		channel_Sub_List[*peerServerAuxNew] = SubChannelData(channelId, channelID_New, peerList[*peerServerAuxNew].GetPeer());
		channelSubListLock.unlock();
		return true;
	}
	return false;
}

void Channel::Remove_ChannelSub(string* source)
{
	boost::mutex::scoped_lock channelSubListLock(*channel_Sub_List_Mutex);

	channelSubListLock.unlock();
}
string* Channel::GetServerSub()
{
	string* a;
	boost::mutex::scoped_lock channelSubCandidatesLock(*channel_Sub_Candidates_Mutex);
	channelSubCandidatesLock.unlock();
	return a;
}

time_t Channel::GetCreationTime()
{
    return creationTime;
}
/* ECM ****
 * Agora, fazemos a busca considerando qual subcanal o peer está.
 * Esse procedimento já trata, por si só, o flash crowd. Isso porque,
 * caso não esteja em flash crowd, todos os peers channelId_Sub == 0
 */
vector<PeerData*> Channel::SelectPeerList(Strategy* strategy, Peer* srcPeer, unsigned int peerQuantity)
{
    vector<PeerData*> allPeers, selectedPeers;
    int unsigned channelId_Sub = peerList[srcPeer->GetID()].GetChannelId_Sub();
    
    for (map<string, PeerData>::iterator i = peerList.begin(); i != peerList.end(); i++)
        if (srcPeer->GetID() != i->second.GetPeer()->GetID() && channelId_Sub== i->second.GetChannelId_Sub())
            allPeers.push_back(&(i->second));
    if (this->GetPeerListSizeChannel_Sub(channelId_Sub) <= peerQuantity)
        return allPeers;
    else
    {
        strategy->Execute(&allPeers, srcPeer, peerQuantity);
        selectedPeers.insert(selectedPeers.begin(),allPeers.begin(),allPeers.begin()+peerQuantity);
        return selectedPeers;
    }
}

unsigned int Channel::GetPeerListSize()
{
    return peerList.size();
}
//ECM
unsigned int Channel::GetPeerListSizeChannel_Sub(unsigned int channelId_Sub)
{
	unsigned int count = 0;
	for (map<string, PeerData>::iterator i = peerList.begin(); i != peerList.end(); i++)
	{
		if (channelId_Sub == i->second.GetChannelId_Sub())
			count++;
	}
	return count;
}

void Channel::CheckActivePeers()
{
    vector<string> deletedPeer;
    for (map<string,PeerData>::iterator i = peerList.begin(); i != peerList.end(); i++) 
    {
        i->second.DecTTLChannel();
        if (i->second.GetTTLChannel() <= 0)
            deletedPeer.push_back(i->first);
    }
    for (vector<string>::iterator peerId = deletedPeer.begin(); peerId < deletedPeer.end(); peerId++)
        RemovePeer(*peerId);
}

void Channel::PrintPeerList()
{
    cout<<"Channel ["<<channelId<<"] Tip["<<serverNewestChunkID<<"] Rate["<<serverEstimatedStreamRate<<"] Peer List:"<<endl;
    for (map<string,PeerData>::iterator i = peerList.begin(); i != peerList.end(); i++)
        cout<<"PeerID: "<<i->first<<" Mode: "<<(int)i->second.GetMode()<<" TTL: "<<i->second.GetTTLChannel()<<endl;
}

FILE* Channel::GetPerformanceFile()
{
    return performanceFile;
}

FILE* Channel::GetOverlayFile()
{
    return overlayFile;
}
