# test unset builtin

TEST="hello"
echo "TEST is $TEST"
unset TEST
echo "TEST is $TEST"

TEST () {
	echo hello
}
unset -v TEST
TEST

unset -f TEST

# make sure unset is not just empty
HELLO=hello
echo "HELLO is $HELLO"
unset -v HELLO
echo "${HELLO:-"HELLO is unset"}"
