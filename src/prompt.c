#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "tsh.h"

struct termios old;
char prompt_buf[256];
int prompt_cursor = 0;
int prompt_index  = 0;
int prompt_len = 0;

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
			case 'W':
			case 'w':
				show_cwd();
				break;
			case 'h':
				printf("host");
				break;
			case 'u':
				if(getenv("LOGNAME"))printf("%s",getenv("LOGNAME"));
				printf("user");
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

	fflush(stdout);
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

#define ESC "\033"

//redraw from cursor to the end
static void redraw(){
	for(int i=prompt_cursor; i<prompt_len; i++){
		putchar(prompt_buf[i]);
	}
	putchar(' ');
	putchar('\b');
	for(int i=prompt_cursor; i<prompt_len; i++){
		putchar('\b');
	}
	fflush(stdout);
}

int prompt_getc(void){
	if(prompt_index < prompt_len){
		return (unsigned char)prompt_buf[prompt_index++];
	}
	if(tcgetattr(STDIN_FILENO,&old) < 0){
		perror("tcgetattr");
		return fgetc(stdin);
	}
	struct termios new = old;
	new.c_lflag &= ~(ICANON | ECHO);
	if(tcsetattr(STDIN_FILENO,TCSANOW,&new) < 0){
		perror("tcsetattr");
		return fgetc(stdin);
	}

	//this is were the magic happend
	prompt_index = 0;
	prompt_cursor = 0;
	prompt_len = 0;
	for(;;){
		int c = getchar();
		switch(c){
		case '\033':
			int c1 = getchar();
			if(c1 != '['){
				ungetc(c1,stdin);
				continue;
			}
			int c2 = getchar();
			switch(c2){
			case 'D':
				if(prompt_cursor <= 0){
					putchar('\a');
					break;
				}
				prompt_cursor--;
				putchar('\b');
				fflush(stdout);
				break;
			case 'C':
				if(prompt_cursor >= prompt_len){
					putchar('\a');
					break;
				}
				putchar(prompt_buf[prompt_cursor]);
				prompt_cursor++;
				fflush(stdout);
				break;
			}
			break;
		case '\t':
			//TODO : auto completion
			continue;
		case '\n':
			prompt_buf[prompt_len] = c;
			while(prompt_cursor < prompt_len){
				putchar(prompt_buf[prompt_cursor++]);
			}
			putchar('\n');
			prompt_len++;
			goto finish;
		case 127:
			if(prompt_cursor <= 0){
				putchar('\a');
				break;
			}
			prompt_cursor--;
			putchar('\b');
			memmove(&prompt_buf[prompt_cursor],&prompt_buf[prompt_cursor+1],prompt_len-prompt_cursor);
			prompt_len--;
			redraw();
			fflush(stdout);
			break;
		default:
			memmove(&prompt_buf[prompt_cursor+1],&prompt_buf[prompt_cursor],prompt_len-prompt_cursor);
			prompt_buf[prompt_cursor] = c;
			prompt_len++;
			redraw();
			prompt_cursor++;
			putchar(c);
			fflush(stdout);
			break;
		}

	}
finish:
	tcsetattr(STDIN_FILENO,TCSANOW,&old);
	return prompt_getc();

}

void prompt_unget(int c){
	if(c != EOF)prompt_index--;
}
