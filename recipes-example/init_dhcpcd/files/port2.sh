# Check if the interface name is provided as an argument
if [ -z "$1" ]; then
    echo "Usage: $0 <interface>"
    exit 1
fi

INTERFACE=$1

# Start configuring ports for traffic
# Clear power down bit and reset SERDES P2
echo "Clearing power down bit and resetting SERDES P2"
mdio-tool w $INTERFACE 0x1C 0x19 0x2000
mdio-tool w $INTERFACE 0x1C 0x18 0x8244
mdio-tool w $INTERFACE 0x1C 0x19 0x0A040
mdio-tool w $INTERFACE 0x1C 0x18 0x8644

# Fix 1000Base-X AN advertisement
# Write 4.2004.5 to 1 (Address 0x12)
echo "Fixing 1000Base-X AN advertisement"
mdio-tool w $INTERFACE 0x1C 0x19 0x2004
mdio-tool w $INTERFACE 0x1C 0x18 0x8244
mdio-tool w $INTERFACE 0x1C 0x19 0x0020
mdio-tool w $INTERFACE 0x1C 0x18 0x8644

# Enable Forwarding on ports
echo "Enabling forwarding on ports"
mdio-tool w $INTERFACE 0x02 0x04 0x007F

echo "Configuration complete."

