# stress test if

if true ; then
	echo hello
fi

if false ; then
	echo wrong
	if false ; then 
		echo still wrong
	fi
	if true ; then
		echo wrong again
	fi
fi

if true ; then
	if true ; then
		echo nested work !
	fi
fi

if 
	if true ; then
		echo hello
	fi ; then
	true
#	echo hello again
else 
	echo no
fi

if false ; then
	true
else
	echo yes
fi
