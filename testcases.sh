#!/bin/bash

echo "Building"
./build.sh


if [ ! -d "output_files" ]; then
    mkdir output_files
else
	rm output_files/*
fi

for input in input_files/*.txt; do
    filename=$(basename "$input" .txt)

    # EP scheduler
    ./bin/interrupts_EP_101268848_101281787 "$input" 
    cp execution.txt "output_files/${filename}_EP.txt"

    # RR scheduler
    ./bin/interrupts_RR_101268848_101281787 "$input"
    cp execution.txt "output_files/${filename}_RR.txt"

    # EP+RR scheduler
    ./bin/interrupts_EP_RR_101268848_101281787 "$input"
    cp execution.txt "output_files/${filename}_EP_RR.txt"
done

echo "All testcases are done running"
