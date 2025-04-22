# Check if the interface name is provided as an argument
if [ -z "$1" ]; then
    echo "Usage: $0 <interface>"
    exit 1
fi

INTERFACE=$1

# disable Forwarding on ports
echo "Enabling forwarding on port 2-10"
mdio-tool w $INTERFACE 0x03 0x04 0x007c
mdio-tool w $INTERFACE 0x04 0x04 0x007c
mdio-tool w $INTERFACE 0x05 0x04 0x007c
mdio-tool w $INTERFACE 0x06 0x04 0x007c
mdio-tool w $INTERFACE 0x07 0x04 0x007c
mdio-tool w $INTERFACE 0x08 0x04 0x007c
mdio-tool w $INTERFACE 0x09 0x04 0x007c
mdio-tool w $INTERFACE 0x0A 0x04 0x007c

echo "Configuration complete."

