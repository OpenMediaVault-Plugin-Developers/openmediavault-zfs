#!/bin/sh

DOXYGEN=$(which doxygen)

[ -z $DOXYGEN ] && exit 1

$DOXYGEN Doxyfile 

exit 0
