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

try_expr() {
	in="$1"
	ex="$2"
	try "int main() { $in }" $ex
}

try_expr '0;' 0
try_expr '42;' 42
try_expr '1+2;' 3
try_expr '3-2;' 1
try_expr '1+2+3;' 6
try_expr '1+2-3;' 0
try_expr '1+2+3+4+5+6+7+8+9+10;' 55
try_expr '1+2+3+4+5+6+7+8+9 + 10;' 55
try_expr '1   +2   -       3;' 0
try_expr '3-2+1;' 2
try_expr '2*3;' 6
try_expr '4/2;' 2
try_expr '(1+2)*3;' 9
try_expr '((1+2)*5+5)/2;' 10
try_expr '4/2*3;' 6
try_expr '1+(2+(3+(4+5)));' 15
try_expr 'int a;int b;a=1;b=2;a+b;' 3
try_expr 'int a; int b; int c; int d; a=b=c=d=1;a+b+c+d;' 4
try_expr '1==1;' 1
try_expr '1!=1;' 0
try_expr 'int a;int b;a=1;b=1;a+b == 2;' 1
try_expr 'int a;int b;a=1;b=2;a+b == 2;' 0
try_expr 'int a;int b;a=1;b=1;a & b;' 1
try_expr 'int a;int b;a=1;b=2;a & b;' 0
try_expr 'int a;int b;a=1;b=1;a ^ b;' 0
try_expr 'int a;int b;a=1;b=2;a ^ b;' 3
try_expr 'int a;int b;a=1;b=1;a | b;' 1
try_expr 'int a;int b;a=1;b=4;a | b;' 5
try_expr 'int alpha;int beta;alpha=1;beta=2;alpha + beta;' 3
try_expr 'int A;int B;int C;int tEmP;A=1;B=2;C=3;tEmP=4; A+B+C+tEmP;' 10

try 'int func(){int a;int b;a=40;b=2;a+b;} int main(){func();}' 42
try 'int func(){int a;int b;a=20;b=2;a+b;} int main(){20+func();}' 42
try 'int func1(){int a;int b;a=20;b=2;a+b;}  int func2(){20;} int main(){func1() + func2();}' 42
try 'int func1(){int a;int b;a=20;b=1;a+b;}  int func2(){20;} int main(){int a;a=1; func1() + func2() + a;}' 42
try 'int func1(){10;} int func2(){func1()*(func1()+func1());} int func3(){int a;int b;a=2;b=3;func2()+a*b-func2();} int main(){func3();}' 6
try 'int f(){10;} int main(){f()+(f()+(f()+(f()+2)));}' 42
try 'int f(int a,int b,int c){a*b+c;} int main(){int A;int B;int C;A=8;B=5;C=2;f(A,B,C);}' 42
try 'int f(int a,int b,int c){a*b+c;} int main(){int A;int B;int C;A=2;B=8;C=5;f(B,C,A);}' 42
try 'int f(int a,int b,int c,int d,int e,int f){a+b+c+d+e+f;} int main(){int A;int B;int C;A=5;B=10;C=15;f(A,B,C, A,B,C);}' 60

try 'int main() {int s0; int s1;s0="Hello";s1="world";strcmp(s0,s1) != 0;}' 1

echo OK
