#!/bin/bash
source /opt/opendds/OpenDDS-3.24.1/setenv.sh
cd /opt/dds/
./autopilot -ORBDebugLevel 1 -DCPSConfigFile rtps.ini
