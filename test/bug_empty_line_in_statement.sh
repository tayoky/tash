# some tash versions triggered syntax error 
# if the line before the then/elif/else/fi/do/done/... was empty or contained a comment

if true ; then
	echo hello

fi

if false ; then
	echo wrong
	# comment
fi

until true

do

echo nothing

done
