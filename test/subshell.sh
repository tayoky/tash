# test subshell substitution

echo $(ls ..)
#echo $(echo "(")
echo $(   ( echo yes && echo it work )
)

echo $(echo hi)
echo $(uname -m)
echo $(realpath $(dirname $0))
