#!/usr/bin/env bash

cd "$(dirname "$0")"

uname_out=$(uname -s)
case $uname_out in
  Linux*)   VLC=vlc; CVLC=cvlc;;
  Darwin*)  VLC="/Applications/VLC.app/Contents/MacOS/VLC"; CVLC="/Applications/VLC.app/Contents/MacOS/VLC";;
  *)        echo "Unsupported OS '$uname_out'"; exit 1;;
esac

"$CVLC" --loop video.mkv --sout="#rtp{mux=ts, dst=$1, port=5010}"
