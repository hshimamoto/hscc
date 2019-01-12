#!/bin/bash

try() {
	in="$1"
	ex="$2"
	echo "$in" > tmp.c
	./hscc tmp.c > tmp.s
	if [ $? -ne 0 ]; then
		echo "compile error"
		exit 1
	fi
	rm -f tmp
	gcc -o tmp tmp.s
	./tmp
	ret="$?"

	if [ "$ret" != "$ex" ]; then
		echo "$ex expected, but got $ret"
		exit 1
	fi
}

try '0;' 0
try '42;' 42
try '1+2;' 3
try '3-2;' 1
try '1+2+3;' 6
try '1+2-3;' 0
try '1+2+3+4+5+6+7+8+9+10;' 55
try '1+2+3+4+5+6+7+8+9 + 10;' 55
try '1   +2   -       3;' 0
try '3-2+1;' 2
try '2*3;' 6
try '4/2;' 2
try '(1+2)*3;' 9
try '((1+2)*5+5)/2;' 10
try '4/2*3;' 6
try 'a=1;b=2;a+b;' 3
try 'a=b=c=d=1;a+b+c+d;' 4
try '1==1;' 1
try '1!=1;' 0
try 'a=1;b=1;a+b == 2;' 1
try 'a=1;b=2;a+b == 2;' 0
try 'a=1;b=1;a & b;' 1
try 'a=1;b=2;a & b;' 0
try 'a=1;b=1;a ^ b;' 0
try 'a=1;b=2;a ^ b;' 3
try 'a=1;b=1;a | b;' 1
try 'a=1;b=4;a | b;' 5
try 'alpha=1;beta=2;alpha + beta;' 3

echo OK
