#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "tsh.h"

static void show_cwd(void){
	char cwd[256];
	getcwd(cwd,256);

	char *home = getenv("HOME");
	if(!home){
		home = "/home";
	}

	//remove trailing /
	if(home[0] && home[strlen(home)-1] == '/'){
		home[strlen(home)-1] = '\0';
	}

	if(!memcmp(home,cwd,strlen(home))){
		memmove(&cwd[1],&cwd[strlen(home)],strlen(cwd) - strlen(home) + 1);
		cwd[0] = '~';
	}
	fputs(cwd,stdout);
}

static void show_prompt(const char *ps){
	time_t t = time(NULL);
	struct tm tm;
	localtime_r(&t,&tm);
	while(*ps){
		if(*ps == '\\'){
			ps++;
			switch(*ps){
			case '\0':
				return;
			case 'a':
				putchar('\a');
				break;
			case 'e':
				putchar('\033');
				break;
			case 'n':
				putchar('\n');
				break;
			case 'r':
				putchar('\r');
				break;
			case 's':
				fputs("tash",stdout);
				break;
			case 't':
				printf("%d:%d:%d",tm.tm_hour,tm.tm_min,tm.tm_sec);
				break;
			case 'T':
				printf("%d:%d:%d",tm.tm_hour > 13?tm.tm_hour%12:tm.tm_hour,tm.tm_min,tm.tm_sec);
				break;
			case '@':
				fputs(tm.tm_hour > 12 ? "pm" : "am",stdout);
				break;
			case 'A':
				printf("%d:%d",tm.tm_hour,tm.tm_min);
				break;
			case 'V':
			case 'v':
				fputs(TASH_VERSION,stdout);
				break;
			case 'w':
				show_cwd();
				break;
			case '$':
				putchar(geteuid() == 0 ? '#' : '$');
				break;
			}
			ps++;
			continue;
		}
		putchar(*ps);
		ps++;
	}
}

void show_ps1(void){
	const char *ps1 = getvar("PS1");
	if(!ps1)ps1 = "\\$ ";
	show_prompt(ps1);
}

void show_ps2(void){
	const char *ps2 = getvar("PS2");
	if(!ps2)ps2 = "> ";
	show_prompt(ps2);
}
