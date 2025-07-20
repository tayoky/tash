# tash
tash (Tayoky's Advanced SHell) is a portable shell made to be small posix compliant and eay to use  
one of its final goal is to be as complete as bash (or at least pretty close)

# features
- shell scripting
- strings (with " and ')
- environ variables ($$ and other)
- output/input redirections (with <, > and >>)
- backslash support
- builtin commands such as cd exit or export
- basic signal control (SIGINTR and stuff)
- pipes
- variables
- if/else
- basic auto completion for filename

# soon
features that will be added soon
- for loop
- subshell

# build
```sh
./configure
```
then 
```sh
make
```
you can install with
```sh
make install
```
that might require super user permission
