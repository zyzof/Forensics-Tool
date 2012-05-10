#ifndef SNIFF_CPP
#define SNIFF_CPP

#include <pcap.h>
#include <ctime>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <set>
#include <sstream>
#include <netinet/if_ether.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <fstream>
#include <pthread.h>
#include <map>

#include "constants.h"
#include "case.h"
#include "server.h"
#include "sniff.h"

using namespace std;

// Defaults:
// Maximum number of ports checked by a source before port scan detected.
#define MAX_NUM_PORTS 20
// Maximum delay time between different-port connections to detect port scan (seconds).
#define MAX_DELAY_TIME 5

#define TEXT_PACKETS_FILE "log_packets.txt"
#define  RAW_PACKETS_FILE "raw_packets.dat"

struct udphdr {
    u_short source;
    u_short dest;
    u_short len;
    u_short checksum;
};

// Class representing a host that is potentially trying to do a port scan.
Host::Host(struct in_addr src, struct in_addr dst) {
    this->sAddr = src;
    this->dAddr = dst;
}

// When a new packet is received from this host
bool Host::newPacket(timeval ts, u_short port) {
    double newTimestamp = (double) ts.tv_sec + ts.tv_usec * 0.000001;
    
    if (newTimestamp - this->timestamp >= MAX_DELAY_TIME) {
        ports.clear();
    }
    ports.insert(port);
    this->timestamp = newTimestamp;
    return ports.size() >= MAX_NUM_PORTS;
}
    
void Host::clear() {
    ports.clear();
    timestamp = 0.0;
}

// Convert an IPv4 address in bytes to readable string format.
string getHostString(u_char* host) {
    u_char* h = host;
    stringstream ss;
    for (int i = ETHER_ADDR_LEN-1; i >= 0; i--) {
        if (i < ETHER_ADDR_LEN-1) {
            ss << ":";
        }
        ss << hex << (u_int32_t) *h++;
    }
    return ss.str();
}

// Convert an IPv6 address in bytes to readable string format.
string getIP6HostString(u_char* host) {
    stringstream ss;
    for (int i = 0; i < 16; i += 2) {
        ss << hex << (u_int32_t) host[i] << (u_int32_t) host[i+1];
        if (i < 14) {
            ss << ":";
        }
    }
    return ss.str();
}

// Print an individual row of a packet in the display.
string printPacket(SnifferState* state, int i, bool printCtrlChars) {
    const struct pcap_pkthdr* pkthdr = state->recentHdrs[i];
    u_char* packet = state->recentPkts[i];
    stringstream ss;
    // Clear entire line
    if (printCtrlChars) {
        ss << "\033[2K";
    }
    if (pkthdr == NULL) {
        if (printCtrlChars) {
            ss << "\n";
        }
        return ss.str();
    }
    
    // Get timestamp
    struct tm* lt = localtime(&pkthdr->ts.tv_sec);
    char timebuf[32], utimebuf[32];
    strftime(timebuf, sizeof timebuf, "%Y-%m-%d %H:%M:%S", lt);
    snprintf(utimebuf, sizeof utimebuf, "%s.%03ld", timebuf, pkthdr->ts.tv_usec / 1000L);
    
    // Check if packet received in last 5 seconds
    struct timeval now;
    gettimeofday(&now, NULL);
    if (now.tv_sec < pkthdr->ts.tv_sec + 5 && printCtrlChars) {
        // Set bright
        ss << "\033[1m";
    }
    
    // Default packet type: unknown
    string packetType = "?";
    u_short srcPort = 0;
    u_short destPort = 0;
    bool knownPorts = false;
    
    char* ptr = (char*) packet;
    struct ether_header* ehdr = (struct ether_header*) ptr;
    ptr += sizeof(struct ether_header);
    string sourceStr = getHostString(ehdr->ether_shost);
    string destStr = getHostString(ehdr->ether_dhost);
    // Check if packet is IPv4
    if (ntohs(ehdr->ether_type) == ETHERTYPE_IP) {
        const struct ip* ipStruct = (struct ip*) ptr;
        ptr += sizeof(struct ip);
        if (ipStruct->ip_v == 4) {
            packetType = "IPv4";
            sourceStr = inet_ntoa(ipStruct->ip_src);
            destStr = inet_ntoa(ipStruct->ip_dst);
            if (ipStruct->ip_p == 6) {
                packetType += "/TCP";
                const struct tcphdr* tcpStruct = (struct tcphdr*) ptr;
                ptr += sizeof(struct tcphdr);
                srcPort = ntohs(tcpStruct->source);
                destPort = ntohs(tcpStruct->dest);
                knownPorts = true;
            }
            else if (ipStruct->ip_p == 17) {
                packetType += "/UDP";
                const struct udphdr* udpStruct = (struct udphdr*) ptr;
                ptr += sizeof(struct udphdr);
                srcPort = ntohs(udpStruct->source);
                destPort = ntohs(udpStruct->dest);
                knownPorts = true;
            }
        }

    }
    // Else if it is IPv6
    else if (ntohs(ehdr->ether_type) == ETHERTYPE_IPV6) {
        char* ip6 = ptr;
        ptr += 40; // length of IPv6 header
        u_char ipv = ip6[0x00] >> 4;
        if (ipv == 6) {
            packetType = "IPv6";
            uint16_t len = (((uint16_t) ip6[0x04]) << 0x08) + (uint16_t) ip6[0x05];
            u_char ipSrc[16];
            u_char ipDst[16];
            for (int i = 0; i < 16; i++) {
                ipSrc[i] = ip6[i + 8];
                ipDst[i] = ip6[i + 24];
            }
            sourceStr = getIP6HostString(ipSrc);
            destStr = getIP6HostString(ipDst);
            int* nextHeader = (int*) ip6[6];
            *nextHeader = ntohl(*nextHeader);
            if (*nextHeader == 6) {
                packetType += "/TCP";
                const struct tcphdr* tcpStruct = (struct tcphdr*) ptr;
                ptr += sizeof(struct tcphdr);
                srcPort = ntohs(tcpStruct->source);
                destPort = ntohs(tcpStruct->dest);
                knownPorts = true;
            }
            else if (*nextHeader == 17) {
                packetType += "/UDP";
                const struct udphdr* udpStruct = (struct udphdr*) ptr;
                ptr += sizeof(struct udphdr);
                srcPort = ntohs(udpStruct->source);
                destPort = ntohs(udpStruct->dest);
                knownPorts = true;
            }
        }
    }
    else if (ntohs(ehdr->ether_type) == ETHERTYPE_ARP) {
        packetType = "ARP";
    }
    // Print row
    char buffer[256] = { '\0' };
    sprintf(buffer, "%23s %-8s %17s", utimebuf, packetType.c_str(), sourceStr.c_str());
    ss << buffer;
    if (knownPorts) {
        sprintf(buffer, "%6u", srcPort);
        ss << buffer;
    }
    else {
        ss << "      ";
    }
    sprintf(buffer, " %17s", destStr.c_str());
    ss << buffer;
    if (knownPorts) {
        sprintf(buffer, "%6u", destPort);
        ss <<  buffer;
    }
    else {
        ss << "      ";
    }
    ss << "\n";
    // Set normal yellow text (if bright)
    if (printCtrlChars) {
        ss << "\033[m\033[33m";
    }
    return ss.str();
}

// Called when port scan detector detects an a port scan.
void alertScan(SnifferState* state, string srcAddr, const struct pcap_pkthdr* pkthdr) {
    char c[32] = { '\0' };
    string str = "Port scan detected from " + srcAddr + ".";
    memcpy(&c, str.c_str(), str.size() + 1);
    log_text(*state->currentCase, c);
    state->scanDetected = true;
    *state->scanDetectedSource = srcAddr;
    memcpy(&state->scanDetectedTime, &pkthdr->ts, sizeof(timeval));
}

// Process a packet for port scan detector
void psdProcessPacket(SnifferState* state, const struct pcap_pkthdr* pkthdr, const u_char* packet) {
    struct ether_header* ehdr = (struct ether_header*) packet;
    if (ntohs(ehdr->ether_type) == ETHERTYPE_IP) { // IPv4
        const struct ip* ipStruct = (struct ip*) (packet + sizeof(struct ether_header));
        if (ipStruct->ip_v == 4) { // IPv4
            if (ipStruct->ip_p == 6) { // TCP
                const struct tcphdr* tcpStruct = (struct tcphdr*) (ipStruct + sizeof(struct ip));
                map<u_long, map<u_long, Host*>* >::iterator it = state->hosts->find(ipStruct->ip_src.s_addr);
                // If new host, create and put into map
                if (it == state->hosts->end()) {
                    Host* host = new Host(ipStruct->ip_src, ipStruct->ip_dst);
                    host->newPacket(pkthdr->ts, tcpStruct->dest);
                    map<u_long, Host*>* dests = new map<u_long, Host*>;
                    dests->insert(pair<u_long, Host*>(ipStruct->ip_dst.s_addr, host));
                    state->hosts->insert(pair<u_long, map<u_long, Host*>* >(ipStruct->ip_src.s_addr, dests));
                }
                // Else if known host, update
                else {
                    map<u_long, Host*>* dests = it->second;
                    map<u_long, Host*>::iterator dIt = dests->find(ipStruct->ip_dst.s_addr);
                    if (dIt == dests->end()) {
                        Host* host = new Host(ipStruct->ip_src, ipStruct->ip_dst);
                        host->newPacket(pkthdr->ts, tcpStruct->dest);
                        dests->insert(pair<u_long, Host*>(ipStruct->ip_dst.s_addr, host));
                    }
                    else {
                        if (dIt->second->newPacket(pkthdr->ts, tcpStruct->dest)) {
                            dIt->second->clear();
                            alertScan(state, inet_ntoa(ipStruct->ip_src), pkthdr);
                        }
                    }
                }
            }
        } else {/* IPv6 packet? */}
    }
}

// Process a packet for packet sniffer
void processPacket(u_char* charState, const struct pcap_pkthdr* pkthdr, const u_char* packet) {
    SnifferState* state = (SnifferState*) charState;
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
    
    // Write raw packet to output file
    state->rawPacketOutput->write((char*) &pkthdr, sizeof(struct pcap_pkthdr));
    state->rawPacketOutput->write((char*) &packet, pkthdr->caplen);
    
    // Add new packet to recent packet list.
    for (int i = 4; i > 0; i--) {
        state->recentHdrs[i] = state->recentHdrs[i-1];
        state->recentPkts[i] = state->recentPkts[i-1];
    }
    state->recentHdrs[0] = (struct pcap_pkthdr*) malloc(sizeof(struct pcap_pkthdr));
    state->recentPkts[0] = (u_char*) malloc(pkthdr->caplen);
    memcpy(state->recentHdrs[0], pkthdr, sizeof(struct pcap_pkthdr));
    memcpy(state->recentPkts[0], packet, pkthdr->caplen);
    
    // Write text packet to packet log file
    string packetStr = printPacket(state, 0, false);
    state->textPacketOutput->write(packetStr.c_str(), packetStr.length());
    state->textPacketOutput->flush();
    
    // If port scan detecting, let it process packet too
    if (state->psd) {
        psdProcessPacket(state, pkthdr, packet);
    }
    
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
}

// Get a set of available network devices
set<string> getDevSet() {
    char errbuf[PCAP_ERRBUF_SIZE];
    pcap_if_t* devs;
    pcap_if_t* dev;
    set<string> devSet;
    if (pcap_findalldevs(&devs, errbuf) == -1) {
        //error
    }
    for (dev = devs; dev != NULL; dev = dev->next) {
        devSet.insert(dev->name);
    }
    pcap_freealldevs(devs);
    return devSet;
}

void* sniff(void* voidState) {
    SnifferState* state = (SnifferState*) voidState;
    char errbuf[PCAP_ERRBUF_SIZE];
    char buffer[256] = { '\0' };
    pcap_t* p = pcap_create(state->sniffDev->c_str(), errbuf);
    if (p == NULL) {
        put_output(*state->currentCase, "Unable to open device: ");
        put_output(*state->currentCase,  errbuf);
        put_output(*state->currentCase, "\n");
    }
    /* Monitor mode:
     *   ifconfig wlan0 down
     *   iwconfig wlan0 mode monitor
     *   ifconfig wlan0 up
     * 
     * Back again:
     *   ifconfig wlan0 down
     *   iwconfig wlan0 mode managed
     *   ifconfig wlan0 up
     */
    pcap_set_snaplen(p, BUFSIZ);
    pcap_set_promisc(p, 1);
    pcap_set_timeout(p, -1);
    int status = pcap_activate(p);
    state->textPacketOutput = new ofstream(state->textOutputFilename->c_str(), ios::out | ios::app);
    if (state->textPacketOutput->fail()) {
        put_output(*state->currentCase, ("Unable to open output stream \"" + *state->textOutputFilename + "\".\n").c_str());
    }
    state->rawPacketOutput = new ofstream(state->rawOutputFilename->c_str(), ios::out | ios::app | ios::binary);
    if (state->rawPacketOutput->fail()) {
        put_output(*state->currentCase, ("Unable to open output stream \"" + *state->rawOutputFilename + "\".\n").c_str());
    }
    pcap_loop(p, -1, processPacket, (u_char*) state);
    state->textPacketOutput->close();
    state->rawPacketOutput->close();
}

void* updateDisplay(void* voidState) {
    SnifferState* state = (SnifferState*) voidState;
    while (true) {
        if (state->currentCase->local == 0) {
            usleep(200000);
        }
        else {
            usleep(50000);
        }
        string clearline = "\033[2K";
        stringstream ss;
        if (state->showSniff || state->scanDetected) {
            // Save cursor position
            ss << "\033[s";
            // Go to top left corner of terminal
            ss << "\033[1;1H";
            if (state->showSniff) {
                // Set yellow text
                ss << "\033[33m";
                ss << clearline << "Latest packets sniffed:\n";
                ss << clearline;
                ss << "----------------------- Packet   Source            Port  Destination       Port \n";
                for (int i = 4; i >= 0; i--) {
                    ss << printPacket(state, i, true);
                }
                ss <<  clearline;
                ss << "--------------------------------------------------------------------------------\n";
            }
            if (state->scanDetected) {
                // Set red text
                ss << "\033[31m";
                
                // Get timestamp
                struct tm* lt = localtime(&state->scanDetectedTime.tv_sec);
                char timebuf[32], utimebuf[32];
                strftime(timebuf, sizeof timebuf, "%Y-%m-%d %H:%M:%S", lt);
                snprintf(utimebuf, sizeof utimebuf, "%s.%03ld", timebuf, state->scanDetectedTime.tv_usec / 1000L);
                
                // Check if packet received in last 10 seconds
                struct timeval now;
                gettimeofday(&now, NULL);
                if (now.tv_sec < state->scanDetectedTime.tv_sec + 10) {
                    // Set bright
                    ss << "\033[1m";
                }
                ss << clearline << "[" <<  utimebuf << "] Port scan detected from ";
                ss << *state->scanDetectedSource << "!\n";
            }
            // Restore colour and cursor position
            ss << "\033[m\033[u";
        }
        put_output(*state->currentCase, ss.str().c_str());
    }
}

void start_psd(Case* currentCase) {
    SnifferState* state = (SnifferState*) currentCase->snifferState;
    if (state->psd) {
        put_output(*currentCase, "\nPort scan detection is already running.");
        return;
    }
    if (!state->sniffing) {
        put_output(*currentCase, "\nPacket sniffer must be started before port scan detection can begin.");
    }
    else {
        log_text(*currentCase, "Port scan detection started.");
        state->psd = true;
        put_output(*currentCase, "\nPort scan detection started.");
    }
}

void stop_psd(Case* currentCase) {
    SnifferState* state = (SnifferState*) currentCase->snifferState;
    if (state->psd) {
        state->psd = false;
        state->scanDetected = false;
        log_text(*currentCase, "Port scan detection stopped.");
        put_output(*currentCase, "\nPort scan detection stopped.");
    }
    else {
        put_output(*currentCase, "\nPort scan detection is not running.");
    }
}

void start_sniff(Case* currentCase) {
    SnifferState* state = (SnifferState*) currentCase->snifferState;
    if (state == NULL) {
        state = (SnifferState*) calloc(1, sizeof(SnifferState));
        currentCase->snifferState = state;
        state->currentCase = currentCase;
        state->showSniff = false;
        state->sniffing = false;
        state->psd = false;
        state->hosts = new map<u_long, map<u_long, Host*>* >();
        state->scanDetectedSource = new string();
        state->scanDetected = false;
    }
    else if (state->sniffing) {
        put_output(*currentCase, "\nError: Packet sniffer is already running.");
        return;
    }
    
    state->sniffDev = new string("");
    bool devSelected = false;
    char devBuff[32] = { '\0' };
    char input[256] = { '\0' };
    set<string> devSet = getDevSet();
    while (!devSelected) {
        put_output(*currentCase, "\nEnter device to sniff: ");
        get_input(*currentCase, devBuff);
        *state->sniffDev = devBuff;
        if (devSet.count(*state->sniffDev) == 0) {
            put_output(*currentCase, "\nUnknown device entered. Possible devices are: ");
            set<string>::iterator it;
            for (it = devSet.begin(); it != devSet.end(); it++) {
                put_output(*currentCase, it->c_str());
                put_output(*currentCase, " ");
            }
        }
        else {
            devSelected = true;
        }
    }
    state->textOutputFilename = new string("");
    state->rawOutputFilename = new string("");
    char filename[BUFFER_SIZE] = { '\0' };
    strcpy(filename, TEXT_PACKETS_FILE);
    file_string(*currentCase, filename);
    *state->textOutputFilename = filename;
    strcpy(filename, RAW_PACKETS_FILE);
    file_string(*currentCase, filename);
    *state->rawOutputFilename = filename;
    
    put_output(*currentCase, "\nShow latest packets sniffed? [y/n] ");
    string answer;
    get_input(*currentCase, input);
    answer = input;
    
    int rc = pthread_create(&state->sniffThreads[0], NULL, sniff, (void*) state);
    if (rc) {
        put_output(*currentCase, "\nError: Unable to create thread for packet sniffing.");
        return;
    }
    
    char c[128] = { '\0' };
    string str = "Packet sniffing started on device \"" + *state->sniffDev
        + "\", logging packets to file \"" + TEXT_PACKETS_FILE
        + "\" and saving raw packets to file \"" + RAW_PACKETS_FILE + "\".";
    memcpy(&c, str.c_str(), str.size() + 1);
    log_text(*currentCase, c);
    
    state->sniffing = true;
    put_output(*currentCase, "\nPacket sniffing started.");
    
    state->showSniff = false;
    if (tolower(answer[0]) == 'y') {
        state->showSniff = true;
    }
    rc = pthread_create(&state->sniffThreads[1], NULL, updateDisplay, (void*) state);
}

void stop_sniff(Case* currentCase) {
    SnifferState* state = (SnifferState*) currentCase->snifferState;
    if (!state->sniffing) {
        put_output(*currentCase, "\nError: Packet sniffer not running.");
        return;
    }
    if (state->psd) {
        put_output(*currentCase, "This will also stop port scan detection. Continue? [y/n] ");
        char answer[2] = { '\0' };
        get_input(*currentCase, answer);
        put_output(*currentCase, "\n");
        if (tolower(answer[0]) == 'y') {
            stop_psd(currentCase);
        }
        else {
            put_output(*currentCase, "Continuing sniffing and detecting port scans.");
            return;
        }
    }
    pthread_cancel(state->sniffThreads[0]);
    pthread_cancel(state->sniffThreads[1]);
    log_text(*currentCase, "Packet sniffing stopped.");
    state->showSniff = false;
    state->sniffing = false;
    put_output(*currentCase, "\nPacket sniffing stopped.");
}

void show_sniff(Case* currentCase) {
    SnifferState* state = (SnifferState*) currentCase->snifferState;
    if (state->sniffing) {
        if (state->showSniff) {
            put_output(*currentCase, "\nAlready showing recent packets.");
        }
        else {
            state->showSniff = true;
            put_output(*currentCase, "\nRecent packets displayed.");
        }
    }
    else {
        put_output(*currentCase, "\nError: Packet sniffer is not running.");
    }
}

void hide_sniff(Case* currentCase) {
    SnifferState* state = (SnifferState*) currentCase->snifferState;
    if (state->sniffing) {
        if (state->showSniff) {
            state->showSniff = false;
            put_output(*currentCase, "\nRecent packets hidden.");
        }
        else {
            put_output(*currentCase, "\nRecent packets already hidden.");
        }
    }
    else {
        put_output(*currentCase, "\nError: Packet sniffer is not running.");
    }
}

void network_devices(Case* currentCase) {
    put_output(*currentCase, "\nNetwork devices:");
    set<string> devSet = getDevSet();
    set<string>::iterator it;
    for (it = devSet.begin(); it != devSet.end(); it++) {
        if (it->size() > 0) {
            put_output(*currentCase, "\n * ");
            put_output(*currentCase, it->c_str());
        }
    }
}

#endif