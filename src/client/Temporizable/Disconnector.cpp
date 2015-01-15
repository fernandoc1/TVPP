#include "Disconnector.hpp"

Disconnector::Disconnector(Strategy *disconnectorStrategy, PeerManager* peerManager, uint64_t timerPeriod) : Temporizable(timerPeriod)
{
	this->strategy = disconnectorStrategy;
	this->peerManager = peerManager;
}

void Disconnector::Disconnect(set<string>* peerActive)
{
	vector<PeerData*> peers;
	boost::mutex::scoped_lock peerListLock(*peerManager->GetPeerListMutex());
	for (set<string>::iterator i = peerActive->begin(); i != peerActive->end(); i++)
	{
		peers.push_back(peerManager->GetPeerData(*i));
	}
	strategy->Execute(&peers, NULL, 1);

	if (!peers.empty() && peers[0])
	{
		peerManager->DisconnectPeer(peers[0]->GetPeer()->GetID());
	}
	peerListLock.unlock();
}

void Disconnector::TimerAlarm(uint64_t timerPeriod, string timerName)
{
	Disconnect();
}
