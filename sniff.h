#ifndef SNIFF_H
#define SNIFF_H

#include <string>
#include <set>

using namespace std;

void start_sniff(Case* theCase);

void stop_sniff();

void show_sniff();

void hide_sniff();

void network_devices(Case* theCase);

void start_psd(Case* theCase);

void stop_psd();

#endif