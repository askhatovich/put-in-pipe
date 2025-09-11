#!/bin/bash

if [ "$#" -ne 2 ]; then
    echo "Usage: $0 pictures_dir output_file"
    exit 1
fi

input_dir="$1"
output_file="$2"

if [ ! -d "$input_dir" ]; then
    echo "Error: input_dir not exists"
    exit 1
fi

echo "// Created with create_bin_pics_cpp.sh" > $output_file
echo "" >> $output_file
for image in "$input_dir"/*.png; do
    if [ -f "$image" ]; then
        filename=$(basename "$image")
        ./a.out $image ${filename%.*} >> $output_file
    fi
done

