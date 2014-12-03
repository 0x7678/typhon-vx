#!/bin/bash

echo ComplexAtiSwitchingScript
echo "(c) 1992-2015 Jan Hrach"
echo License number: 13718-00694-16489-86891-89956
echo Licensed to: brmlab o.s.
echo Unauthorized distribution of this software is PROHIBITED
echo For pricing, contact jenda@hrach.eu
echo

if [ -L A5Ati.so ]; then
  unlink A5Ati.so
  echo ATI is now DISABLED
else
  ln -s ../a5_ati/A5Ati.so .
  echo ATI is now ENABLED
fi
