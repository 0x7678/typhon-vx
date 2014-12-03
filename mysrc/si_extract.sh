#!/bin/bash

INPUT="$1"
KEY="$2"

# UGLY, UGLY, UGLY
CONFIG="$HOME/.omgsm/config";
GSMPATH=`cat "$CONFIG" | grep '^GSMPATH' | cut -d '=' -f 2`;

"$GSMPATH"/bin/gsm_convert -f "$INPUT" -k "$KEY" -d 2>/dev/null | grep "^FRAME: " | while read line; do
  FRAME=`echo "$line" | cut -d " " -f 4-`
  SI=`echo "$FRAME" | cut -d " " -f 7`
  if [ "$SI" = 1d ]; then # System Information Type 5
    PWR=`echo "$FRAME" | cut -d " " -f 1`
    TA=`echo "$FRAME" | cut -d " " -f 2`
    FRAME=`echo "$FRAME"|tr -d " "`
    echo "SI5    PWR=$PWR TA=$TA FRAME=$FRAME"
  fi
  if [ "$SI" = 06 ]; then # System Information Type 5ter
    PWR=`echo "$FRAME" | cut -d " " -f 1`
    TA=`echo "$FRAME" | cut -d " " -f 2`
    FRAME=`echo "$FRAME"|tr -d " "`
    echo "SI5ter FIXME           FRAME=$FRAME"
  fi
  if [ "$SI" = 1e ]; then # System Information Type 6
    PWR=`echo "$FRAME" | cut -d " " -f 1`
    TA=`echo "$FRAME" | cut -d " " -f 2`
    FRAME=`echo "$FRAME"|tr -d " "`
    echo "SI6    PWR=$PWR TA=$TA FRAME=$FRAME"
  fi
done
