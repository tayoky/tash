#!/bin/sh

#compare two s output
SHELL1=bash
SHELL2=tash
cd test

rm -f $(find -not -name "*.sh" -type f)

for TEST in $(ls) ; do
	$SHELL1 $TEST > $TEST-$SHELL1.out
	$SHELL2 $TEST > $TEST-$SHELL2.out
	if diff -s $TEST-$SHELL1.out $TEST-$SHELL2.out > /dev/null ; then
		echo $TEST passed
	else
		echo $TEST failed
		diff $TEST-$SHELL1.out $TEST-$SHELL2.out --color
		exit 1
	fi
done
