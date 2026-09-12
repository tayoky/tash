# in older tash versions, a negate inside a subshell gave issues
# because of the no fork optimisation the subshell would direcly exec content of the negate
# and so the negate habdling code could not inverse the exit code

(! cat /do-not-exist.txt 2>/dev/null)
echo $?
