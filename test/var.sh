# test variables

TEST=hello
echo path is $PATH
echo 'home is '$HOME
echo PATH is ${PATH}
echo test is $TEST
echo test len is ${#TEST}

HELLO=
echo test :- ${HELLO:-"stuff"}
echo test :- ${HELLO:-../sr?}
echo test := ${HELLO:="hello"}
echo HELLO is ${HELLO}

# a pretty weird case
# in case one no globing
# in case two globing work (cause it set TESTB then expand to TESTB without quotes)
echo glob case 1 ${TESTA:-"../sr?"}
echo glob case 2 ${TESTB:="../sr?"}
