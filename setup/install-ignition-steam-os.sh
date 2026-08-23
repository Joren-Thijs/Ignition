#!/bin/bash

echo "Setup starting"

# Settings
steamvr_fixes_version="v0.1.4"
psvr2_toolkit_version="v1.0.0-experimental-1"
psvr2_toolkit_unity_setup_version="v1.1.0"

echo "Checking if necessary files are present..."
steamvr_path="$HOME/.steam/steam/steamapps/common/SteamVR"
psvr2_app_path="$HOME/.steam/steam/steamapps/common/PlayStation VR2 App"
vrpath_file="$HOME/.config/openvr/openvrpaths.vrpath"
steamvr_settings_file="$HOME/.steam/steam/config/steamvr.vrsettings"

if [ ! -d "$steamvr_path" ]; then
    echo "Error: $steamvr_path does not exist yet.
    You need to install SteamVR from Steam before proceeding.
    Aborting installation..." >&2
    exit 1
fi

if [ ! -d "$psvr2_app_path" ]; then
    echo "Error: $psvr2_app_path does not exist yet.
    You need to install the Playstation VR2 App from Steam before proceeding.
    Aborting installation..." >&2
    exit 1
fi

if [ ! -f "$vrpath_file" ]; then
    echo "Error: $vrpath_file does not exist yet.
    You need to run SteamVR at least once before proceeding.
    Aborting installation..." >&2
    exit 1
fi

if [ ! -f "$steamvr_settings_file" ]; then
    echo "Error: $steamvr_settings_file does not exist yet.
    You need to run SteamVR at least once before proceeding.
    Aborting installation..." >&2
    exit 1
fi

echo "All necessary files are present, proceeding with installation..."

echo "Requesting sudo permissions..."
sudo echo "sudo permissions granted"

# Disable readonly filesystem
echo "Disabling readonly filesystem"
sudo steamos-readonly disable

#Setup working directories
work_dir="$HOME/PSVR2-toolkit-setup"
temp_dir="$work_dir/temp"

mkdir -p "$temp_dir"

echo "Setup XR Hardware Rules"

sudo wget -O /etc/udev/rules.d/70-xrhardware.rules "https://gitlab.freedesktop.org/monado/utilities/xr-hardware/-/raw/main/70-xrhardware.rules?ref_type=heads" 
sudo udevadm control --reload-rules
sudo udevadm trigger

echo "Finished setup XR Hardware Rules"

# Re-enable readonly filesystem
echo "Enabling readonly filesystem"
sudo steamos-readonly enable

echo "Setup SteamVR Fixes"

steamvr_fixes_zip_path="$temp_dir/steamvr_fixes.zip"
steamvr_fixes_dir="$work_dir/steamvr-fixes"

wget -O "$steamvr_fixes_zip_path" "https://github.com/BnuuySolutions/SteamVRLinuxFixes/releases/download/$steamvr_fixes_version/VK_LAYER_BNUUY_steamvr_linux_fixes.zip" > /dev/null

unzip -o "$steamvr_fixes_zip_path" -d "$steamvr_fixes_dir"

# Modify vrcompositor-launcher.sh. See https://github.com/BnuuySolutions/SteamVRLinuxFixes
vr_compositor_path="$steamvr_path/bin/linux64/vrcompositor-launcher.sh"

if grep -q "VK_LAYER_BNUUY_steamvr_linux_fixes" "$vr_compositor_path"; then
    echo "SteamVR fixes layer already present in $vr_compositor_path, skipping patch."
else
    echo "Patching $vr_compositor_path..."
    sed -i.bak "/export SDL_VIDEODRIVER=x11/a export VK_ADD_LAYER_PATH=\"$steamvr_fixes_dir\"\nexport VK_INSTANCE_LAYERS=VK_LAYER_BNUUY_steamvr_linux_fixes" "$vr_compositor_path"
fi

echo "Finished Setup SteamVR Fixes"

echo "Setup Ignition"

ignition_dir="$HOME/ignition"
psvr2_plugin_dir="$psvr2_app_path/SteamVR_Plug-In"
psvr2_plugin_linux_dir="$psvr2_plugin_dir/bin/linux64"
psvr2_plugin_windows_dir="$psvr2_plugin_dir/bin/win64"

wget -O "$temp_dir/ignition.zip" "https://github.com/BnuuySolutions/Ignition/releases/download/v1.0.0/Ignition-Linux-Windows.zip"

unzip -o "$temp_dir/ignition.zip" -d "$ignition_dir"

# Ensure installer script is executable
chmod +x "$ignition_dir/install_ignition.sh"

echo "Executing Ignition install script"
(
    cd "$ignition_dir" || exit 1
    ./install_ignition.sh "$psvr2_plugin_dir/"
)

echo "Executing Driver install script"
(
    cd "$psvr2_plugin_linux_dir" || exit 1
    ./driver_install.sh
)

echo "Installing PSVR2 Toolkit"
psvr2_toolkit_zip_path="$temp_dir/psvr2-toolkit.zip"
wget -O "$psvr2_toolkit_zip_path" "https://github.com/BnuuySolutions/PSVR2Toolkit/releases/download/$psvr2_toolkit_version/PSVR2TK-win64-Ignition.zip"

orig_dll="$psvr2_plugin_windows_dir/driver_playstation_vr2_orig.dll"
target_dll="$psvr2_plugin_windows_dir/driver_playstation_vr2.dll"

echo "Creating backup of PSVR2 Driver: driver_playstation_vr2.dll"
if [ ! -f "$orig_dll" ]; then
    if [ -f "$target_dll" ]; then
        cp "$target_dll" "$orig_dll"
    else
        echo "Error: PSVR2 Driver $target_dll does not exist to back up." >&2
        exit 1
    fi
else
    echo "Backup of PSVR2 driver already exists. Skipping copy."
fi

unzip -o "$psvr2_toolkit_zip_path" -d "$psvr2_plugin_windows_dir"

echo "Finished Setup Ignition"

echo "Editing SteamVR settings for best experience..."

if [ -f "$steamvr_settings_file" ]; then
    echo "Updating $steamvr_settings_file using jq..."
    
    # Create a temporary file to safely write JSON before replacing the original
    tmp_json=$(mktemp)
    
    jq '.steamvr //= {} | .steamvr.enableLinuxVulkanAsync = true | .steamvr.useFacetRenderer = true' "$steamvr_settings_file" > "$tmp_json" && mv "$tmp_json" "$steamvr_settings_file"
    
    echo "Successfully updated steamvr.vrsettings"
else
    echo "Warning: $steamvr_settings_file not found. Skipping settings update."
fi

echo "Finished editing SteamVR settings"

echo "Installing PSVR2 Toolkit UnitySetup for setting up playspace"

unity_setup_path="$work_dir/psvr2-toolkit-unity-setup"
unity_setup_zip_path="$temp_dir/psvr2-toolkit-unity-setup.zip"

wget -O "$unity_setup_zip_path" "https://github.com/BnuuySolutions/PSVR2Toolkit.UnitySetup/releases/download/$psvr2_toolkit_unity_setup_version/PSVR2Toolkit.UnitySetup-Linux.zip"

unzip -o "$unity_setup_zip_path" -d "$unity_setup_path"

echo "Setup Finished"
