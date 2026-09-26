# older versions of tash did not reset 
# the exit code if the if statement did not execute
# posix mandate that if no branch of a if statement is executed
# then the exit code must be 0


false

if false ; then
	echo wrong
fi

echo "$?"

if false ; then
	echo wrong again
elif false ; then
	echo still wrong
fi

echo "$?"

if false ; then
	true
else
	false
fi

echo "$?"
