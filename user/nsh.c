#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"

#define MAXARGS 64

struct cmd{
    char *infile;
    char *einfile;
    char *outfile;
    char *eoutfile;
    char *argv[MAXARGS];
    char *eargv[MAXARGS];
};

static struct cmd lcmd;
static struct cmd rcmd;


char whitespace[] = " \t\r\n\v";
char symbols[] = "<|>";

void panic(char *s){
    fprintf(2, "%s\n", s);
    exit(-1);
}

int getcmd(char *buf, int nbuf) {
    fprintf(2, "@ ");
    memset(buf, 0 , nbuf);
    gets(buf, nbuf);
    if(buf[0] == 0){
        return -1;
    }
    return 0;
}

void clearcmd(struct cmd *cmd){
    memset(cmd, 0, sizeof(struct cmd));
}

void clear(void){
    clearcmd(&lcmd);
    clearcmd(&rcmd);
}


int peek(char **ps, char *es, char *toks){
    char *s;
    s = *ps;
    while(s < es && strchr(whitespace, *s)){
        s++;
    }
    *ps = s;
    return *s && strchr(toks, *s);
}

int gettoken(char **ps, char *es, char **q, char **eq)
{
    char *s;
    int ret;
    s = *ps;
    while(s < es && strchr(whitespace, *s)){
        s++;
    }
    if(q){
        *q = s;
    }
    ret = *s;
    switch(*s){
        case 0:
            break;
        case '|':
        case '<':
        case '>':
            s++;
            break;
        default:
            ret = 'a';
            while(s < es && !strchr(whitespace, *s) && !strchr(symbols, *s)){
                s++;
            }
            break;
    }  
    if(eq){
        *eq = s;
    }
    while(s < es && strchr(whitespace, *s)){
        s++;
    }
    *ps = s;
    return ret;
}

void parser(char **ps, char *es, struct cmd *cmd){
    int argc, tok, flag;
    char *q, *eq;

    flag = 0;
    argc = 0;
    while(!peek(ps, es, "|")){
        if((tok = gettoken(ps, es, &q, &eq)) == 0){
            break;
        }
        if(!flag){
            flag = 1;
            if(tok != 'a'){
                panic("syntax.");
            }
        }
        cmd->argv[argc] = q;
        cmd->eargv[argc] = eq;
        argc++;
        if(argc >= MAXARGS){
            panic("too many arguments.");
        }
        while(peek(ps, es, "<>")){
            tok = gettoken(ps, es, 0, 0);
            if(gettoken(ps, es, &q, &eq) != 'a'){
                panic("missing file for redirection.");
            }
            switch(tok){
                case '<':
                    cmd->infile = q;
                    cmd->einfile = eq;
                    break;
                case '>':
                    cmd->outfile = q;
                    cmd->eoutfile = eq;
                    break;
            }
        }
    }
    cmd->argv[argc] = 0;
    cmd->eargv[argc] = 0;
}

void parsecmd(char *s){
    char *es;
    es = s+ strlen(s);
    parser(&s, es, &lcmd);
    if(peek(&s, es, "|")){
        gettoken(&s, es, 0, 0);
        parser(&s, es, &rcmd);
    }
}

void redire(struct cmd *cmd){
    if(cmd->einfile){
        *cmd->einfile = 0;
        close(0);
        if(open(cmd->infile, O_RDONLY) < 0){
            fprintf(2, "open %s failed\n", cmd->infile);
            exit(-1);
        }
    }
    if(cmd->eoutfile){
        *cmd->eoutfile = 0;
        close(1);
        if(open(cmd->outfile, O_WRONLY | O_CREATE) < 0){
            fprintf(2, "open %s failed\n", cmd->outfile);
            exit(-1);
        }
    }
}

void execcmd(void){
    int pd[2], i;
    if(rcmd.argv[0] != 0){
        if(pipe(pd) < 0){
            panic("pipe failed.");
        }
        if(fork() == 0){
            close(1);
            dup(pd[1]);
            close(pd[0]);
            close(pd[1]);
            if(lcmd.argv[0] == 0){
                exit(-1);
            }
            redire(&lcmd);
            for(i = 0; lcmd.argv[i]; i++){
                *lcmd.eargv[i] = 0;
            }
            exec(lcmd.argv[0], lcmd.argv);
            fprintf(2, "exec %s failed\n", lcmd.argv[0]);
        }

        if(fork() == 0){
            close(0);
            dup(pd[0]);
            close(pd[0]);
            close(pd[1]);
            if(rcmd.argv[0] == 0){
                exit(-1);
            }
            redire(&rcmd);
            for(i = 0; rcmd.argv[i]; i++){
                *rcmd.eargv[i] = 0;
            }
            exec(rcmd.argv[0], rcmd.argv);
            fprintf(2, "exec %s failed\n", rcmd.argv[0]);
        }

        close(pd[0]);
        close(pd[1]);
        wait(0);
        wait(0);
    }
    else{
        if(fork() == 0){
            if(lcmd.argv[0] == 0){
                exit(-1);
            }
            redire(&lcmd);
            for(i = 0; lcmd.argv[i]; i++){
                *lcmd.eargv[i] = 0;
            }
            exec(lcmd.argv[0], lcmd.argv);
            fprintf(2, "exec %s failed\n", lcmd.argv[0]);
        }
        wait(0);
    }
}

int main(void){
    char buf[128];
    int fd;
    while((fd = open("console", O_RDWR)) >= 0){
        if(fd >= 3){
            close(fd);
            break;
        }
    }
    //clear();
    while(getcmd(buf, sizeof(buf)) >= 0){
        if(fork() == 0){
            parsecmd(buf);
            execcmd();
        }
        wait(0);
        clear();
    }
    exit(0);
}
