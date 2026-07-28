#!/bin/bash

unset SteamGameId

# Add registry file for Ignition to ensure PSVR2 Sense controllers work
./proton run reg import ./wine_psvr2_hidraw.reg

./proton run "$@"
