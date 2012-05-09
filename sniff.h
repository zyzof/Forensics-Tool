#ifndef SNIFF_H
#define SNIFF_H

#include <string>
#include <set>
#include <netinet/in.h>
#include <map>

using namespace std;

// Class representing a host that is potentially trying to do a port scan.
class Host {
  private:
    double timestamp;
    struct in_addr sAddr;
    struct in_addr dAddr;
    set<u_short> ports;
  
  public:
    Host(struct in_addr src, struct in_addr dst);
    bool newPacket(timeval ts, u_short port);
    void clear();
};

typedef struct SnifferState_struct {
    Case* currentCase;
    
    bool showSniff;
    bool sniffing;
    bool psd;
    map<u_long, map<u_long, Host*>* >* hosts;
    string* scanDetectedSource;
    bool scanDetected;
    timeval scanDetectedTime;
    
    ofstream* packetOutput;
    struct pcap_pkthdr* recentHdrs[5];
    u_char* recentPkts[5];
    pthread_t sniffThreads[2];
    string* sniffDev;
    string* outputFilename;
} SnifferState;

void start_sniff(Case* currentCase);

void stop_sniff(Case* currentCase);

void show_sniff(Case* currentCase);

void hide_sniff(Case* currentCase);

void network_devices(Case* currentCase);

void start_psd(Case* currentCase);

void stop_psd(Case* currentCase);

#endif