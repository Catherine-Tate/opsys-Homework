#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>


void childTerm() {
	int status;	
	pid_t pid = wait(&status);
	printf("[Process %d terminated ", pid);
	
	if(WIFEXITED(status)) {
		int rc = WIFEXITED(status); 
		printf("with exit status %d]\n", rc);  
	}
	else if(WIFSIGNALED(status)) {
		printf("abnormally]\n");
	}
	
	signal(SIGCHLD, NULL);
}

int cd(char * dir) {
	char * path;
	if(strcmp(dir, "cd") == 0 || strcmp(dir, "cd ") == 0){
		path = calloc(1, 1024);
		strcpy(path, getenv("HOME"));
	}
	else path = dir+3;
	int rc = chdir(path);
	if(rc != 0) fprintf(stderr, "chdir() failed: Not a directory\n");
	else {
		char cDir[FILENAME_MAX]; //buffer var to store current directory
		getcwd(cDir, FILENAME_MAX);
	}	
	return(rc);
}


/*
rc error codes: 

0: Okay
1: Error
2: Not Found
*/
int xCom(char ** mainComs, char ** paths, char ** inputs, int bg, int pipeVar) { //eXecute COMand

	if(bg == 1) {
		signal(SIGCHLD, childTerm);
	}

	#ifdef DEBUG_BG
	printf("BG set to: %d\n", bg);
	#endif

	#ifdef DEBUG_EXECUTE	
	printf("inputs: %s\npaths:%s\n", inputs[0], paths[0]);
	printf("inputs: %s\npaths:%s\n", inputs[1], paths[1]);
	#endif
	
	int p[2];
	int pc = pipe(p);
	if(pc == -1) {
		fprintf(stderr, "ERROR MAKING PIPING\n");
		return(1);
	}
	#ifdef DEBUG_PIPE
	else {
		printf("SUCCESS MAKING PIPING!\n");
	}
	#endif


	#ifdef DEBUG_EXECUTE	
	printf("p[0]: %d\np[1]: %d\n", p[0], p[1]);		
	#endif

	int i, rc;
	for(i = 0; i <= pipeVar; i++) {
	
		if(paths[i] == NULL) {
			fprintf(stderr, "ERROR: command \"%s\" not found\n", mainComs[i]);
			rc = 2;
			continue;
		}

		if(strcmp(paths[i], "cd") == 0){
			rc = cd(inputs[i]);
			break;
		}
		
		int spaces, j;
		//count number of spaces in each command to get the number of arguments
		if(inputs[i] == NULL) continue;
		for( spaces = 0, j = 1; j < strlen(inputs[i]); j++ ) { 
			if(inputs[i][j] == ' ' ) spaces++;
		}

		#ifdef DEBUG_EXECUTE 
		//printf("Args: %d\n", spaces);
		#endif

		char * args = strtok(inputs[i], " ");	
		char ** argArr = calloc(spaces+2, sizeof(char *));
		int k = 0;
		//split the string into an array of arguments by the space char
		while(args != NULL) {
			argArr[k] = calloc(64, sizeof(args)+1);
			strcpy(argArr[k], args);
			//printf("argArr[%d]: \"%s\"\n", k, argArr[k]);
			args = strtok(NULL, " ");
			k++;
		}
		argArr[spaces+1] = NULL;
		
		int pid;
		pid = fork();
		
		#ifdef DEBUG_PIPE
		//printf("---\nPID: %d\ni: %d\npipeVar: %d\n---\n", pid, i, pipeVar);		
		#endif
		//printf("I'm going in!\n");
		if(pid == 0) { //Child
			//printf("I'm on the other side!\n");
			if(pipeVar && i == 0) {
				//printf("I'm in the inner layer!\n");	
				//printf("p[0]: %d\np[1]: %d\n", p[0], p[1]);
				int dupCode;
				dupCode = dup2(p[1], 1);
				if(dupCode == -1) {
					fprintf(stderr, "dup2 error: %d\n", errno);
				}
				#ifdef DEBUG_PIPEIN
				fprintf(stderr, "errno: %d\n", errno);
				fprintf(stderr, "making input pipe\n");
				//printf("p[0]: %d\np[1]: %d\n", p[0], p[1]);
				fprintf(stderr, "dupCode: %d\n", dupCode);		
				#endif
				close(p[0]);
			}
			else if(pipeVar && i == 1) {
				errno = 0;
				int dupCode = dup2(p[0], 0);
				if(dupCode == -1) {
					fprintf(stderr, "error duping: %d\n", errno);
				}
				#ifdef DEBUG_PIPEOUT
				//printf("errno: %d\n", errno);
				printf("making output pipe\n");
				//printf("p[0]: %d\np[1]: %d\n", p[0], p[1]);
				//printf("dupCode: %d\n", dupCode);		
				#endif
				close(p[1]);
			}
			rc = execv(paths[i], argArr);
			#ifdef DEBUG_MODE
			fprintf(stderr, "rc: %d\n", rc);
			#endif	
		}
		else if( pid > 0 ){ //Parent
			int status;
			if(pipeVar) {
				if(i == 1) close(p[0]);
				if(i == 0) close(p[1]);
			}
			if(bg == 0) {
				//printf("waiting now!\n");
				waitpid( pid, &status, 0 );
				//printf("%d returned!\n", pid);	
			}
			else {
				printf("[running background process \"%s\"]\n", mainComs[i] ); 
				//int childPid = waitpid(pid, &status, WNOHANG);
			}
		}
		else { //error catching
			fprintf(stderr, "ERROR with Fork\n");	
			return(EXIT_FAILURE);
		}	
	}
	#ifdef DEBUG_EXECUTE
	printf("returning status: %d\n", rc);
	#endif
	return rc;
};

char * findCommand(char * com) {
	char * env = getenv("MYPATH");
	char * path = calloc(1, strlen(env) + 1);
	strcpy(path, env);
	char * paths = strtok(path, ":");
	while(paths != NULL) {
		char * wholePath = calloc(1, FILENAME_MAX);
		strcat( wholePath, paths  );
		strcat( wholePath, "/"  );
		strcat( wholePath, com  );
		#ifdef DEBUG_FIND
		printf("%s\n", wholePath);
		#endif
		struct stat status;
		int rc = lstat(wholePath, &status);
		if(rc == 0 && status.st_mode & S_IXUSR) {
			free(path);
			return(wholePath);
		}
		paths = strtok(NULL, ":");
	}
	free(paths);
	free(path);
	return(NULL);
}

int main() {

	char cDir[FILENAME_MAX]; //buffer var to store current directory
	char * input = calloc(1, 1025); //buffer var to hold input commands
	
	while(1) {
		if(getcwd(cDir, FILENAME_MAX) != NULL) {
			printf("%s$ ", cDir);
			fflush(stdout);
		}
		else {
			fprintf(stderr, "SOMETHING WENT WRONG\n");
			return EXIT_FAILURE;
		}
		
		int bg = 0; //var to save if the command should be run in the background	
		fgets(input, 1025, stdin); //get the user's command	
			
		char * last = input + strlen(input)-1;
		if(*last == '\n') {
			*last  = '\0';
		}
		last--;	
		while(last > input && isspace(*last)) {
			last --;
		}
		*(last+1) = '\0';
	
		if(strlen(input) == 0) {
			continue;
		}

		if(strcmp(input, "exit") == 0) break;

		if(*last == '&') {
		if(last-1) last --;
			*last = '\0';
			bg = 1;
		}
			

		if(strcmp(input, " ") == 0) {
			continue;
		}
		//check for pipe here
		char * checkPipe = input;
		char ** commands;
		int pipe = 0; //0 if no pipe, 1 if pipe
		if((checkPipe = strrchr(checkPipe, '|')) != NULL) {
			pipe++;	
			commands = calloc(2, sizeof(char *));
			char * ptr = checkPipe;
			ptr --;
			*ptr = '\0';
			checkPipe+=2;
			commands[0] = calloc(1, strlen(input) + 1);
			strcpy(commands[0], input);
			commands[1] = calloc(1, strlen(checkPipe) + 1);
			strcpy(commands[1], checkPipe);
		}
		else {
			commands = calloc(1, sizeof(char *));
			commands[0] = calloc(1, strlen(input)+1);
			strcpy(commands[0], input);
		}
		free(input);
		
		int i;
		#ifdef DEBUG_MODE
		printf("Pipe is %d\n", pipe);
		#endif
		char ** mainComs = calloc(pipe + 1, sizeof(char *));
		char ** locations = calloc(pipe + 1, sizeof(char *));
		for(i = 0; i <= pipe; i++) {
			
			char * com = strtok(commands[i], " ");
			mainComs[i] = calloc(1, strlen(com)+1);
			strcpy(mainComs[i], com);

			if(strcmp(mainComs[i], "cd") == 0) {
				locations[i] = calloc(1, 3);
				strcpy(locations[i], "cd");
			}
			else {
				locations[i] = findCommand(mainComs[i]);
				//if(locations[i]) printf("%s\n", locations[i]);
			}
			char * fixer = commands[i] + strlen(com);
			*fixer = ' ';
		}
	
		int rc = xCom(mainComs, locations, commands, bg, pipe);
		if(rc == 1) {
			fprintf(stderr, "There was an error\n");
				
		
		}
		int c;
		for(c = 0; c <= pipe; c++) {
			free(commands[c]);
			free(mainComs[c]);
			free(locations[c]);
		}
		free(commands);
		free(mainComs);
		free(locations);
	}
	
	printf("bye\n");	
	//free(input);


	
	return EXIT_SUCCESS;
}
