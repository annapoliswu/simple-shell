#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <limits.h>
#include <unistd.h>
#include <stdlib.h>
#include <pwd.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include "sh.h"


int sh( int argc, char **argv, char **envp )
{  
  //int pid in header
  glob_t globbuf;
  extern char **environ;
  char buffer[MAXLINE];
  char promptbuffer[PROMPTMAX];
  
  char *prompt = calloc(PROMPTMAX, sizeof(char));
  char *commandline = calloc(MAX_CANON, sizeof(char));
  char *command, *arg, *commandpath, *p, *pwd, *owd;
  char **args = calloc(MAXARGS, sizeof(char*));
  int uid, i, status, argsct, go = 1;
  struct passwd *password_entry;
  char *homedir;
  struct pathelement *pathlist;

  uid = getuid();
  password_entry = getpwuid(uid);               /* get passwd info */
  homedir = password_entry->pw_dir;		/* Home directory to start
						   out with*/

  if ( (pwd = getcwd(NULL, PATH_MAX+1)) == NULL )
    {
      perror("getcwd");
      exit(2);
    }
  owd = calloc(PATH_MAX+1, sizeof(char)); //don't use strlen(pwd) cause pwd can change later with cd 
  memcpy(owd, pwd, strlen(pwd));
  prompt[0] = ' '; prompt[1] = '\0';

  /* Put PATH into a linked list */
  pathlist = get_path();

  while ( go )
    {
    
      /* print your prompt */    
      printf("\n%s [%s]> ", prompt, pwd);

      /* get command line and process */
      if(fgets(buffer, MAXLINE, stdin) != NULL){
	int linelength = strlen(buffer);
	if(buffer[linelength-1] == '\n'){
	  buffer[linelength-1] = 0;
	}
	strcpy(commandline, buffer);
      }

      //parse commandline and extract command and args, args[0] is command
      int i = 0;
      char* token = strtok(commandline, " "); //commandline string gets destroyed 
      command = token;
      memset(args, '\0', MAXARGS*sizeof(char*));
      while(token){
	args[i] = token;
	token = strtok(NULL, " ");
	i++;
      }

 
      if(command != NULL){
      
	/* check for each built in command and implement */
	if(strcmp(command,"exit") == 0){
	  printexecuting(command);
	  break;
	}
	
	//check if absolute path
	else if((command[0] == '/') |
		(command[0] == '.') & (command[1] == '/') |
		((command[1] == '.') & (command[2] == '/'))){	  
	  if ( access(command, X_OK) == -1){
	    printf("\nUnable to access %s", command);
	    perror("Error ");
	  }else{
	    printf("\nExecuted path %s\n", command);
	    executeCommand(command, args, status);
	  }
	}

	else if(strcmp(command, "which" ) == 0){
	  for( int i = 1; args[i] != NULL; i++){
	    commandpath = which(args[i], pathlist);
	    printf("\n%s",commandpath);
	    free(commandpath);
	  }
	}

	else if(strcmp(command, "where" ) == 0){
	  for( int i = 1; args[i] != NULL; i++){
	    commandpath = where(args[i], pathlist);
	    free(commandpath);
	  }
	}      
	
	//change directories
	else if(strcmp(command, "cd" ) == 0){
	  printexecuting(command);
	  if(args[1] == NULL){
	    strcpy(owd, pwd);
	    strcpy(pwd, homedir);
	    chdir(pwd);
	  }else if(strcmp(args[1], "-") == 0){
	    p = pwd;
	    pwd = owd;
	    owd = p;
	    chdir(pwd);
	  }else if(args[1] != NULL && args[2] == NULL){		
	    if(chdir(args[1]) == -1){
	      perror("Error ");
	    }else{
	      memset(owd, '\0', strlen(owd));
	      memcpy(owd, pwd, strlen(pwd));  
	      getcwd(pwd, PATH_MAX+1);
	    }	    
	  }

	  //built-in list
	}else if(strcmp(command, "list") == 0){
	  printexecuting(command);
	  if(args[1] == NULL){ //0 holds command
	    list(pwd);
	  }else{
	    int i = 1;
	    while(args[i]){
	      if(access(args[i], X_OK) == -1){
		perror("\nError ");
	      }else{
		printf("\n%s:\n", args[i]);
		list(args[i]);
	      }
	      i++;
	    }
	  }
	}

	//set prompt
	else if(strcmp(command, "prompt") == 0){
	  printexecuting(command);
	  if(args[1] == NULL){
	    printf("/nInput prompt prefix: ");
	    if(fgets(promptbuffer, PROMPTMAX, stdin) != NULL){
	      int linelength = strlen(promptbuffer);
	      if(promptbuffer[linelength-1] == '\n'){
		promptbuffer[linelength-1] = 0; 
	      }
	      strtok(promptbuffer, " ");
	      strcpy(prompt, promptbuffer);
	    }
	  }else{
	    // prompt = args[1]; cannot do this!! undefined, addres changes, use strcpy
	    strcpy(prompt, args[1]); 
	  }	
	}

	//prints environment
	else if(strcmp(command, "printenv") == 0){
	  printexecuting(command);
	  if(args[1] == NULL){
	    printenv(environ);
	  }else if(args[2] == NULL){
	    printf("\n%s\n", getenv(args[1]));
	  }else{
	    printf("\nprintenv: Too many arguments.");
	  } 
	}

	//sets environment
	else if(strcmp(command, "setenv") == 0){
	  printexecuting(command);
	  if(args[1] == NULL){
	    printenv(environ);
	  }else if(args[2] == NULL && (strcmp(args[1], "PATH") == 0 || strcmp(args[1], "HOME") == 0)){
	    printf("\nPlease do not set PATH or HOME to empty\n");
	  }else if(args[2] == NULL){
	    if(setenv(args[1] , "", 1) == -1){
	      perror("Error ");
	    }
	  }else if(args[3] == NULL){
	    if(setenv(args[1], args[2],1) == -1){
	      perror("Error ");
	    }else{
	      if(strcmp(args[1], "PATH")==0){
		deletepath(&pathlist);
		pathlist = NULL; 
	      }
	      if(strcmp(args[1], "HOME") == 0){
		homedir = args[2];
	      }
	    }
	  }else{
	    printf("\nsetenv: Too many arguments.");
	  }
	}

	else if( strcmp(command, "pid") == 0){
	  printexecuting(command);
	  printf("\nPID: %d", getpid());
	}
	else if(strcmp(command, "pwd") == 0){
	  printexecuting(command);
	  printf("\nPWD: %s", pwd);
	}

	//kills or sends sig to kill
	else if( strcmp(command, "kill") == 0){
	  if(args[1] == NULL){
	    printf("\nNo argument input for kill");
	  }else if(args[2] == NULL){
	    int temppid = -1;
	    sscanf(args[1], "%d", &temppid);
	    if(temppid != -1){
	      if(kill(temppid, 15) == -1){
		perror("Error ");
	      }
	    }else{
	      printf("\nEntered invalid PID: Not a number!");
	    }
	  }else if(args[3] == NULL){
	    int temppid = -1;
	    int sig = 0;
	    sscanf(args[2], "%d", &temppid);
	    sscanf(args[1], "%d", &sig);
	    if(temppid != -1 && sig < 0){
	      if(temppid == getpid() && sig == -1){
		  free(owd);  
		  free(pwd);
		  free(prompt);  
		  free(args);
		  free(commandline);
		  deletepath(&pathlist);
		  pathlist = NULL;
	      }
	      if( kill(temppid, abs(sig)) == -1){
		perror("Error ");
	      }
	    }else{
	      printf("\nInvalid arguments for kill");
	    }
	  }
	}

	//find command and execute, handles * and ? wildcards for ls
	else{
	  printf("Executed %s", command);
	  int sCard = getWildcardIndex('*', args);
	  int qCard = getWildcardIndex('?', args);
	  if(strcmp(command, "ls") == 0 && sCard != -1){	    
	    executeGlob(sCard, commandpath, pathlist, args, globbuf, status);	  
	  }else if(strcmp(command, "ls") == 0 && qCard != -1){
	    executeGlob(qCard, commandpath, pathlist, args, globbuf, status);
	  }else{
	    commandpath = which(command, pathlist);
	    executeCommand(commandpath, args, status);
	    free(commandpath);
	  }   

	}
	
      }/*end huge conditional*/


    }/*while loop*/
    
  
  free(owd);  
  free(pwd);
  free(prompt);  
  free(args);
  free(commandline);
  deletepath(&pathlist);
  pathlist = NULL;
  
  exit(0);
  
  return 0;
  
} /* sh() */


/* loop through pathlist until finding command and returns it.  Return NULL when not found. */
char *which(char *command, struct pathelement *pathlist )
{
  char buffer[MAXLINE];
  
  while(pathlist){
    snprintf(buffer, MAXLINE, "%s/%s", pathlist->element, command);
    
    if(access(buffer, X_OK) == -1){
      pathlist = pathlist->next;
    }else{
      int linelength = strlen(buffer);
      char* ret = calloc(linelength+1, sizeof(char));    
      strncpy(ret, buffer, linelength);
      return ret;
    }
    
  }
  return NULL;
    
} /* which() */


/*finds and prints all matching commandpaths, return a commandpath*/
char *where(char *command, struct pathelement *pathlist )
{
  char buffer[MAXLINE];
  int found = 0;
  char* ret;
  while(pathlist){
   snprintf(buffer, MAXLINE, "%s/%s", pathlist->element, command);
    
    if(access(buffer, X_OK) == -1){
      pathlist = pathlist->next;
    }else if (access(buffer, X_OK) != -1 && found == 0){
      found = 1;
      int linelength = strlen(buffer);
      ret = calloc(linelength+1, sizeof(char));    
      strncpy(ret, buffer, linelength);
      printf("\n%s", ret);
      pathlist = pathlist->next;
    }else if (access(buffer, X_OK) != -1){
      printf("\n%s", buffer);
      pathlist = pathlist->next;
    }
  }
    return ret;
    
} /* where() */


/*reads in directoy and prints out files one-by-one on separate lines*/
void list ( char* dir)
{
  DIR* adir = opendir(dir);
  struct dirent* afile;
  if(adir){
    while((afile = readdir(adir)) != NULL){
      printf("%s\n", afile->d_name);
    }
  }
  closedir(adir);
  
} /* list() */


/*forks and execs to create process from commandpath */
void executeCommand(char *commandpath, char** args, int status){
  if(commandpath == NULL){
      fprintf(stderr, "%s: Command not found.\n", args[0]); 
      }else{
	pid = fork();
	if(pid == 0){ //in child
	  execve(commandpath,args,NULL);
	  exit(EXIT_FAILURE); //exec fails to return
	}else if(pid < 0){

	}else{ //parent
	  do{
	    waitpid(pid, &status, WUNTRACED);
	  }while(!WIFEXITED(status) && !WIFSIGNALED(status));
	}
	
      }
}

/*prints all environmental vars*/
void printenv(char ** envp){
    int i = 0;
    while(envp[i]!=NULL){
      printf("%s\n",envp[i]);
      i++;
    }
} 
  
void printexecuting(char * command){
  printf("\nExecuting built-in %s", command);
}

//returns -1 if no wildcard arg found in args, else returns index of the wildcard arg
int getWildcardIndex( char wildcard, char **args){
  int i = 0;
  char *p;
  while(args[i]){
    p = strchr(args[i], wildcard);
    if(p != NULL){
      return i;
    }
    i++;
  }
  return -1;
}

/*takes in the wildcard index in args and performs glob expansion, cleans self*/
void executeGlob(int cardIndex, char *commandpath, struct pathelement *pathlist, char **args, glob_t globbuf, int status){
      globbuf.gl_offs = cardIndex;
      glob(args[cardIndex], GLOB_DOOFFS, NULL, &globbuf);
      for (int i = 0; i < cardIndex; i++){
	globbuf.gl_pathv[i] = calloc(sizeof(char), strlen(args[i])+1 );
	strcpy( globbuf.gl_pathv[i], args[i]);	
      }
      commandpath = which(globbuf.gl_pathv[0], pathlist);
      executeCommand(commandpath, globbuf.gl_pathv, status);
      free(commandpath);
      for (int i = 0; i < cardIndex; i++){
	free(globbuf.gl_pathv[i]);
      }
      globfree(&globbuf);
}
