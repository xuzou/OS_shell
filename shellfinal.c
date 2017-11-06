#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>



#define MAX_SUB_COMMANDS 5
#define MAX_ARGS 10

struct SubCommand
{
	char *line;
	char *argv[MAX_ARGS];
};

struct Command
{
	struct SubCommand sub_commands[MAX_SUB_COMMANDS];
	int num_sub_commands;
	char *stdin_redirect;
	char *stdout_redirect;
	int background;
	int redirect;
};


void ReadArgs(char *in, char **argv, int size);
void ReadCommand(char *line, struct Command *command);
void ReadRedirectsAndBackground(struct Command *command);
void SingleCommand(struct Command *command);
void MultipleCommands(struct Command *command);
void SubcommandsRed(struct Command *command);
int BuiltinCommands(struct Command * command);

int main()
{
	char cwd[1024];
	struct Command command;
	char s[200];
	char *argv[MAX_ARGS];
	int ret;
	while(1)
	{   
		
		if(getcwd(cwd, sizeof(cwd))!=NULL)
		{
			printf("%s$ ", cwd);
		}
		else
		{
			perror("getcwd");
		}
		fgets(s,sizeof(s),stdin);
		if(strcmp(s, "\n") == 0)
		{
			if((ret = waitpid(-1, NULL, WNOHANG))>0)
			{
				printf("[%d] finished\n", ret);
				continue;
			}
			else
			{
				continue;
			}
		}
		ReadCommand(s, &command);
		ret = fork();
		int p;
		if(ret<0)
		{
			perror("fork");
			return 1;
		}
		else if(ret == 0)
		{
			p = getpid();
			//one subcommand
			if(command.num_sub_commands == 1)
			{
				ReadRedirectsAndBackground(&command);
				if(BuiltinCommands(&command) == 0)
				{
					if(command.background == 1)
					{
						printf("[%d]\n",p);
						close(1);
						command.background = 0;
					}
					SingleCommand(&command);
				}	
			}
			//multiple subcommands
			else
			{
				ReadRedirectsAndBackground(&command);
				if(command.redirect == 1 || command.background == 1)
				{
					if(command.background == 1)
					{
						printf("[%d]\n",p);
						close(1);
						fopen("/dev/null", "w");
					}  
					SubcommandsRed(&command);
				}          
				MultipleCommands(&command);
			}
		}
		else
		{
			waitpid(ret, NULL, 0);
		}
		
	}
}

//judge operation modes
void ReadRedirectsAndBackground(struct Command *command)
{
	int m;
	command->stdin_redirect = NULL;
	command->stdout_redirect = NULL;
	command->background = 0;
	int n = command->num_sub_commands - 1;
	int fileDescriptor;
		
	for(m=0;command->sub_commands[n].argv[m]!=NULL;m++)
	{
		//judge redirect in
		if((strcmp(command->sub_commands[n].argv[m],"<")==0))
		{
			if(command->sub_commands[n].argv[m+1]!=NULL && strcmp(command->sub_commands[n].argv[m+1],"<")!=0 && strcmp(command->sub_commands[n].argv[m+1],"&")!=0)
			{
				
				command->stdin_redirect = command->sub_commands[n].argv[m + 1];
				close(0);
				fileDescriptor = open(command->stdin_redirect, O_RDONLY);
				
				command->sub_commands[n].argv[m] = NULL;
				if(fileDescriptor == -1)
				{
					close(1);
					fopen("/dev/null", "r");
					fopen("/dev/null", "w");
					fprintf(stderr, "%s: File not found\n", command->stdin_redirect);
				}
				command->redirect = 1;
			}
			else
			{
				printf("No stdin redirect\n");
				command->redirect = 1;
			}
		}
		//judge redirect out
		else if(strcmp(command->sub_commands[n].argv[m],">")==0)
		{
			if(command->sub_commands[n].argv[m+1]!=NULL && strcmp(command->sub_commands[n].argv[m+1],">")!=0 && strcmp(command->sub_commands[n].argv[m+1],"&")!=0)
			{
				
				command->stdout_redirect = command->sub_commands[n].argv[m + 1];
				close(1);
				fileDescriptor = open(command->stdout_redirect, O_WRONLY | O_CREAT, 0660);
				command->sub_commands[n].argv[m] = NULL;
				command->sub_commands[n].argv[m+1] = NULL;			

				if(fileDescriptor == -1)
				{
					fopen("/dev/null", "w");
					fprintf(stderr, "%s: Cannot create file\n", command->stdout_redirect);
				}
				command->redirect = 1;
			}
			else
			{
				printf("No stdout redirect\n");
				command->redirect = 1;
			}
		}
		//judge background
		else if(strcmp(command->sub_commands[n].argv[m],"&")==0)
		{
			command->background = 1;
			command->sub_commands[n].argv[m] = NULL;
		}
	}
}

//split the command line into mutiple subcommands
void ReadCommand(char *line, struct Command *command)
{
	char *str = strdup(line);
	char *token;
	token = strtok (str, "|");
	int a;
	for (a = 0; a < MAX_SUB_COMMANDS; a++)
	{
		command->sub_commands[a].line = (char*)malloc(200);
	}
	int i = 0;
	while (token != NULL)
	{
		if(i == MAX_SUB_COMMANDS)
		{
			break;
		}
		else
		{
			command->sub_commands[i++].line = strdup(token);
			token = strtok (NULL, "|"); 
		}

	}
	command->num_sub_commands = i;
	int j = 0;
	for(j = 0; j < command->num_sub_commands; j++)
	{
		ReadArgs(command->sub_commands[j].line, command->sub_commands[j].argv, MAX_ARGS);
	}
}

//split each subcommand into multiple arguments
void ReadArgs(char *in, char **argv, int size)
{
	char *str = strdup(in);
	int argc = 0;
	int len;
	
	*argv = malloc(size*sizeof(*argv));
	argv[argc] = strtok(str, " ");
	while(argv[argc]!=NULL)
	{
		argv[argc] = strdup(argv[argc]);
		if(argc>=(size-2))
			break;
		len = strlen(argv[argc]);
		if(strlen(argv[argc])>0)
		{
			if(argv[argc][len-1]=='\n')
			{
				argv[argc][len-1]='\0';
			}
		}
		argv[++argc] = strtok(NULL, " ");
	}
	argv[argc + 1] = NULL;
	free(str);
}

//execute multiple subcommands with multiple pipes
void MultipleCommands(struct Command * command) 
{
    //num of pipe = num of subcommands - 1
	int numPipes = command->num_sub_commands - 1;
	int status;
	int i = 0;
	int pid;
    //num of filedescriptor = num of pipe * 2
	int pipefds[2*numPipes];
    //create pipe
	for(i = 0; i < (numPipes); i++)
	{
		if(pipe(pipefds + i*2) < 0) 
		{
			perror("couldn't pipe");
			exit(EXIT_FAILURE);
		}
	}

	//create child processes
	int j = 0;
	while(j < command->num_sub_commands)
	{
		pid = fork();
		if(pid == 0) 
		{
            //if not the last subcommand
			if(j != command->num_sub_commands-1)
			{
				if(dup2(pipefds[j * 2 + 1], 1) < 0)
				{
					perror("dup2");
					exit(EXIT_FAILURE);
				}
			}
            //if not the first subcommand
			if(j != 0 )
			{
				if(dup2(pipefds[(j - 1) * 2], 0) < 0)
				{
					perror("dup2");
					exit(EXIT_FAILURE);
				}
			}

           //close unused file decriptors
			for(i = 0; i < 2*numPipes; i++)
			{
				close(pipefds[i]);
			}

			if( execvp(command->sub_commands[j].argv[0],command->sub_commands[j].argv) < 0 )
			{
				exit(EXIT_FAILURE);
			}
		}
		else if(pid < 0)
		{
			perror("error");
			exit(EXIT_FAILURE);
		}

		j++;
	}
    //parent process: close file decriptors and wait
	for(i = 0; i < 2 * numPipes; i++)
	{
		close(pipefds[i]);
	}

	for(i = 0; i < numPipes + 1; i++)
		wait(&status);
}

void SubcommandsRed(struct Command *command)
{
	int fd1[2];
	int ret;
	pipe(fd1);
	ret = fork();
	if(ret < 0)
	{
		perror("fork");
	}
	else if(ret==0)
	{
		close(fd1[0]);
		dup2(fd1[1],1);
		execvp(command->sub_commands[0].argv[0], command->sub_commands[0].argv);
		exit(1);
	}
	else
	{
		close(fd1[1]);
		dup2(fd1[0],0);
		execvp(command->sub_commands[1].argv[0], command->sub_commands[1].argv);    
		exit(1);
	}
	waitpid(ret, NULL, 0);
}
//do things like "exit" and "cd", change the prompt title
int BuiltinCommands(struct Command * command)
{
	char cwd[1024];
	if (strcmp(command->sub_commands[0].argv[0], "exit")==0)
	{
		exit(0);
	}
	else if(strcmp(command->sub_commands[0].argv[0], "cd")==0)
	{
		if(command->sub_commands[0].argv[1] == NULL)
		{
			chdir(getenv("HOME"));
			return 1;
		}
		else
		{
			int err = chdir(command->sub_commands[0].argv[1]);
			if(err < 0)
			{
				printf("No such directory\n");
				return 1;
			}
			else
			{
				if(getcwd(cwd, sizeof(cwd))!=NULL)
				{
					return 1;
				}
				else
				{
					perror("getcwd()");
					return 1;
				}
				
			}
		}
	}
	return 0;
}
//execute single command
void SingleCommand(struct Command *command)
{
	int i;
	for(i=0;i<command->num_sub_commands;i++)
	{
		if(execvp(command->sub_commands[i].argv[0], command->sub_commands[i].argv) == -1)
		{
			fprintf(stderr, "%s: Command not found\n", command->sub_commands[i].argv[0]);
		}
	}
}
