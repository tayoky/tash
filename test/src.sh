# test . built in

# start by generating the script
echo "#script sourced by the . test" > src
echo "export TEST=hello" >> src

export TEST=""

# then source it
. ./src
echo $TEST
