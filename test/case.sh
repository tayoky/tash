# test case

case test in
	wrong)
		echo wrong
		;;
	(test)
		echo yes
esac
echo $?

TEST=test2
case $TEST in
	test?)
		echo right
		false
		;;
esac
echo $?
