#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <tash.h>

static int builtin_set(int argc, char **argv) {
	for (int i=1; i<argc; i++) {
		int mask  = 0;
		char *f = argv[i];
		if (!*f) goto invalid;
		f++;
		while (*f) {
			switch (*f) {
			case 'e':
				mask |= TASH_ERR_EXIT;
				break;
			case 'u':
				mask |= TASH_UNSET_EXIT;
				break;
			case 'm':
				mask |= TASH_JOB_CONTROL;
				break;
			case 'f':
				mask |= TASH_NO_GLOBING;
				break;
			default:
				goto invalid;
			}
			f++;
		}
		switch (argv[i][0]) {
		case '-':
			flags |= mask;
			break;
		case '+':
			flags &= ~mask;
			break;
		default:
			goto invalid;
		}
		if (mask & TASH_JOB_CONTROL) {
			job_control_setup();
		}
		continue;
invalid:
		error("set : invalid option : '%s'",argv[i]);
		return 1;
	}
	return 0;
}

static int builtin_exit(int argc, char **argv) {
	if (argc > 2) {
		error("exit : too many arguments");
		return 1;
	} else if (argc == 2) {
		char *end;
		int status = strtol(argv[1],&end,10);
		if(end == argv[1] || *end){
			error("exit : numeric argument required");
			exit(2);
		}
		exit(status);
	}

	exit(exit_status);
}

static int builtin_export(int argc, char **argv) {
	if (argc < 2 || !strcmp(argv[1], "-p")) {
		for (size_t i=0; i<vars_count; i++) {
			if (vars[i].exported) printf("%s=%s\n", vars[i].name, vars[i].value);
		}
		return 0;
	}

	int ret = 0;
	int i = 1;
	while (i < argc) {
		if (strchr(argv[i], '=')) {
			char *name = argv[i];
			char *value = strchr(name, '=');
			*value = '\0';
			value++;
			putvar(name, value);
			export_var(name);
		} else {
			export_var(argv[i]);
		}
		i++;
	}
	return ret;
}

static int builtin_cd(int argc, char **argv) {
	if (argc > 2) {
		error("cd : too many arguments");
		return 1;
	} else if(argc == 2){
		if (chdir(argv[1]) < 0) {
			perror(argv[1]);
			return 1;
		}
		char buf[256];
		if (getcwd(buf, sizeof(buf))) {
			putvar("PWD", buf);
		}
		return 0;
	} else {
		const char *home = getvar("HOME");
		if (!home) {
			error("cd : HOME not set");
			return 1;
		}
		if (chdir(home) < 0) {
			perror(home);
			return 1;
		}
		putvar("PWD", home);
		return 0;
	}
}

static int builtin_src(int argc, char **argv) {
	//TODO : search in PATH first ???
	if (argc < 2) {
		error("source : missing argument");
		return 1;
	}

	// save argc/argv to restore after
	int old_argc = _argc;
	char **old_argv = _argv;
	char *old_argv0 = _argv0;
	_argc = argc - 2;
	_argv = argv + 2;
	_argv0 = argv[1];
	stack_depth++;
	eval_script(argv[1]);
	stack_depth--;
	_argc = old_argc;
	_argv = old_argv;
	_argv0 = old_argv0;
	return exit_status;
}

static int builtin_echo(int argc, char **argv) {
	int newline = 1;
	int i=1;
	if (argc > 2 && !strcmp(argv[1], "-n")) {
		newline = 0;
		i++;
	}

	for (; i<argc; i++) {
		int ret;
		if (i == argc - 1) {
			ret = printf("%s", argv[i]);
		} else {
			ret = printf("%s ", argv[i]);
		}
		if (ret < 0) {
			perror("write");
			return 1;
		}
	}
	if (newline) {
		if (putchar('\n') == EOF) {
			// HACK workaround for a weird stanix bug
#ifndef __stanix__
			perror("write");
			return 1;
#endif
		}
	}
	return 0;
}

static int builtin_eval(int argc, char **argv) {
	size_t total = 1;
	for (int i=1; i<argc; i++) {
		total += strlen(argv[i]) + 1;
	}
	char *buf = xmalloc(total);
	char *ptr = buf;
	for (int i=1; i<argc; i++) {
		strcpy(ptr, argv[i]);
		ptr += strlen(argv[i]);
		*(ptr++) = ' ';
	}
	*ptr = '\0';

	eval(buf);

	xfree(buf);
	return exit_status;
}

static int builtin_true(int argc, char **argv) {
	(void)argc;
	(void)argv;
	return 0;
}

static int builtin_false(int argc, char **argv) {
	(void)argc;
	(void)argv;
	return 1;
}

static int builtin_break(int argc, char **argv) {
	if (loop_depth == 0) {
		error("break : 'break' only work in 'while', 'until' and 'for' loops");
		return 0;
	}
	if (argc < 2) {
		break_depth = 1;
		return 0;
	} else if (argc == 2) {
		char *end;
		break_depth = strtol(argv[1], &end, 10);
		if (end == argv[1] || *end) {
			error("break : numeric argument required");
			return 1;
		}
		if (break_depth > loop_depth) break_depth = loop_depth;
		return 0;
	} else {
		error("break : too many arguments");
		return 1;
	}
}

static int builtin_continue(int argc, char **argv) {
	if (loop_depth == 0) {
		error("continue : 'continue' only work in 'while', 'until' and 'for' loops");
		return 0;
	}
	if (argc < 2) {
		continue_depth = 1;
		return 0;
	} else if (argc == 2) {
		char *end;
		continue_depth = strtol(argv[1], &end, 10);
		if (end == argv[1] || *end) {
			error("continue : numeric argument required");
			return 1;
		}
		if (continue_depth > loop_depth) continue_depth = loop_depth;
		return 0;
	} else {
		error("continue : too many arguments");
		return 1;
	}
}

static int builtin_wait(int argc, char **argv) {
	exit_status = 0;
	if (argc < 2) {
		for (;;) {
			if (job_wait_pid(-1) < 0) {
				return 0;
			}
		}
	} else {
		for (int i=1; i<argc; i++) {
			char *end;
			pid_t pid = (pid_t)strtol(argv[i], &end, 10);
			if (end == argv[i] || *end) {
				error("wait : invalid pid '%s'", argv[i]);
				exit_status = 1;
				continue;
			}
			if (job_wait_pid(pid) < 0) {
				exit_status = 127;
				if (errno == ECHILD) {
					error("wait : %ld is not a child process", (long)pid);
				} else {
					perror(argv[i]);
				}
				continue;
			}
		}
		return exit_status;
	}
}

static int builtin_return(int argc, char **argv) {
	if (stack_depth == 0) {
		error("return : 'return' only work in sourced scripts and functions");
		return 1;
	}
	if (argc > 2) {
		error("return : too many arguments");
		return 1;
	} else if (argc == 2) {
		char *end;
		int status = strtol(argv[1],&end,10);
		if(end == argv[1] || *end){
			error("return : numeric argument required");
			exit_status = 2;
		}
		exit_status = status;
	}
	return_break = 1;
	return exit_status;
}

static int builtin_shift(int argc, char **argv) {
	int amount;
	if (argc < 2) {
		amount = 1;
	} else if (argc == 2) {
		char *end;
		amount = strtol(argv[1], &end, 10);
		if (end == argv[1] || *end) {
			error("shift : numeric argument required");
			return 1;
		}
	} else {
		error("shift : too many arguments");
		return 1;
	}
	if (amount < 0) {
		return 1;
	}
	if (amount > _argc) {
		_argc = 0;
		return 1;
	}
	_argc -= amount;
	_argv += amount;
	return 0;
}

static int builtin_unset(int argc, char **argv) {
	int i=1;
	int do_func = 0;
	int do_var  = 0;
	while (i < argc && argv[i][0] == '-') {
		if (!strcmp(argv[i], "--")) {
			i++;
			break;
		}
		for (int k=1; argv[i][k]; k++) {
			switch (argv[i][k]) {
			case 'v':
				do_var = 1;
				break;
			case 'f':
				do_func = 1;
				break;
			default:
				error("unset : unknow options '-%c'", argv[i][k]);
				return 1;
			}
		}
		i++;
	}

	if (!do_var && !do_func) {
		// by default try the two
		do_var = 1;
		do_func = 1;
	}
		
	for (int j=i; j<argc; j++) {
		if (do_var && unset_var(argv[j])) continue;
		if (do_func) unregister_func(argv[j]);
	}
	return 0;
}

#define CMD(n,cmd) {.name = n,.func = (int (*)(int,char**))cmd}
static builtin_t builtin[] = {
	CMD("cd"      ,builtin_cd),
	CMD("exit"    ,builtin_exit),
	CMD("export"  ,builtin_export),
	CMD("source"  ,builtin_src),
	CMD("."       ,builtin_src),
	CMD("set"     ,builtin_set),
	CMD("echo"    ,builtin_echo),
	CMD("eval"    ,builtin_eval),
	CMD("true"    ,builtin_true),
	CMD("false"   ,builtin_false),
	CMD("break"   ,builtin_break),
	CMD("continue",builtin_continue),
	CMD("wait"    ,builtin_wait),
	CMD("return"  ,builtin_return),
	CMD("shift"   ,builtin_shift),
	CMD("unset"   ,builtin_unset),
};

// TODO handle SIGINT in builtins
int try_builtin(int argc,char **argv){
	for(size_t i=0; i<arraylen(builtin);i++){
		if(!strcmp(argv[0],builtin[i].name)){
			return builtin[i].func(argc,argv);
		}
	}

	return -1;
}
