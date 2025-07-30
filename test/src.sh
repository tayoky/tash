# test . built in
# TODO : this test is highly broken on tash

if [ "$1" = "test" ] ; then
	export TEST=hello
	exit
fi

export TEST=""
#. ./src.sh test
#echo $TEST
