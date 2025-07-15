#include <ctype.h>
#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>
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

static void move(int m){
	if(m < 0){
		for(int i=0; i>m; i--){
			putchar('\b');
		}
	} else {
		for(int i=0; i<m; i++){
			putchar(prompt_buf[prompt_cursor+i]);
		}
	}
	prompt_cursor += m;
}

int prompt_getc(void){
	if(prompt_index < prompt_len){
		if(prompt_buf[prompt_index] == 0){
			prompt_index++;
			return EOF;
		}
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
		if(c == new.c_cc[VEOF]){
			if(prompt_len >0)continue;
			prompt_buf[0] = 0;
			prompt_len++;
			goto finish;
		}
		switch(c){
		case '\033':;
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
				move(-1);
				fflush(stdout);
				break;
			case 'C':
				if(prompt_cursor >= prompt_len){
					putchar('\a');
					break;
				}
				move(1);
				fflush(stdout);
				break;
			case 'H':
				move(-prompt_cursor);
				break;
			case 'F':
				move(prompt_len-prompt_cursor);
				break;
			}
			break;
		case '\t':{
			//TODO : auto completion
			//find the start and end of current arg
			int search_command = 0;
			int start = prompt_cursor > 0 ? prompt_cursor - 1 : 0;
			while(start > 0 && !isblank(prompt_buf[start])){
				start--;
			}
			if(isblank(prompt_buf[start]) && start < prompt_cursor){
				start++;
			}
			int end = prompt_cursor;
			while(end < prompt_len && !isblank(prompt_buf[end])){
				end++;;
			}
			char search[end-start+1];
			memcpy(search,&prompt_buf[start],end-start);
			search[end-start] = '\0';
			char *file = strrchr(search,'/') ? strrchr(search,'/') + 1: search;
			char *dir = file == search ? strdup("./") : strndup(search,file - search);
			DIR *dirfd = opendir(dir);
			if(!dirfd){
				free(dir);
				continue;
			}

			char *fill = NULL;
			for(;;){
				struct dirent *ent = readdir(dirfd);
				if(!ent)break;
				if(!strncmp(ent->d_name,file,strlen(file))){
					fill = malloc(strlen(dir)+strlen(ent->d_name)+2);
					sprintf(fill,"%s%s",dir,ent->d_name);
					struct stat st;
					if(stat(fill,&st) >= 0 && S_ISDIR(st.st_mode)){
						strcat(fill,"/");
					}
					break;
				}
			}

			if(fill){
				if(!strncmp("./",fill,2)){
					char *simp = strdup(fill+2);
					free(fill);
					fill = simp;
				}
				memmove(&prompt_buf[start + strlen(fill)],&prompt_buf[start + end],prompt_len - (end - start));
				prompt_len += strlen(fill) - (end - start);
				strcpy(&prompt_buf[start],fill);
				move(start-prompt_cursor);
				redraw();
				move(start+strlen(fill)-prompt_cursor);
				fflush(stdout);
			}
			closedir(dirfd);
			free(dir);

			continue;
		}
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
			move(-1);
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
