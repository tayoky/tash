#include <ctype.h>
#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include <stdlib.h>
#include <string.h>
#ifdef WCHAR_SUPPORT
#include <wchar.h>
#endif
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include "tsh.h"

static struct termios old;
static size_t prompt_cursor = 0;
static size_t prompt_index  = 0;
static size_t prompt_len = 0;
#ifdef WCHAR_SUPPORT
static wchar_t prompt_buf[256];
static int prompt_subindex;
#else
static char prompt_buf[256];
#endif

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
		if(*ps != '\\'){
			putchar(*ps);
			ps++;
			continue;
		}
		// we have an escape sequence
		ps++;
		switch(*ps){
		case '\0':
			break;
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
			fputs("tash", stdout);
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
			//TODO : use pwd ?
			break;
		case '$':
			putchar(geteuid() == 0 ? '#' : '$');
			break;
		}
		ps++;
	}

	fflush(stdout);
}

static void show_ps1(void){
	const char *ps1 = getvar("PS1");
	if (!ps1) ps1 = "\\$ ";
	show_prompt(ps1);
}

static void show_ps2(void) {
	const char *ps2 = getvar("PS2");
	if (!ps2) ps2 = "> ";
	show_prompt(ps2);
}

static void print_prompt(void) {
	switch (prompt) {
	case 1:
		show_ps1();
		break;
	case 2:
		show_ps2();
		break;
	}
}

#define ESC "\033"

static void move(int m){
	if(!m)return;
	struct winsize win;
	if(ioctl(STDOUT_FILENO,TIOCGWINSZ,&win) < 0){
		win.ws_col = 80;
	}
	if(m < 0){
		for(int i=0; i>m; i--){
			putchar('\b');
		}
	} else {
		for(int i=0; i<m; i++){
#ifdef WCHAR_SUPPORT
			putwchar(prompt_buf[prompt_cursor+i]);
#else
			putchar(prompt_buf[prompt_cursor+i]);
#endif
		}
	}

	prompt_cursor += m;
}

//redraw from cursor to the end
static void redraw(){
	//put a space at the end
	//which allow to clear one char further
	prompt_buf[prompt_len] = ' ';
	int old = prompt_cursor;
	move(prompt_len-prompt_cursor+1);
	move(old-prompt_cursor);
	fflush(stdout);
}

static int alpha_sort(const void *e1,const void *e2){
	const char *const *str1 = e1;
	const char *const *str2 = e2;
	return strcmp(*str1,*str2);
}

static char **autocomplete(char *str){
	char *file = strrchr(str,'/') ? strrchr(str,'/') + 1: str;
	char *dir = file == str ? strdup("")  : strndup(str,file - str);
	DIR *dirfd = opendir(!strcmp(dir,"") ? "." : dir);
	if(!dirfd){
		free(dir);
		return NULL;
	}

	char **fill = NULL;
	size_t count = 0;
	for(;;){
		struct dirent *ent = readdir(dirfd);
		if(!ent)break;
		if(!strncmp(ent->d_name,file,strlen(file))){
			fill = realloc(fill,sizeof(char*)*(count+1));
			fill[count] = malloc(strlen(dir)+strlen(ent->d_name)+2);
			sprintf(fill[count],"%s%s",dir,ent->d_name);
			struct stat st;
			if(stat(fill[count],&st) >= 0 && S_ISDIR(st.st_mode)){
				strcat(fill[count],"/");
			}
			count++;
		}
	}
	closedir(dirfd);
	free(dir);
	fill = realloc(fill,sizeof(char*)*(count+1));
	fill[count] = NULL;
	return fill;
}

static void setup_termios(void) {
	if(tcgetattr(STDIN_FILENO,&old) < 0){
		perror("tcgetattr");
	}
	struct termios new = old;
	new.c_lflag &= ~(ICANON | ECHO);
	if(tcsetattr(STDIN_FILENO,TCSANOW,&new) < 0){
		perror("tcsetattr");
	}
}

static void restore_termios(void) {
	tcsetattr(STDIN_FILENO,TCSANOW,&old);
}

static void handle_escape(void) {
	int c1 = getchar();
	if(c1 != '['){
		ungetc(c1,stdin);
		return;
	}
	int c2 = getchar();
	switch(c2){
	case 'D':
		if(prompt_cursor <= 0){
			putchar('\a');
			break;
		}
		move(-1);
		break;
	case 'C':
		if(prompt_cursor >= prompt_len){
			putchar('\a');
			break;
		}
		move(1);
		break;
	case 'H':
		move(-prompt_cursor);
		break;
	case 'F':
		move(prompt_len-prompt_cursor);
		break;
	}
}

static void reprint_prompt(void) {
	print_prompt();
	int old_cursor = prompt_cursor;
	prompt_cursor = 0;
	redraw();
	move(old_cursor);
}

static void handle_autocompletion(void) {
	// first convert to mb char
#ifdef WCHAR_SUPPORT
	char buf[sizeof(prompt_buf)/sizeof(wchar_t)*MB_CUR_MAX];
	prompt_buf[prompt_len] = '\0';
	wcstombs(buf, prompt_buf, sizeof(buf));
	size_t len = strlen(buf);
	const wchar_t *src = prompt_buf;
	size_t cursor = wcsnrtombs(NULL, &src, prompt_cursor, 0, NULL);
#else
#define buf prompt_buf
#define len prompt_len
#define cursor prompt_cursor
#endif
	// find the start and end of current arg
	int search_command = 0;
	size_t start = cursor > 0 ? cursor - 1 : 0;
	while(start > 0 && !isblank(buf[start])){
		start--;
	}
	if(isblank(buf[start]) && start < cursor){
		start++;
	}
	size_t end = cursor;
	while(end < len && !isblank(buf[end])){
		end++;
	}
	char search[end-start+1];
	memcpy(search,&buf[start],end-start);
	search[end-start] = '\0';

	char **fill = autocomplete(search);
	if(!fill || !*fill){
		return;
	}

	// try to find something in common in all possibilities
	size_t common = strlen(fill[0]);
	for(size_t i=0; fill[i]; i++){
		if(strncmp(fill[0],fill[i],common)){
			for(size_t j=0; j < common; j++) {
				if(fill[0][j] != fill[i][j]){
					common = j;
					break;
				}
			}
		}
	}

	if(common <= strlen(search) || !common){
		// we have mulitpes possibilities
		// print all of them
	
		// find the biggest string
		size_t cell_size = 0;
		size_t count= 0;
		for(size_t i=0; fill[i]; i++){
			count++;
			if(strlen(fill[i]) > cell_size){
				cell_size = strlen(fill[i]);
			}
		}
		cell_size++;
		struct winsize win;
		if(ioctl(STDOUT_FILENO,TIOCGWINSZ,&win) < 0){
			win.ws_col = 80;
		}

		size_t cell_per_line = win.ws_col / cell_size;

		// sort in aphabetic
		qsort(fill,count,sizeof(char *),alpha_sort);

		// print time
		putchar('\n');
		for(size_t i=0; i<count; i++){
			fputs(fill[i],stdout);
			for(size_t j = strlen(fill[i]); j<cell_size; j++) putchar(' ');
			if(i % cell_per_line  == cell_per_line - 1 && i < count - 1){
				putchar('\n');
			}
		}

		putchar('\n');

		reprint_prompt();

		return;
	}

	memmove(&buf[start + common], &prompt_buf[end], len - end);
	len += common - (end - start);
	memcpy(&buf[start], fill[0], common);

#ifdef WCHAR_SUPPORT
	buf[len] = 0;
	if (mbstowcs(NULL, buf, 0) + 1 > sizeof(prompt_buf)/sizeof(wchar_t)) {
		// no place
		putchar('\n');
		error("no place in prompt buffer");
		reprint_prompt();
		return;
	}
	mbstowcs(prompt_buf, buf, sizeof(prompt_buf)/sizeof(wchar_t));
	prompt_len = wcslen(prompt_buf);
	const char *buf_src = buf;
	size_t cursor_end = mbsnrtowcs(NULL, &buf_src, start + common, 0, NULL);
	move(cursor_end - prompt_cursor);
#else
	move(start + common - prompt_cursor);
#endif
}

static void do_prompt(void) {
	print_prompt();
	setup_termios();

	// this is were the magic happend
	prompt_index = 0;
	prompt_cursor = 0;
	prompt_len = 0;
#ifdef WCHAR_SUPPORT
	prompt_subindex = 0;
#endif

	for(;;){
		fflush(stdout);
#ifdef WCHAR_SUPPORT
		wint_t c = getwchar();
#else
		int c = getchar();
#endif
		if(c == old.c_cc[VEOF]){
			if(prompt_len > 0)continue;
			prompt_buf[0] = 0;
			prompt_len++;
			goto finish;
		}
		switch(c){
		case '\033':
			handle_escape();
			break;
		case '\t':
			handle_autocompletion();
			break;
		case '\n':
			putchar('\n');
			prompt_len++;
			goto finish;
		case 127:
			if(prompt_cursor <= 0){
				putchar('\a');
				break;
			}
			move(-1);
			memmove(&prompt_buf[prompt_cursor],&prompt_buf[prompt_cursor+1],(prompt_len-prompt_cursor) * sizeof(*prompt_buf));
			prompt_len--;
			redraw();
			break;
		default:;
			memmove(&prompt_buf[prompt_cursor+1],&prompt_buf[prompt_cursor],(prompt_len-prompt_cursor) * sizeof(*prompt_buf));
			prompt_buf[prompt_cursor] = c;
			prompt_len ++;
			redraw();
			prompt_cursor ++;
#ifdef WCHAR_SUPPORT
			putwchar(c);
#else
			putchar(c);
#endif
			break;
		}

	}
finish:
	fflush(stdout);
	restore_termios();

}

int prompt_getc(void){
	if (prompt_index >= prompt_len) {
		// nothing left
		do_prompt();
	}
	if(prompt_buf[prompt_index] == 0){
		prompt_index++;
		return EOF;
	}
#ifdef WCHAR_SUPPORT
	char buf[MB_CUR_MAX];
	int len = wctomb(buf, prompt_buf[prompt_index]);
	unsigned char ret = (unsigned char)buf[prompt_subindex];
	prompt_subindex++;
	if (prompt_subindex >= len) {
		prompt_subindex = 0;
		prompt_index++;
	}
	return ret;
#else
	return (unsigned char)prompt_buf[prompt_index++];
#endif
}
