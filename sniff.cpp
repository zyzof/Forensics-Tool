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

using namespace std;

// Defaults:
// Maximum number of ports checked by a source before port scan detected.
#define MAX_NUM_PORTS 5
// Maximum delay time between different-port connections to detect port scan (seconds).
#define MAX_DELAY_TIME 5

// Class representing a host that is potentially trying to do a port scan.
class Host {
  private:
    double timestamp;
    struct in_addr sAddr;
    struct in_addr dAddr;
    set<u_short> ports;
  
  public:
    Host(struct in_addr src, struct in_addr dst) {
        this->sAddr = src;
        this->dAddr = dst;
    }
    
    // When a new packet is received from this host
    bool newPacket(timeval ts, u_short port) {
        double newTimestamp = (double) ts.tv_sec + ts.tv_usec * 0.000001;
        
        if (newTimestamp - this->timestamp >= MAX_DELAY_TIME) {
            ports.clear();
        }
        ports.insert(port);
        this->timestamp = newTimestamp;
        return ports.size() >= MAX_NUM_PORTS;
    }
    
    void clear() {
        ports.clear();
        timestamp = 0.0;
    }
};

bool showSniff = false;
bool sniffing = false;
bool psd = false;
map<u_long, map<u_long, Host> > hosts;
string scanDetectedSource = "";
bool scanDetected = false;
timeval scanDetectedTime;

Case* currentCase;

ofstream* packetOutput;
struct pcap_pkthdr* recentHdrs[5];
u_char* recentPkts[5];
pthread_t sniffThreads[2];
string sniffDev;
string outputFilename;

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
void printPacket(Case* current_case, int i) {
    const struct pcap_pkthdr* pkthdr = recentHdrs[i];
    u_char* packet = recentPkts[i];
    // Clear entire line
    put_output(*current_case, "\033[2K");
    if (pkthdr == NULL) {
        put_output(*current_case, "\n");
        return;
    }
    
    // Get timestamp
    struct tm* lt = localtime(&pkthdr->ts.tv_sec);
    char timebuf[32], utimebuf[32];
    strftime(timebuf, sizeof timebuf, "%Y-%m-%d %H:%M:%S", lt);
    snprintf(utimebuf, sizeof utimebuf, "%s.%03ld ", timebuf, pkthdr->ts.tv_usec / 1000L);
    
    // Check if packet received in last 5 seconds
    struct timeval now;
    gettimeofday(&now, NULL);
    if (now.tv_sec < pkthdr->ts.tv_sec + 5) {
        // Set bright
        put_output(*current_case, "\033[1m");
    }
    
    // Default packet type: unknown
    string packetType = "?";
    
    struct ether_header* ehdr = (struct ether_header*) packet;
    string sourceStr = getHostString(ehdr->ether_shost);
    string destStr = getHostString(ehdr->ether_dhost);
    // Check if packet is IPv4
    if (ntohs(ehdr->ether_type) == ETHERTYPE_IP) {
        const struct ip* ipStruct = (struct ip*) (packet + sizeof(struct ether_header));
        if (ipStruct->ip_v == 4) {
            packetType = "IPv4";
            sourceStr = inet_ntoa(ipStruct->ip_src);
            destStr = inet_ntoa(ipStruct->ip_dst);
        }

    }
    // Else if it is IPv6
    else if (ntohs(ehdr->ether_type) == ETHERTYPE_IPV6) {
        char* ip6 = (char*) (packet + sizeof(struct ether_header));
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
        }
    }
    // Print row
    char buffer[256] = { '\0' };
    sprintf(buffer, "%23s %4s %17s %17s\n", utimebuf, packetType.c_str(), sourceStr.c_str(), destStr.c_str());
    put_output(*current_case, buffer);
    // Set normal yellow text (if bright)s
    put_output(*current_case, "\033[m\033[33m");
}

// Called when port scan detector detects an a port scan.
void alertScan(string srcAddr, const struct pcap_pkthdr* pkthdr) {
    char c[32] = { '\0' };
    string str = "Port scan detected from " + srcAddr + ".";
    memcpy(&c, str.c_str(), str.size() + 1);
    log_text(*currentCase, c);
    scanDetected = true;
    scanDetectedSource = srcAddr;
    memcpy(&scanDetectedTime, &pkthdr->ts, sizeof(timeval));
}

// Process a packet for port scan detector
void psdProcessPacket(const struct pcap_pkthdr* pkthdr, const u_char* packet) {
    struct ether_header* ehdr = (struct ether_header*) packet;
    if (ntohs(ehdr->ether_type) == ETHERTYPE_IP) { // IPv4
        const struct ip* ipStruct = (struct ip*) (packet + sizeof(struct ether_header));
        if (ipStruct->ip_v == 4) { // IPv4
            if (ipStruct->ip_p == 6) { // TCP
                const struct tcphdr* tcpStruct = (struct tcphdr*) (ipStruct + sizeof(struct ip));
                map<u_long, map<u_long, Host> >::iterator it = hosts.find(ipStruct->ip_src.s_addr);
                // If new host, create and put into map
                if (it == hosts.end()) {
                    Host host(ipStruct->ip_src, ipStruct->ip_dst);
                    host.newPacket(pkthdr->ts, tcpStruct->dest);
                    map<u_long, Host> dests;
                    dests.insert(pair<u_long, Host>(ipStruct->ip_dst.s_addr, host));
                    hosts.insert(pair<u_long, map<u_long, Host> >(ipStruct->ip_src.s_addr, dests));
                }
                // Else if known host, update
                else {
                    map<u_long, Host> dests = it->second;
                    map<u_long, Host>::iterator dIt = dests.find(ipStruct->ip_dst.s_addr);
                    if (dIt == dests.end()) {
                        Host host(ipStruct->ip_src, ipStruct->ip_dst);
                        host.newPacket(pkthdr->ts, tcpStruct->dest);
                        dests.insert(pair<u_long, Host>(ipStruct->ip_dst.s_addr, host));
                    }
                    else {
                        if (dIt->second.newPacket(pkthdr->ts, tcpStruct->dest)) {
                            dIt->second.clear();
                            alertScan(inet_ntoa(ipStruct->ip_src), pkthdr);
                        }
                    }
                }
            }
        } else {/* IPv6 packet? */}
    }
}

// Process a packet for packet sniffer
void processPacket(u_char*, const struct pcap_pkthdr* pkthdr, const u_char* packet) {
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
    
    // Write raw packet to output file
    packetOutput->write((char*) &pkthdr, sizeof(struct pcap_pkthdr));
    packetOutput->write((char*) &packet, pkthdr->caplen);
    
    // Add new packet to recent packet list.
    for (int i = 4; i > 0; i--) {
        recentHdrs[i] = recentHdrs[i-1];
        recentPkts[i] = recentPkts[i-1];
    }
    recentHdrs[0] = (struct pcap_pkthdr*) malloc(sizeof(struct pcap_pkthdr));
    recentPkts[0] = (u_char*) malloc(pkthdr->caplen);
    memcpy(recentHdrs[0], pkthdr, sizeof(struct pcap_pkthdr));
    memcpy(recentPkts[0], packet, pkthdr->caplen);
    
    // If port scan detecting, let it process packet too
    if (psd) {
        psdProcessPacket(pkthdr, packet);
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

void* sniff(void*) {
    char errbuf[PCAP_ERRBUF_SIZE];
    char buffer[256] = { '\0' };
    pcap_t* p = pcap_create(sniffDev.c_str(), errbuf);
    if (p == NULL) {
        put_output(*currentCase, "Unable to open device: ");
        put_output(*currentCase,  errbuf);
        put_output(*currentCase, "\n");
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
    packetOutput = new ofstream(outputFilename.c_str(), ios::out | ios::binary);
    if (packetOutput->fail()) {
        put_output(*currentCase, ("Unable to open output stream \"" + outputFilename + "\".\n").c_str());
    }
    pcap_loop(p, -1, processPacket, NULL);
    packetOutput->close();
}

void* updateDisplay(void*) {
    while (true) {
        usleep(50000);
        string clearline = "\033[2K";
        if (showSniff || scanDetected) {
            // Save cursor position
            put_output(*currentCase, "\033[s");
            // Go to top left corner of terminal
            put_output(*currentCase, "\033[1;1H");
            if (showSniff) {
                // Set yellow text
                put_output(*currentCase, "\033[33m");
                put_output(*currentCase, clearline.c_str());
                put_output(*currentCase, "Latest packets sniffed:\n");
                put_output(*currentCase, clearline.c_str());
                put_output(*currentCase, "-----------------------   Pkt            Source       Destination\n");
                for (int i = 4; i >= 0; i--) {
                    printPacket(currentCase, i);
                }
                put_output(*currentCase, clearline.c_str());
                put_output(*currentCase, "-----------------------------------------------------------------\n");
            }
            if (scanDetected) {
                // Set red text
                put_output(*currentCase, "\033[31m");
                
                // Get timestamp
                struct tm* lt = localtime(&scanDetectedTime.tv_sec);
                char timebuf[32], utimebuf[32];
                strftime(timebuf, sizeof timebuf, "%Y-%m-%d %H:%M:%S", lt);
                snprintf(utimebuf, sizeof utimebuf, "%s.%03ld", timebuf, scanDetectedTime.tv_usec / 1000L);
                
                // Check if packet received in last 10 seconds
                struct timeval now;
                gettimeofday(&now, NULL);
                if (now.tv_sec < scanDetectedTime.tv_sec + 10) {
                    // Set bright
                    put_output(*currentCase, "\033[1m");
                }
                put_output(*currentCase, clearline.c_str());
                put_output(*currentCase, "[");
                put_output(*currentCase, utimebuf);
                put_output(*currentCase, "] Port scan detected from ");
                put_output(*currentCase, scanDetectedSource.c_str());
                put_output(*currentCase, "!\n");
            }
            // Restore colour and cursor position
            put_output(*currentCase, "\033[m\033[u");
        }
    }
}

void start_psd(Case* theCase) {
    currentCase = theCase;
    if (psd) {
        put_output(*currentCase, "\nPort scan detection is already running.");
        return;
    }
    if (!sniffing) {
        put_output(*currentCase, "\nPacket sniffer must be started before port scan detection can begin.");
    }
    else {
        log_text(*currentCase, "Port scan detection started.");
        psd = true;
        put_output(*currentCase, "\nPort scan detection started.");
    }
}

void stop_psd() {
    if (psd) {
        psd = false;
        scanDetected = false;
        log_text(*currentCase, "Port scan detection stopped.");
        put_output(*currentCase, "\nPort scan detection stopped.");
    }
    else {
        put_output(*currentCase, "\nPort scan detection is not running.");
    }
}

void start_sniff(Case* theCase) {
    currentCase = theCase;
    sniffDev = "";
    bool devSelected = false;
    char devBuff[32] = { '\0' };
    char input[256] = { '\0' };
    set<string> devSet = getDevSet();
    while (!devSelected) {
        put_output(*currentCase, "\nEnter device to sniff: ");
        get_input(*currentCase, devBuff);
        sniffDev = devBuff;
        if (devSet.count(sniffDev) == 0) {
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
    outputFilename = "";
    while (outputFilename.length() == 0) {
        put_output(*currentCase, "\nEnter name of output file: ");
        get_input(*currentCase, input);
        outputFilename = input;
    }
    string origFilename = outputFilename;
    char filename[BUFFER_SIZE] = { '\0' };
    strcpy(filename, outputFilename.c_str());
    file_string(*theCase, filename);
    outputFilename = filename;
    
    put_output(*currentCase, "\nShow latest packets sniffed? [y/n] ");
    string answer;
    get_input(*currentCase, input);
    answer = input;
    
    int rc = pthread_create(&sniffThreads[0], NULL, sniff, NULL);
    if (rc) {
        return;
    }
    
    char c[128] = { '\0' };
    string str = "Packet sniffing started on device \"" + sniffDev + "\", saving to file \"" + origFilename + "\".";
    memcpy(&c, str.c_str(), str.size() + 1);
    log_text(*currentCase, c);
    
    sniffing = true;
    put_output(*currentCase, "\nPacket sniffing started.");
    
    showSniff = false;
    if (tolower(answer[0]) == 'y') {
        rc = pthread_create(&sniffThreads[1], NULL, updateDisplay, NULL);
        showSniff = true;
    }
}

void stop_sniff() {
    if (!sniffing) {
        put_output(*currentCase, "\nError: Packet sniffer not running.");
        return;
    }
    if (psd) {
        put_output(*currentCase, "This will also stop port scan detection. Continue? [y/n] ");
        char answer[2] = { '\0' };
        get_input(*currentCase, answer);
        put_output(*currentCase, "\n");
        if (tolower(answer[0]) == 'y') {
            stop_psd();
        }
        else {
            put_output(*currentCase, "Continuing sniffing and detecting port scans.");
            return;
        }
    }
    pthread_cancel(sniffThreads[0]);
    pthread_cancel(sniffThreads[1]);
    log_text(*currentCase, "Packet sniffing stopped.");
    showSniff = false;
    sniffing = false;
    put_output(*currentCase, "\nPacket sniffing stopped.");
}

void show_sniff() {
    if (sniffing) {
        if (showSniff) {
            put_output(*currentCase, "\nAlready showing recent packets.");
        }
        else {
            showSniff = true;
            put_output(*currentCase, "\nRecent packets displayed.");
        }
    }
    else {
        put_output(*currentCase, "\nError: Packet sniffer is not running.");
    }
}

void hide_sniff() {
    if (sniffing) {
        if (showSniff) {
            showSniff = false;
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

void network_devices() {
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