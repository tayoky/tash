# older versions of tash did not reset 
# the exit code if the while body did not execute
# posix mandate that if the body of a while never execute
# then the exit code must be 0


false

while false ; do
	echo "no"
done

echo "$?"
