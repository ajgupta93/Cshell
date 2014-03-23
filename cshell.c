#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<limits.h>
#include<unistd.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<pwd.h>
#include<termios.h>
#include<signal.h>

#define BASESIZE 100 //the most use size for arrays
#define HOSTSIZE 30 //max size of machine name
#define PQSIZE 32768 //max number of process permitted 

//global Variables and struct declarations

char *username,*cwd;
char home[BASESIZE]; //home directory string
typedef struct processqueue{
    int pid;
    char name[40];
    char pstatus[40];
    char memory[40];
    char path[40];
    int ifstopped;
}node;
node * pqueue[PQSIZE]; //queue of background processes
int qend; //queue end pointer
pid_t shellid; //process id of the shell
pid_t fore; //process id of foreground process if it exists
int shell_terminal;
int fdpipe[BASESIZE][2];
int pipecount,pipos[BASESIZE];

//Function Definations
void sig_handler(int signo); //signal handler
int parse(char * input,char **afterparse); //convert input to 2d array of arguments
node * setinfo(node * queue,int prid,int check); //set the structures of background processes
void print_prompt(); //print the command prompt
void execute_command(char **split); //execute a native shell command
node* insert_job_queue(node *queue,pid_t spid,int check); //insert a background process into the queue
void remove_job_queue(int return_val); //remove a bcakground process from queue
void refresh_backinfo(); //update the structures of background processes
node* insert_pip_job_queue(node *queue,pid_t spid,char *inp);
void funcjobs();
void funckjob(char **afterparse,int roW);
void funcoverkill();
void funcpinfo(char **afterparse,int row);
void funcfg(char **afterparse,int row,char *backprocess);
void testforothers(char **afterparse,int row,char *backprocess);

//Function Code

void testforothers(char **afterparse,int row,char *backprocess)
{
    if(strcmp(afterparse[0],"fg")==0)
    {
	funcfg(afterparse,row,backprocess);
    }
    else if(strcmp(afterparse[0],"pinfo")==0)
    {
	funcpinfo(afterparse,row);
    }
    else if(strcmp(afterparse[0],"quit")==0)
    {
	return;
    }
    else if(strcmp(afterparse[0],"jobs")==0)
    {
	funcjobs(); 
    }
    else if(strcmp(afterparse[0],"kjob")==0)
    {
	funckjob(afterparse,row);

    }
    else if(strcmp(afterparse[0],"overkill")==0)
    {
	funcoverkill();
    }
    return;
}
void funcfg(char **afterparse,int row,char *backprocess)
{
    if(row>1)
    {
	int jobid=atoi(afterparse[1]);
	if(pqueue[jobid]!=NULL)
	{
	    int state,valofreturn,spid;
	    char name[30];
	    strcpy(name,pqueue[jobid]->name);
	    spid=pqueue[jobid]->pid;
	    remove_job_queue(pqueue[jobid]->pid);
	    fore=spid;
	    if(pqueue[jobid]->ifstopped==1)
	    {
		kill(spid,SIGCONT); //continuing process if brining into foreground not in speceification but implemented
	    }
	    tcsetpgrp (shell_terminal, spid);
	    valofreturn=waitpid(spid,&state,WUNTRACED); // wait till signal for suspend is receieved
	    tcsetpgrp (shell_terminal, shellid);
	    if(WIFSTOPPED(state)) //push into background if suspend is received
	    {
		pqueue[qend]=insert_job_queue(pqueue[qend],spid,1);
	    }
	    else
	    {
		sprintf(backprocess,"%s with pid %d exited with status %d\n",name,spid, WEXITSTATUS(state));
		write(2,backprocess,strlen(backprocess));
	    }
	}
	else
	{
	    write(2,"No such Job\n",12);
	}
    }
    else
    {
	write(2,"Usage fg <jobid>\n",17);
    }
    return;
}
void funcoverkill()
{
    int k;
    for(k=1;k<qend;k++)
    {
	//	printf("%s\n",pqueue[k]->name);
	if(pqueue[k]->ifstopped!=-2)
	    kill(pqueue[k]->pid,9);
    }
    return;
}
void funcpinfo(char **afterparse,int row)
{
    refresh_backinfo();
    int j,mark;
    mark=-1;
    pid_t id;
    if(row==1)
	id=getpid();
    else
	id=atoi(afterparse[1]);
    for(j=0;j<qend;j++)
	if(pqueue[j]->pid==id)
	{
	    mark=j;
	    break;
	}
    if(mark>=0)
    {
	char temp1[100];
	sprintf(temp1,"pid -- %d",pqueue[mark]->pid);
	write(1,temp1,strlen(temp1));
	write(1,"\n",1);
	write(1,"Name -- ",8);
	write(1,pqueue[mark]->name,strlen(pqueue[mark]->name));
	write(1,"\n",1);
	write(1,"State -- ",9);
	write(1,pqueue[mark]->pstatus,strlen(pqueue[mark]->pstatus));
	write(1,"\n",1);
	write(1,"Memory -- ",10);
	write(1,pqueue[mark]->memory,strlen(pqueue[mark]->memory));
	write(1,"\n",1);
	write(1,"Executable Path -- ",19);
	write(1,pqueue[mark]->path,strlen(pqueue[mark]->path));
	write(1,"\n",1);
    }
    else
	write(2,"No such  process\n",17);
    return;
}
void funckjob(char **afterparse,int row)
{
    int signalno,jobid;
    if(row<3)
    {
	write(2,"usage <kjob jobno signal>\n",26);
	return;
    }
    jobid=atoi(afterparse[1]);
    signalno=atoi(afterparse[2]);
    if(jobid<qend)
	kill(pqueue[jobid]->pid,signalno);
    else
	write(2,"No such job exists\n",19);
    return;
}
void funcjobs()
{
    int j;
    char try[100];
    char jobno[100];
    refresh_backinfo();
    for(j=1;j<qend;j++)
    {
	sprintf(jobno,"[%d]",j);
	write(1,jobno,strlen(jobno));
	write(1," ",1);
	write(1,pqueue[j]->name,strlen(pqueue[j]->name));
	write(1," ",1);
	sprintf(try,"[%d]",pqueue[j]->pid);
	write(1,try,strlen(try));
	write(1,"\n",1);
    }
    return;
}
void refresh_backinfo()
{
    int i;
    for(i=0;i<qend;i++)
    {
	if(pqueue[i]->ifstopped!=-2)
	    pqueue[i]=setinfo(pqueue[i],pqueue[i]->pid,pqueue[i]->ifstopped);
    }
    return;
}

node* insert_job_queue(node *queue,pid_t spid,int check)
{
    queue=(node *)malloc(sizeof(node));
    queue=setinfo(queue,spid,check);
    qend++;
    return queue;
}

node* insert_pip_job_queue(node *queue,pid_t spid,char *inp)
{
    queue=(node *)malloc(sizeof(node));
    strcpy(queue->name,inp);
    queue->pid=spid;
    queue->name[0]='\0';
    queue->pstatus[0]='\0';
    queue->memory[0]='\0';
    queue->path[0]='\0';
    queue->ifstopped=-2;
    qend++;
    return queue;
}
void sig_handler(int signo)
{
    pid_t thisprocess;
    int statet;
    thisprocess=getpid();
    if (signo == SIGINT)
    {
	//write(1,"received SIGINT\n",16);
	write(1,"Use quit to exit shell\n",23);
	print_prompt();
    }
    else if (signo == SIGKILL)
    {
	//write(1,"received SIGKILL\n",17);
	write(1,"Use quit to exit shell\n",23);
	print_prompt();
    }
    else if (signo == SIGCHLD)
    {
	//write(1,"received SIGCHLD\n",17);
	;
    }
    else if (signo == SIGTSTP)
    {
	//write(1,"received SIGTSTP\n",17);
	if(fore!=-1) // fore is the process id of the foreground process if no foreground process then it is -1
	    kill(fore,SIGTSTP); // suspend signal sent to foreground process
	else
	    write(1,"Use quit to exit shell\n",23);
    }
    return;
}

int parse(char * input,char **afterparse)
{

    int i=0;
    i=0;
    int row=0;
    while(input[i]!='\0')
    {
	while(input[i]==' '||input[i]=='\t'||input[i]=='\n')
	{
	    input[i++]='\0';
	}
	afterparse[row] = &(input[i]);
	row++;
	while(input[i]!=' '&& input[i]!='\t'&& input[i]!='\n'&& input[i]!='\0')
	    i++;
    }
    afterparse[row]='\0';
    return row;
}

node * setinfo(node * queue,int prid,int check)
{
    int fd,count=0,nread;
    char pathtoproc[BASESIZE];
    char pidtostr[BASESIZE];
    char buffer;
    queue->pid=prid;
    strcpy(pathtoproc,"/proc/");
    sprintf(pidtostr,"%d",prid);
    strcat(pathtoproc,pidtostr);
    char base[100];
    strcpy(base,pathtoproc);
    strcat(pathtoproc,"/stat");
    char command[100];
    fd=open(pathtoproc,4444);
    int fd1;
    strcpy(pathtoproc,base);
    strcat(pathtoproc,"/status");
    fd1=open(pathtoproc,4444);
    strcpy(pathtoproc,base);
    strcat(pathtoproc,"/exe");
    queue->ifstopped=check;
    queue->name[0]='\0';
    queue->pstatus[0]='\0';
    queue->memory[0]='\0';
    queue->path[0]='\0';
    if(fd==-1)
    {
	write(2,"could not open status file\n",27);
    }
    else
    {
	char buffer;
	int i;
	count=0;
	int n1=0,n2=0;
	while((nread=read(fd,&buffer,1))>0)
	{
	    if(buffer==' ' ||  buffer=='\t')
	    {
		count++;
	    }
	    else if(count==2)
	    {
		queue->pstatus[n2++]=buffer;
	    }
	}
	int nobytes;
	nobytes=readlink(pathtoproc,queue->path,40);
	queue->path[nobytes]='\0';
	if(strncmp(queue->path,home,strlen(home))==0)
	{
	    char newpath[40];
	    newpath[0]='\0';
	    strcat(newpath,"~");
	    int lenhome=strlen(home);
	    int lenstring=strlen(queue->path);
	    int ar;
	    for(ar=0;ar<=lenstring-lenhome;ar++)
	    {
		queue->path[ar]=queue->path[ar+lenhome];
	    }
	    strcat(newpath,queue->path);
	    strcpy(queue->path,newpath);
	}
	queue->pstatus[n2]='\0';
	close(fd);
	count=0;
	n1=0;
	n2=0;
	while((nread=read(fd1,&buffer,1))>0)
	{
	    if(buffer=='\n')
	    {
		count++;
	    }
	    else if(count==0)
	    {
		queue->name[n1++]=buffer;
	    }
	    else if(count==10)
	    {
		queue->memory[n2++]=buffer;
	    }
	}
	queue->name[n1]='\0';
	queue->memory[n2]='\0';
	int len4;
	len4=strlen(queue->name);
	for(i=0;i<=len4;i++)
	{
	    if(i<len4 && queue->name[i]=='\t')
	    {
		while(i<len4)
		{
		    queue->name[i]=queue->name[i+1];
		    i++;
		}
	    }
	}
	len4=strlen(queue->name);
	for(i=5;i<=len4;i++)
	{
	    queue->name[i-5]=queue->name[i];
	}

	len4=strlen(queue->memory);
	for(i=7;i<=len4;i++)
	{
	    queue->memory[i-7]=queue->memory[i];
	}
	len4=strlen(queue->memory);
	int k=-1;
	for(i=0;i<=len4;i++)
	{
	    if(i<len4 && (queue->memory[i]!='\t'&& queue->memory[i]!=' '))
	    {
		k=i;
		break;
	    }
	}
	len4=strlen(queue->memory);
	if(k!=-1)
	    for(i=k;i<=len4;i++)
	    {
		queue->memory[i-k]=queue->memory[i];
	    }

	close(fd1);
    }
    return queue;
}

void print_prompt()
{
    char hostname[HOSTSIZE];
    gethostname(hostname,HOSTSIZE);//getting hostname or machine name
    register struct passwd *pawd;
    register uid_t uid;
    uid=geteuid();//getting user id
    pawd=getpwuid(uid);//storing user info in structpawd
    username=pawd->pw_name;
    char tilda[BASESIZE];
    getcwd(tilda,BASESIZE);
    chdir(tilda);
    char cwd[BASESIZE];
    char display[BASESIZE];
    if(strstr(tilda,home)==NULL)
	strcpy(cwd,tilda);
    else
    {
	int i;
	int len=strlen(tilda);
	int homelen=strlen(home);
	cwd[0]='~';
	i=0;
	int k=1;
	if(len!=homelen)
	{
	    for(i=homelen;i<len;i++)
		cwd[k++]=tilda[i];
	}
	cwd[k]='\0';
    }
    sprintf(display,"<%s@%s:%s>",username,hostname,cwd);
    write(1,display,strlen(display));
    return;
}

void execute_command(char **split)
{
    if(execvp(*split,split)==-1)
	write(2,"No such command exists\n",23);
    return;
}

void remove_job_queue(int return_val)
{
    int j,mark;
    node *temp;
    for(j=0;j<PQSIZE && pqueue[j]!=NULL;j++)
    {
	if(pqueue[j]->pid==return_val)
	{ 
	    mark=j;
	    temp=pqueue[j];
	    break;
	}
    }
    for(j=mark;j+1<PQSIZE && j<qend-1;j++)
    {
	pqueue[j]=pqueue[j+1];
    }
    pqueue[j+1]=NULL;
    qend--;
    free(temp);
    return;
}

int main()
{
    shell_terminal=STDIN_FILENO; //new
    shellid=getpid();
    while (tcgetpgrp (shell_terminal) != (shellid = getpgrp ()))
	kill (- shellid, SIGTTIN);
    shellid=getpid();
    setpgid(shellid,shellid);
    tcsetpgrp (shell_terminal, shellid); //new
    fore=-1;
    int i,inputlen=0,pflag=0,pbgflag=0;
    int lid;
    char  inp[BASESIZE];
    char pathtopipe[100];
    int row;
    int outredirectflag=0,inredirectflag=0,infile,outfile,oldin,newin,oldout,newout;
    getcwd(home,BASESIZE);
    char backprocess[BASESIZE]; //buffer to print exited process info
    pqueue[qend]=(node *)malloc(sizeof(node));
    pqueue[qend]=setinfo(pqueue[qend],shellid,0);
    qend++;
    lid=0;
    while(1)
    {

	signal (SIGTTOU, SIG_IGN);
	signal (SIGTTIN, SIG_IGN);
	if (signal(SIGINT, sig_handler) == SIG_ERR)
	{
	    write(2,"\ncan't catch SIGINT\n",20);
	    continue;
	}
	if (signal(SIGQUIT, sig_handler) == SIG_ERR)
	{
	    write(2,"\ncan't catch SIGKILL\n",21);
	    continue;
	}
	if (signal(SIGCHLD, sig_handler) == SIG_ERR)
	{
	    write(2,"\ncan't catch SIGCHLD\n",21);
	    continue;
	}
	if (signal(SIGTSTP, sig_handler) == SIG_ERR)
	{
	    write(2,"\ncan't catch SIGTSTP\n",21);
	    continue;
	}

	if(inredirectflag==1)
	{
	    dup2(oldin,0);
	    close(infile);
	}
	if(outredirectflag==1)
	{
	    dup2(oldout,1);
	    close(outfile);	
	}
	refresh_backinfo();
	row=0;
	backprocess[0]='\0';
	int status,return_val;
	pathtopipe[0]='\0';
	if(lid>0 && pflag==1 && pbgflag==1 && access(pathtopipe,F_OK)!=0)
	{
	    sprintf(backprocess,"pid %d exited with exit status %d\n",lid,WEXITSTATUS(status));
	    write(2,backprocess,strlen(backprocess));
	    remove_job_queue(lid);
	    backprocess[0]='\0';
	    lid=0;
	}
	return_val=waitpid(-1,&status,WNOHANG); //to check if background processes exited or not
	while(return_val>0)
	{
	    if(pflag==1 && pbgflag==1)
	    {
		sprintf(backprocess,"aspid %d exited with exit status %d\n",return_val,WEXITSTATUS(status));
		write(2,backprocess,strlen(backprocess));
		remove_job_queue(return_val);
		backprocess[0]='\0';
	    }
	    else if(pflag==0)
	    {
		sprintf(backprocess,"pid %d exited with exit status %d\n",return_val,WEXITSTATUS(status));
		write(2,backprocess,strlen(backprocess));
		remove_job_queue(return_val);
		backprocess[0]='\0';
	    }
	    return_val=waitpid(-1,&status,WNOHANG); //to check if background processes exited or not
	}
	lid=0;
	pflag=0;
	pbgflag=0;
	print_prompt();
	i=0;
	inp[i]='\0';
	while(i==0 || inp[i-1]!='\n')
	{
	    read(0,&inp[i],1);
	    i++;
	}
	inp[i-1]='\0';
	inputlen=strlen(inp);
	char *afterparse[BASESIZE];
	row=parse(inp,afterparse);

	for(i=0;i<row;i++)
	{
	    if(strcmp(afterparse[i],"<")==0)
	    {
		if(access(afterparse[i+1],F_OK)==0)
		{
		    infile=open(afterparse[i+1],O_RDONLY);
		    oldin=dup(0);
		    dup2(infile,0);
		    inredirectflag=1;
		    int m,k;
		    k=i;
		    for(m=i+2;m<=row;m++)
		    {
			afterparse[k++]=afterparse[m];
		    }
		    row=row-2;
		}
		else
		    write(2,"No such input file\n",19);
		break;
	    }
	}
	for(i=0;i<row;i++)
	{
	    if(strcmp(afterparse[i],">")==0 || strcmp(afterparse[i],">>")==0)
	    {
		if(strcmp(afterparse[i],">")==0)
		{
		    outfile=open(afterparse[i+1],O_CREAT | O_WRONLY,0777);
		    oldout=dup(1);
		    dup2(outfile,1);
		    outredirectflag=1;
		    int m,k;
		    k=i;
		    for(m=i+2;m<=row;m++)
		    {
			afterparse[k++]=afterparse[m];
		    }
		    row=row-2;
		}
		else if(strcmp(afterparse[i],">>")==0)
		    if(access(afterparse[i+1],F_OK)==0)
		    {	
			outfile=open(afterparse[i+1],O_APPEND | O_WRONLY,0777);
			oldout=dup(1);
			dup2(outfile,1);
			outredirectflag=1;
			int m,k;
			k=i;
			for(m=i+2;m<=row;m++)
			{
			    afterparse[k++]=afterparse[m];
			}
			row=row-2;
		    }

		    else
		    {
			write(2,"No such file exists\n",20);
			inputlen=0;
		    }
		break;
	    }

	}
	pipecount=0;
	pipos[pipecount++]=-1;
	for(i=0;i<row;i++)
	{   
	    if(strcmp(afterparse[i],"|")==0)
	    {   
		afterparse[i]='\0';
		pipos[pipecount++]=i;
	    }   
	    pipe(fdpipe[i]);
	}  
	pipecount--;
	int acin=dup(0);
	int acout=dup(1);
	if(inputlen==0)
	    continue;
	else if(pipecount!=0)
	{
	    pflag=1;
	    int flag=1;
	    int arr[pipecount+1];
	    if(row>1 && strlen(afterparse[row-1])>0 && strcmp(afterparse[row-1],"&")==0) 
	    {
		flag=-1;
		afterparse[row-1]='\0'; //becaus & has no symentics it is not an argument e.g. "gedit a &" shouldnt open an '&' file
		row--; //since we decreased one argument
	    }
	    if(flag==-1)
		pbgflag=1;
	    lid=fork();
	    if (lid==0)
	    {
		_exit(0);
	    }
	    else
	    {
		;
	    }
	    {
		int level,ppos=0;
		lid=fork();
		if(lid==0)
		{ 
		    //ls
		    int myid=getpid();
		    arr[0]=myid;
		    setpgid(myid,lid);
		    dup2(fdpipe[0][1],1);
		    for(i=0;i<=pipecount;i++) //0
		    {
			close(fdpipe[i][0]);
			close(fdpipe[i][1]);
		    }
		    char *temp1=afterparse[pipos[0]+1],**temp2=&afterparse[pipos[0]+1];
		    if(execvp(temp1,temp2)==-1)
		    {
			testforothers(temp2,row,backprocess);
		    }
		}
		else
		{
		    if(pbgflag==1)
			pqueue[qend]=insert_job_queue(pqueue[qend],lid,0);
		    level=1;
		    pid_t mid;
		    for(i=1;i<=pipecount-1;i++)
		    {
			mid=fork();
			if(mid==0)
			{
			    int myid=getpid();
			    setpgid(myid,lid);
			    arr[i]=myid;
			    dup2(fdpipe[level-1][0],0);
			    dup2(fdpipe[level][1],1);
			    level=level+1;
			    int j;
			    for(j=0;j<=pipecount;j++)  //i
			    {
				close(fdpipe[j][0]);
				close(fdpipe[j][1]);
			    }
			    char *temp1=afterparse[pipos[i]+1],**temp2=&afterparse[pipos[i]+1];
			    if(execvp(temp1,temp2)==-1)
			    {
				testforothers(temp2,row,backprocess);
			    }
			}
			else 
			{
			    if(pbgflag==1)
				pqueue[qend]=insert_job_queue(pqueue[qend],mid,0);
			    level++;
			}
		    }
		    pid_t end;
		    end=fork();
		    if(end==0)
		    {
			int myid=getpid();
			setpgid(myid,lid);
			arr[pipecount]=myid;
			dup2(fdpipe[level-1][0],0);
			int j;
			for(j=0;j<=pipecount;j++)
			{
			    close(fdpipe[j][0]);
			    close(fdpipe[j][1]);
			}
			char *temp1=afterparse[pipos[pipecount]+1],**temp2=&afterparse[pipos[pipecount]+1];
			if(execvp(temp1,temp2)==-1)
			{
			    testforothers(temp2,row,backprocess);
			}
		    }
		    else
		    {
			if(pbgflag==1)
			    pqueue[qend]=insert_job_queue(pqueue[qend],end,0);
			;
		    }
		}
		int j;
		for(j=0;j<=pipecount;j++)
		{
		    close(fdpipe[j][0]);
		    close(fdpipe[j][1]);
		}
		int status;
		if(flag==-1)
		{
		    fore=-1;
		    int value_ret;
		    value_ret=waitpid(-1,&status,WNOHANG); //to check if background processes exited or not
		    pqueue[qend]=insert_pip_job_queue(pqueue[qend],lid,inp);
		}
		else if(flag==1)
		{
		    fore=lid;
		    tcsetpgrp(shell_terminal,lid);
		    for(j=0;j<=pipecount;j++)
		    {
			int status;
			wait(arr[j],&status,0);
		    }
		    tcsetpgrp(shell_terminal,shellid);
		}
	    }
	}
	else if(strcmp(afterparse[0],"fg")==0)
	{
	    funcfg(afterparse,row,backprocess);
	    continue;

	}
	else if(strcmp(afterparse[0],"pinfo")==0)
	{
	    funcpinfo(afterparse,row);
	    continue;
	}
	else if(strcmp(afterparse[0],"quit")==0)
	{
	    return 0;
	}
	else if(strcmp(afterparse[0],"jobs")==0)
	{
	    funcjobs(); 
	    continue;
	}
	else if(strcmp(afterparse[0],"kjob")==0)
	{
	    funckjob(afterparse,row);
	    continue;

	}
	else if(strcmp(afterparse[0],"overkill")==0)
	{
	    funcoverkill();
	    continue;
	}
	else if(strcmp(afterparse[0],"cd")==0)
	{
	    int j,flag=0,k=0;
	    int checkret=0;
	    char newdir[BASESIZE];
	    if(row==1)
		checkret=chdir(home);
	    if(row>1)
	    {
		strcpy(newdir,afterparse[1]);
		if(strlen(newdir)==0)
		    checkret=chdir(home);
		else
		{
		    if(newdir[0]=='~')
		    {
			char tildapath[BASESIZE];
			strcpy(tildapath,home);
			int len=strlen(tildapath);
			int len2=strlen(newdir);
			int n,m;
			m=len;
			for(n=1;n<=len2;n++)
			{
			    tildapath[m++]=newdir[n];
			}
			checkret=chdir(tildapath);
		    }
		    else
			checkret=chdir(newdir);
		}
	    }
	    if(checkret==-1)
		write(2,"No such directory exists\n",25);
	    continue;
	}
	else
	{
	    pid_t pid;
	    int flag=0;
	    if(row>1 && strlen(afterparse[row-1])>0 && strcmp(afterparse[row-1],"&")==0) 
	    {
		flag=-1;
		afterparse[row-1]='\0'; //becaus & has no symentics it is not an argument e.g. "gedit a &" shouldnt open an '&' file
		row--; //since we decreased one argument
	    }
	    else
		flag=1;
	    pid=fork();
	    if(pid>0)
	    {
		if(flag==-1) //background process
		{
		    int value_ret;
		    value_ret=waitpid(-1,&status,WNOHANG); //to check if background processes exited or not
		    pqueue[qend]=insert_job_queue(pqueue[qend],pid,0);
		    fore=-1;
		}

		else if(flag==1) //foreground process
		{
		    fore=pid;
		    int status,return_val;
		    tcsetpgrp (shell_terminal, pid);
		    return_val=waitpid(pid,&status,WUNTRACED); // wait till signal for suspend is receieved
		    tcsetpgrp (shell_terminal, shellid);
		    if(WIFSTOPPED(status)) //push into background if suspend is received
		    {
			pqueue[qend]=insert_job_queue(pqueue[qend],pid,1);
		    }
		    else
		    {
			sprintf(backprocess,"%s with pid %d exited with status %d\n",afterparse[0],pid, WEXITSTATUS(status));
			write(2,backprocess,strlen(backprocess));
		    }

		}
		continue;
	    }
	    if(pid==0)
	    {
		pid_t cpid,pgid;
		cpid=getpid();
		pgid=cpid;
		setpgid(cpid,pgid);
		signal (SIGINT, SIG_DFL);
		signal (SIGQUIT, SIG_DFL);
		signal (SIGTSTP, SIG_DFL);
		signal (SIGTTIN, SIG_DFL);
		signal (SIGTTOU, SIG_DFL);
		signal (SIGCHLD, SIG_DFL);
		if(flag==1)
		{
		    tcsetpgrp(shell_terminal,pgid);
		}

		execute_command(afterparse);
		_exit(0);
	    }
	}

    }
    return 0;
}

