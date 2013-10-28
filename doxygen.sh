#!/bin/sh

DOXYGEN=$(which doxygen)

[ -z $DOXYGEN ] && exit 1

$DOXYGEN OMV*.php

exit 0
