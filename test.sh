#!/bin/bash

try() {
	ex=$1
	in=$2
	./hscc "$in" > tmp.s
	gcc -o tmp tmp.s
	./tmp
	ret=$?

	if [ "$ret" != "$ex" ]; then
		echo "$ex expected, but got $ret"
		exit 1
	fi
}

try 0 0
try 42 42

echo OK
