# test functions
func () {
	echo hi
}

func
func

if false ; then
	func2 () (echo bad)
else
	func2 ()
	(echo good)
fi
