echo "Updating Bluetooth Driver BlueZ with Community patch for better PSVR2 tracking..."

echo "Requesting sudo permissions..."
sudo echo "sudo permissions granted"

echo "Disabling readonly filesystem"
sudo steamos-readonly disable

echo "Updating SteamOS package lists"
sudo pacman-key --init
sudo pacman-key --populate archlinux
sudo pacman-key --populate holo

echo "Setting Download folder as working directory"
cd "$HOME/Downloads"

echo "Cloning RealSupremium's BlueZ fork"
git clone -b optional-force-active https://github.com/RealSupremium/bluez.git
cd bluez

echo "Prepare build env"

sudo pacman -S --needed base-devel git python-docutils

# SteamOS stripped out the development files for glib2 and other dependencies.
# So dbus-1.pc will look for libsystemd.pc, which isn't present in the base SteamOS image.
# Reinstall glib2 and other build dependencies with the overwrite flag to restore {dependency}.pc files and their header files.
sudo pacman -S --overwrite "*" glib2 glib2-devel sysprof pcre2 dbus readline libical systemd systemd-libs icu

echo "Creating package config file stub for missing glib2 dependency"

# SteamOS’s package manifest leaves a requirement for sysprof-capture-4 inside glib-2.0.pc,
# but omits the actual .pc file from its system image.
# Creating a stub package config file satisfies pkg-config so GLib can build.
sudo tee /usr/lib/pkgconfig/sysprof-capture-4.pc << 'EOF'
Name: sysprof-capture-4
Description: Stub for SteamOS build compatibility
Version: 4.0.0
Libs:
Cflags:
EOF

echo "Configuring Build"

./bootstrap
./configure \
  --prefix=/usr \
  --sysconfdir=/etc \
  --localstatedir=/var \
  --libexecdir=/usr/lib \
  --enable-library \
  --enable-sixaxis \
  --enable-hid2hci \
  --enable-experimental

echo "Compiling BlueZ"
make -j$(nproc)

echo "Installing BlueZ"
sudo make install
echo "Install complete!"

echo "Enabling readonly filesystem"
sudo steamos-readonly enable

echo "Restarting Bluetooth Driver"
sudo systemctl daemon-reload
sudo systemctl restart bluetooth

echo "Getting Bluetooth status"
systemctl status bluetooth

echo "Getting current Bluetooth Driver status. This should be 5.87 or higher."
bluetoothctl --version

echo "Finished"
echo "Re-pair your PSVR2 Controllers to your PC for improved and stable tracking"
echo "Remember to Re-run this script after a SteamOS update." 
