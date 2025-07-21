# test . built in

if [ "$1" = "test" ] ; then
	export TEST=hello
	exit
fi

export TEST=""
. ./src.sh test
echo $TEST
