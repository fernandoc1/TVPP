#include "PeerManager.hpp"
//Alterações: Eliseu César Miguel
//13/01/2015
PeerManager::PeerManager()
{
}

unsigned int PeerManager::GetMaxActivePeers()
{
	return maxActivePeers;
}

void PeerManager::SetMaxActivePeers(unsigned int maxActivePeers)
{
	this->maxActivePeers = maxActivePeers;
}

bool PeerManager::AddPeer(Peer* newPeer)
{
	boost::mutex::scoped_lock peerListLock(peerListMutex);
	if (peerList.find(newPeer->GetID()) == peerList.end())
	{
		peerList[newPeer->GetID()] = PeerData(newPeer);
		peerListLock.unlock();
		cout<<"Peer "<<newPeer->GetID()<<" added to PeerList"<<endl;
		return true;
	}
	peerListLock.unlock();
	return false;
}

//ECM
set<string>* PeerManager::GetPeerActiveIn()
{
	return &peerActiveIn;
}
set<string>* PeerManager::GetPeerActiveOut()
{
	return &peerActiveOut;
}

//ECM
map<string, unsigned int>* PeerManager::GetPeerActiveCooldown(set<string>* peerActive)
{
	if (peerActive == &peerActiveIn) return &peerActiveCooldownIn;
	if (peerActive == &peerActiveOut) return &peerActiveCooldownOut;
	return NULL;
}
//ECM

//ECM - efetivamente, insere o par em uma das lista In ou Out
bool PeerManager::ConnectPeer(string peer, set<string>* peerActive, boost::mutex* peerActiveMutex)

{
	map<string, unsigned int>* peerActiveCooldown = this->GetPeerActiveCooldown(peerActive);
	if (peerActiveCooldown->find(peer) == (*peerActiveCooldown).end())
	{
		boost::mutex::scoped_lock peerActiveLock(*peerActiveMutex);
		if (peerActive->size() < maxActivePeers)
		{
			if (peerActive->insert(peer).second)
			{
				peerActiveLock.unlock();
				cout<<"Peer "<<peer<<" connected to PeerActive"<<endl;
				return true;
			}
		}
		peerActiveLock.unlock();
	}
	return false;
}
//ECM

//ECM - retira o par da lista (aqui pode ter problema se ele for da outra lista e for retirado da lista Cooldown
void PeerManager::DisconnectPeer(string peer, set<string>* peerActive, boost::mutex* peerActiveMutex)
{
	map<string, unsigned int>* peerActiveCooldown = this->GetPeerActiveCooldown(peerActive);
	boost::mutex::scoped_lock peerActiveLock(*peerActiveMutex);
	peerActive->erase(peer);
	peerActiveLock.unlock();
	(*peerActiveCooldown)[peer] = PEER_ACTIVE_COOLDOWN;
    cout<<"Peer "<<peer<<" disconnected from PeerActive"<<endl;
}
//ECM

//ECM
void PeerManager::RemovePeer(string peer)
{
	boost::mutex::scoped_lock peerListLock(peerListMutex);
	peerList.erase(peer);
	peerListLock.unlock();
	cout<<"Peer "<<peer<<" removed from PeerList"<<endl;
}
//ECM

//ECM
unsigned int PeerManager::GetPeerActiveSize(set<string>* peerActive, boost::mutex* peerActiveMutex)
{
	boost::mutex::scoped_lock peerActiveLock(*peerActiveMutex);
	unsigned int size = peerActive->size();
	peerActiveLock.unlock();
	return size;
}

//gera total de parceiros somando In e Out sem repeticoes
//esta função perderá sua utilização quando o cliente informar separado ao bootstrap quantos peers in e Out
unsigned int PeerManager::GetPeerActiveSizeTotal()
{
	unsigned int size = this->GetPeerActiveSize(&peerActiveIn, &peerActiveMutexIn);
	boost::mutex::scoped_lock peerActiveInLock(peerActiveMutexIn);
	boost::mutex::scoped_lock peerActiveOutLock(peerActiveMutexOut);
	//Soma não repetidos em Out
	for (set<string>::iterator i = peerActiveOut.begin(); i != peerActiveOut.end(); i++)
	{
		if (peerActiveIn.find(*i) == peerActiveIn.end())
			size++;
	}
	peerActiveInLock.unlock();
	peerActiveOutLock.unlock();
    return size;
}
//ECM

//ECM
bool PeerManager::IsPeerActive(string peer,set<string>* peerActive, boost::mutex* peerActiveMutex)
{
	boost::mutex::scoped_lock peerActiveLock(*peerActiveMutex);
	if (peerActive->find(peer) != peerActive->end())
	{
		peerActiveLock.unlock();
		return true;
	}
	peerActiveLock.unlock();
	return false;
}
//ECM


PeerData* PeerManager::GetPeerData(string peer)
{
	return &peerList[peer];
}
map<string, PeerData>* PeerManager::GetPeerList()
{
	return &peerList;
}
boost::mutex* PeerManager::GetPeerListMutex()
{
	return &peerListMutex;
}

//ECM
boost::mutex* PeerManager::GetPeerActiveMutexIn()
{
	return &peerActiveMutexIn;
}
boost::mutex* PeerManager::GetPeerActiveMutexOut()
{
	return &peerActiveMutexOut;
}
//ECM

//ECM metodo privado criado para ser chamado duas vezes (In e Out) em CheckPeerList()
void PeerManager::CheckpeerActiveCooldown(map<string, unsigned int>* peerActiveCooldown)
{
	set<string> deletedPeer;
	for (map<string, unsigned int>::iterator i = peerActiveCooldown->begin(); i != peerActiveCooldown->end(); i++)
	   {
		i->second--;
		if (i->second == 0)
			deletedPeer.insert(i->first);
	}
	for (set<string>::iterator i = deletedPeer.begin(); i != deletedPeer.end(); i++)
    {
	       peerActiveCooldown->erase(*i);
	}
	deletedPeer.clear();
}
//ECM
void PeerManager::CheckPeerList()
{
	//ECM
	//Tabela de decisao para remover peer.
	//|--------------------------------------------------------------------------------------------------|
	//| ttlIn ttlOut PeerActiveIn    PeerActiveOut |  Desconectar In | Desconectar Out | Remover  | caso |
	//|--------------------------------------------------------------------------------------------------|
	//|   0    <>0     pertence        pertence    |       X         |                 |          |   1  |
	//|  <>0    0      pertence        pertence    |                 |        X        |          |   2  |
	//|   0     0      pertence        pertence    |       X         |        X        |     X    |   3  |
	//|   0    <>0     pertence      nao pertence  |       X         |                 |     X    |   4  |
	//|  <>0    0    nao pertence      pertence    |                 |        X        |     X    |   5  |
    //|--------------------------------------------------------------------------------------------------|

    set<string> desconectaPeerIn;  //pares In a serem desconectados
    set<string> desconectaPeerOut; //pares Out a serem desconectados
    set<string> deletaPeer;        //pares a serem removidos de peersList

    bool isPeerActiveIn = false;
    bool isPeerActiveOut  = false;

    boost::mutex::scoped_lock peerActiveInLock(peerActiveMutexIn);
    boost::mutex::scoped_lock peerActiveOUTLock(peerActiveMutexOut);

    //gera lista com todos os pares ativos
    set<string> temp_allActivePeer (peerActiveIn);
    for (set<string>::iterator i = peerActiveOut.begin(); i != peerActiveOut.end(); i++)
    	temp_allActivePeer.insert(*i);

    for (set<string>::iterator i = temp_allActivePeer.begin(); i != temp_allActivePeer.end(); i++)
    {
    	isPeerActiveIn = peerActiveIn.find(*i) != peerActiveIn.end();
    	if (isPeerActiveIn) //trata a lista In
    	{
    		peerList[*i].DecTTLIn();
    		if (peerList[*i].GetTTLIn() <= 0)
    		{
    			desconectaPeerIn.insert(*i);
    			isPeerActiveIn = false;
    		}
    	}
    	isPeerActiveOut = peerActiveOut.find(*i) != peerActiveOut.end();
    	if (isPeerActiveOut) //trata a lista Out
    	{
    		peerList[*i].DecTTLOut();
    		if (peerList[*i].GetTTLOut() <= 0)
    	    {
    			desconectaPeerOut.insert(*i);
    			isPeerActiveOut = false;
    		}

    	}
    	if ((!isPeerActiveIn) && (!isPeerActiveOut)) //remove o peer da lista de vizinhos
     		deletaPeer.insert(*i);
    }
    peerActiveInLock.unlock();
	peerActiveOUTLock.unlock();

    for (set<string>::iterator i = desconectaPeerIn.begin(); i != desconectaPeerIn.end(); i++)
    {
    	DisconnectPeer(*i, &peerActiveIn, &peerActiveMutexIn);
    }
    for (set<string>::iterator i = desconectaPeerOut.begin(); i != desconectaPeerOut.end(); i++)
        {
        	DisconnectPeer(*i, &peerActiveOut, &peerActiveMutexOut);
        }
    for (set<string>::iterator i = deletaPeer.begin(); i != deletaPeer.end(); i++)
    {
        RemovePeer(*i);
    }

    this->CheckpeerActiveCooldown(&peerActiveCooldownIn);
    this->CheckpeerActiveCooldown(&peerActiveCooldownOut);
}

//funcao usada interna em ShowPeerList para auxiliar na impressao
int PeerManager::showPeerActive(set<string>* peerActive, boost::mutex* peerActiveMutex)
{
	int j = 0; int ttl = 0; string ttlRotulo("");
	boost::mutex::scoped_lock peerActiveLock(*peerActiveMutex);

    for (set<string>::iterator i = (*peerActive).begin(); i != (*peerActive).end(); i++, j++)
	{
    	if (peerActive == &peerActiveIn)
        {
        	ttl = peerList[*i].GetTTLIn();
        	ttlRotulo == "TTLIn";
        }
        else
        {
        	ttl = peerList[*i].GetTTLOut();
        	ttlRotulo == "TTLOut";
        }

	    cout<<"Key: "<<*i<<" ID: "<<peerList[*i].GetPeer()->GetID()<<" Mode: "<<(int)peerList[*i].GetMode()<<ttlRotulo<<": "<<ttl << " PR: "<<peerList[*i].GetPendingRequests() << endl;
	}
	peerActiveLock.unlock();
	return j;

}

void PeerManager::ShowPeerList()
{
    int k = 0;
    int j = 0;
    cout<<endl<<"- Peer List Active -"<<endl;
    k = showPeerActive(&peerActiveIn, &peerActiveMutexIn);
	cout<<"Total: "<<k<<" Peers In"<<endl<<endl;
    j = showPeerActive(&peerActiveOut, &peerActiveMutexOut);
    cout<<"Total: "<<j<<" Peers Out"<<endl<<endl;
    cout<<"Total: "<<k+j<<" ActivePeers"<<endl<<endl;

	j = 0;
	cout<<endl<<"- Peer List Total -"<<endl;
	boost::mutex::scoped_lock peerListLock(peerListMutex);
    for (map<string, PeerData>::iterator i = peerList.begin(); i != peerList.end(); i++, j++)
	{
		cout<<"Key: "<<i->first<< endl;
		cout<<"ID: "<<i->second.GetPeer()->GetID()<<" Mode: "<<(int)i->second.GetMode()<<" TTLIn: "<<i->second.GetTTLIn() <<" TTLOut: "<<i->second.GetTTLOut() << " RTT(delay): " <<i->second.GetDelay()<< "s PR: "<<i->second.GetPendingRequests() << endl;
	}
	peerListLock.unlock();
	cout<<"Total: "<<j<<" Peers"<<endl<<endl;
}

