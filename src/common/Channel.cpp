#include "Channel.hpp"

Channel::Channel(unsigned int channelId, Peer* serverPeer) 
{
    if (channelId != 0 || serverPeer != NULL) //Avoid creation by map[]
    {
        this->channelId = channelId;
        this->serverPeer = serverPeer;
        if(serverPeer)
            AddPeer(serverPeer);
        serverEstimatedStreamRate = 0;

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
    peerList[peer->GetID()] = PeerData(peer);
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

time_t Channel::GetCreationTime()
{
    return creationTime;
}
//* texto original ...
vector<PeerData*> Channel::SelectPeerList(Strategy* strategy, Peer* srcPeer, unsigned int peerQuantity)
{
    vector<PeerData*> allPeers, selectedPeers;
    
    for (map<string, PeerData>::iterator i = peerList.begin(); i != peerList.end(); i++)
        if (srcPeer->GetID() != i->second.GetPeer()->GetID())
            allPeers.push_back(&(i->second));

    if (peerList.size() <= peerQuantity)
        return allPeers;
    else
    {
        strategy->Execute(&allPeers, srcPeer, peerQuantity);
        selectedPeers.insert(selectedPeers.begin(),allPeers.begin(),allPeers.begin()+peerQuantity);
        return selectedPeers;
    }
}


//************************************ codigo eliseu 02 ************************************************** //
// com este metodo, a topologia tera o servidor atendendo apenas a um cliente x e os demais liagados a x
/*
vector<PeerData*> Channel::SelectPeerList(Strategy* strategy, Peer* srcPeer, unsigned int peerQuantity)
{
    vector<PeerData*> allPeers, selectedPeers;

    for (map<string, PeerData>::iterator i = peerList.begin(); i != peerList.end(); i++)
        if (srcPeer->GetID() != i->second.GetPeer()->GetID()){
            allPeers.push_back(&(i->second));
             cout <<"allpeers tem um cara com mode "<<(int)i->second.GetMode()<<" e String "<<(string)i->first<<endl;
        }

    //eliseu
    cout<<"*******************************************************"<<endl;
    cout<<"temos no total "<<(unsigned int) peerList.size()<<endl;

    // uso 3 porque peerList tem um elemento a mais que allPeers.
    // Primeiro o bootstrap insere o novo em peerList. Finalmente, gera allPeers para o novo sem ele
    // Além disso, peerList ordena pelo campo IP, que é a string do map. Assim, o servidor é sempre o último
    if (peerList.size() < 3)
    {
        cout <<"retornando todos os peers"<<endl;
        return allPeers;
    }
    else
    {

        //if (peerList.size() <= peerQuantity)
           peerQuantity = peerList.size()-2;
        cout <<"entrei no else e teremos "<<peerQuantity<<" parceiros"<<endl;
        //strategy->Execute(&allPeers, srcPeer, peerQuantity);
        selectedPeers.insert(selectedPeers.begin(),allPeers.begin(),allPeers.begin()+peerQuantity);

        //codigo eliseu

        for (unsigned int i = 0 ; i < selectedPeers.size(); i++)
           cout <<"selecionamos alguem de modo "<<(int)selectedPeers[i]->GetMode()<<endl;
        cout<<"*******************************************************"<<endl;

        return selectedPeers;
    }
}
*/

unsigned int Channel::GetPeerListSize()
{
    return peerList.size();
}

void Channel::CheckActivePeers()
{
    vector<string> deletedPeer;
    for (map<string,PeerData>::iterator i = peerList.begin(); i != peerList.end(); i++) 
    {
        i->second.DecTTLIn();
        if (i->second.GetTTLIn() <= 0)
            deletedPeer.push_back(i->first);
    }
    for (vector<string>::iterator peerId = deletedPeer.begin(); peerId < deletedPeer.end(); peerId++)
        RemovePeer(*peerId);
}

void Channel::PrintPeerList()
{
    cout<<"Channel ["<<channelId<<"] Tip["<<serverNewestChunkID<<"] Rate["<<serverEstimatedStreamRate<<"] Peer List:"<<endl;
    for (map<string,PeerData>::iterator i = peerList.begin(); i != peerList.end(); i++)
        cout<<"PeerID: "<<i->first<<" Mode: "<<(int)i->second.GetMode()<<" TTL: "<<i->second.GetTTLIn()<<endl;
}

FILE* Channel::GetPerformanceFile()
{
    return performanceFile;
}

FILE* Channel::GetOverlayFile()
{
    return overlayFile;
}
