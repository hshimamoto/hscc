#!/bin/bash

try() {
	in="$1"
	ex="$2"
	./hscc "$in" > tmp.s
	gcc -o tmp tmp.s
	./tmp
	ret="$?"

	if [ "$ret" != "$ex" ]; then
		echo "$ex expected, but got $ret"
		exit 1
	fi
}

try 0 0
try 42 42
try 1+2 3
try 3-2 1

echo OK