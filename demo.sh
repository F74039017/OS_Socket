#!/bin/bash
make clean
make
if [ -d "cli" ]; then
	rm -rf cli
fi
if [ -d "ser" ]; then
	rm -rf ser
fi
mkdir ser
mkdir cli
cp server ser/server
cp client cli/client
	
