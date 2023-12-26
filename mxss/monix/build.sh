#!/bin/bash

if [ $# -eq 0 ]; then
    echo "Usage: $0 <architecture>"
    exit 1
fi

# Set architecture.
ARCH=$1

# push cwd to script dir
pushd `dirname $0` > /dev/null

rm -rf obj/$ARCH
rm -rf bin/$ARCH

mkdir -p obj/$ARCH
mkdir -p bin/$ARCH

if [ -z "$CXX" ]; then
    if [ -x "$(command -v $ARCH-linux-gnu-g++)" ]; then
        CXX=$ARCH-linux-gnu-g++
    elif [ -x "$(command -v $ARCH-linux-musl-g++)" ]; then
        CXX=$ARCH-linux-musl-g++
    elif [ -x "$(command -v $ARCH-linux-g++)" ]; then
        CXX=$ARCH-linux-g++
    elif [ -x "$(command -v $ARCH-linux-gnu-clang++)" ]; then
        CXX=$ARCH-linux-gnu-clang++
    elif [ -x "$(command -v $ARCH-linux-musl-clang++)" ]; then
        CXX=$ARCH-linux-musl-clang++
    elif [ -x "$(command -v $ARCH-linux-clang++)" ]; then
        CXX=$ARCH-linux-clang++
    elif [ -x "$(command -v g++)" ]; then
        echo "Cannot find architecture specific compiler. Using default g++."
        CXX=g++
    else
        echo "No compatible compiler for the specified architecture found."
        exit 1
    fi
fi

mkdir -p obj/$ARCH/bin
for filename in src/*; do
    filename=$(basename $filename)
    filenamenoext="${filename%.*}"
    $CXX src/$filename                      \
        -o obj/$ARCH/bin/$filenamenoext     \
        -I ../../lxmonika/include           \
        -nostdlib -static -s                \
        -Werror -Wall -Wextra -Wpedantic    \
        -Wno-multichar                      \
        -std=c++23 -O2                      \
        -mno-red-zone
done

mkdir -p bin/$ARCH

tar -C obj/$ARCH -czf bin/$ARCH/monix-$ARCH.tar.gz .

popd > /dev/null
