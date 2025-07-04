#!/bin/sh
cd test

rm -f $(find -not -name "*.sh" -type f)

for TEST in $(ls) ; do
	sh $TEST > $TEST-sh.out
	tash $TEST > $TEST-tash.out
	if diff -s $TEST-tash.out $TEST-sh.out > /dev/null ; then
		echo $TEST passed
	else
		echo $TEST failed
		diff $TEST-tash.out $TEST-sh.out --color
		exit 1
	fi
done
