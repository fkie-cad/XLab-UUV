#!/usr/bin/env bash

uname_out=$(uname -s)
case $uname_out in
  Linux*)   VLC=vlc;;
  Darwin*)  VLC="/Applications/VLC.app/Contents/MacOS/VLC";;
  *)        echo "Unsupported OS '$uname_out'"; exit 1;;
esac

$VLC --zoom=0.25 rtp://:5010
