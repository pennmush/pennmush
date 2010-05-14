#!/bin/sh

# Wrapper script for running cscope.
# From the root pennmush directory: utils/cscope.sh

exec cscope *.h hdrs/*.h src/*.c
