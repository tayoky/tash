# test suffix/prefix removing

FILE1=src.c
FILE2=archive.tar.xz

echo ${FILE1} ${FILE1%".c"} ${FILE1%.*} ${FILE1%%.*}
echo ${FILE2} ${FILE2%".xz"} ${FILE2%.*} ${FILE2%%.*}
