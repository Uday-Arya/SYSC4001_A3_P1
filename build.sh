#!/bin/bash

if [ ! -d "bin" ]; then
    mkdir bin
else
	rm bin/*
fi

g++ -g -O0 -I . -o bin/interrupts_EP_101268848_101281787.cpp interrupts_EP_101268848_101281787.cpp
g++ -g -O0 -I . -o bin/interrupts_RR_101268848_101281787.cpp interrupts_RR_101268848_101281787.cpp
g++ -g -O0 -I . -o bin/interrupts_EP_RR_101268848_101281787.cpp interrupts_EP_RR_101268848_101281787.cpp