# tash
**TASH** (tayoky's advanced shell) aim to be a small portable posix shell. Tash is also the default shell of [the Stanix operating system](https://github.com/tayoky/stanix)

## features
Somes of the features tash currently has :
- quote support
- if/else/elif statements
- for/while/until loops
- pipes
- && and || support
- subshell with ( ) and $( )
- command groups with { xxx ; }
- tilde expansion
- variables (assignements, temporary environement variables and expansion)
- redirections (only on commands)
- parameter expansion ${} with % # %% ## - = + and ? support
- case statements with dynamic expansion in cases
- async commands with & (without job control)
- functions with argument support
