#pragma once
#include "alias.hpp"

void ListSerialPorts();
void InitSerial();
void TerminateSerial();
void UpdateSerial(bool scan_ports);