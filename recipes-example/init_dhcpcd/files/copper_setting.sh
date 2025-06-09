# Check if the interface name is provided as an argument
if [ -z "$1" ]; then
    echo "Usage: $0 <interface>"
    exit 1
fi

INTERFACE=$1

# Version 2.4
# Updated to switch Ethernet ports to Copper mode
# 3-20-17 jpannell (Modified for Copper Mode Transition)

# Step 1: Force CMODE transition for Port 9
echo "Forcing CMODE transition to Copper for Port 9"
mdio-tool w $INTERFACE 0x09 0x00 0x000C  # Intermediate state
mdio-tool w $INTERFACE 0x09 0x00 0x000F  # Final state (Copper)

# Step 2: Force CMODE transition for Port 10
echo "Forcing CMODE transition to Copper for Port 10"
mdio-tool w $INTERFACE 0x0A 0x00 0x000C  # Intermediate state
mdio-tool w $INTERFACE 0x0A 0x00 0x000F  # Final state (Copper)

# Step 3: Reset SERDES for Port 9
echo "Resetting SERDES for Port 9"
mdio-tool w $INTERFACE 0x1C 0x19 0x2000
mdio-tool w $INTERFACE 0x1C 0x18 0x8124
mdio-tool w $INTERFACE 0x1C 0x19 0x9340
mdio-tool w $INTERFACE 0x1C 0x18 0x8524

# Step 4: Reset SERDES for Port 10
echo "Resetting SERDES for Port 10"
mdio-tool w $INTERFACE 0x1C 0x19 0x2000
mdio-tool w $INTERFACE 0x1C 0x18 0x8144
mdio-tool w $INTERFACE 0x1C 0x19 0x9340
mdio-tool w $INTERFACE 0x1C 0x18 0x8544

# Step 5: Set ports to forwarding state
echo "Setting Port 9 to forwarding state"
mdio-tool w $INTERFACE 0x09 0x04 0x007F

echo "Setting Port 10 to forwarding state"
mdio-tool w $INTERFACE 0x0A 0x04 0x007F

echo "Copper mode configuration complete."

