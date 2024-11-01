#!/bin/bash

# Check if the interface name is provided as an argument
if [ -z "$1" ]; then
    echo "Usage: $0 <interface>"
    exit 1
fi

INTERFACE=$1

# Version 2.3
# 3-18-16 jpannell

# Force CMODE, Use SGMII AN results
# P5
echo "Forcing CMODE, Using SGMII AN results for P5"
mdio-tool w $INTERFACE 0x05 0x1A 0xA100
mdio-tool w $INTERFACE 0x04 0x1A 0xFCA0

# Set Ports in SGMII mode
echo "Setting ports in SGMII mode for P5"
mdio-tool w $INTERFACE 0x05 0x00 0x000A

# Force SGMII PHY Mode, SPEED 1000Mbps
# P5
echo "Forcing SGMII PHY mode, Speed 1000Mbps for P5"
mdio-tool w $INTERFACE 0x05 0x1A 0x0A02
mdio-tool w $INTERFACE 0x04 0x1A 0xFCA2

# Reset SERDES
# Port 5
echo "Resetting SERDES for Port 5"
mdio-tool w $INTERFACE 0x1C 0x19 0x2000
mdio-tool w $INTERFACE 0x1C 0x18 0x82A4
mdio-tool w $INTERFACE 0x1C 0x19 0x9340
mdio-tool w $INTERFACE 0x1C 0x18 0x86A4

# Set port state to forwarding
echo "Setting port state to forwarding for P5"
mdio-tool w $INTERFACE 0x05 0x04 0x007F

echo "Configuration complete."

