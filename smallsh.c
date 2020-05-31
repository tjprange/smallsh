/* 
Thomas Prange
CS 340
Small Shell
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>

typedef enum {false, true} bool;
bool running = true;
#define MAX_CHARS 2048
#define MAX_ARGS 512
#define ARG_SIZE 100

bool leftMarker; // t/f if < exists in command
int leftIndex;	// hold index of command < will be operated on
bool rightMarker; // t/f if > exists command
int rightIndex; // hold index of command > will be operated on

int exitStatus; // hold extis status value

char* commandLineArg; // string to hold the user entered command
char args[MAX_ARGS][ARG_SIZE];	// will hold args unless a special character ie "<" or ">"
char placeHolderArgs[MAX_ARGS][ARG_SIZE]; // will hold args from command line

int numArgs; // num args 

bool background; // t/f value for background process
int backgroundPC[100]; // holds the background processes
int numProcesses = 0;

bool sigtstpFlag = false;

void getCommand();

void catchSIGINT(int signo)
{
	printf("Exited with signal %d\n", signo);
	fflush(stdout);	
}

/* This will toggle the sigtstpFlag bool upon receipt of signal */
void catchSIGTSTP(){
	if(sigtstpFlag) {
		sigtstpFlag = false;
		printf("Exiting foreground-only mode\n");
		fflush(stdout);
	}
	else{
		sigtstpFlag = true;
		background = false;
		printf("Entering foreground-only mode (& is now ignored)\n");
		fflush(stdout);
	}
}

/* This function will free the dynamically allocated memory */
void freeMemory(){
	free(commandLineArg);
	commandLineArg = NULL;
}

/* 
This function will acquire input from the user and store the stdin contents into
the string commandLineArg. Sectios of this code were adapted from:
https://canvas.oregonstate.edu/courses/1764435/pages/3-dot-3-advanced-user-input-with-getline 
*/
void getCommand()
{
	size_t bufferSize = 0; // Holds how large the allocated buffer is
	clearerr(stdin);
	// Get input from the user
	printf(": ");
	fflush(stdout);
	// Get a line from the user
	getline(&commandLineArg, &bufferSize, stdin);

	// Remove the trailing \n that getline adds
	commandLineArg[strcspn(commandLineArg, "\n")] = '\0';
}

/* This will return true if the input begins with # or " " */
bool checkComment(){
	bool isComment = false;
	// if the first character of commandLineArg is # or " " do nothing
  	if (strncmp(commandLineArg, "#", 1) == 0 || strncmp(commandLineArg, " ", 1) == 0){
    	//printf("Bad arg!\n");
		isComment = true;
  	}
	return isComment;
}

/* This function will create substrings from the commandLineArg using " "
as a delimiter. Each substring (token) will be placed into a cooresponding location in the
args array. The number of args will be accumulated by the number of " " characters. */
tokenizeCommand(){
	int i = 0;	
	// If commandLineArg begins with # or " "
	if(checkComment() == true){
		// do nothing
		//printf("bad args\n");
		numArgs = 0;
	}
	else{// tokenize the args
		// grab first token
		char *token = strtok(commandLineArg, " ");	
		// and subsuquent tokens
  		while( token != NULL ) {
				strcpy(placeHolderArgs[i], token);   
				i++;
    			token = strtok(NULL, " ");					
		}
		// the number of args will be equivalent to the number of " " chars in commandLineArg	
		numArgs = i;
	}
}

/* This function will copy the contents of placeHolderArgs and place the values into args. < and >
values will not be copied. If either of these tokens are encountered, their location is marked and flag set
to true. This will mark the location of the argument to which < or > would be called on */
void removeTokens(){

	int i;
	int j = 0;
	int totalArgs = numArgs;
	// Copy strings into args, unless it is a < or > character
	for(i = 0; i < totalArgs; i++){
		if((strcmp(placeHolderArgs[i], "<") == 0) || (strcmp(placeHolderArgs[i], ">") == 0)){
			if (strcmp(placeHolderArgs[i], "<") == 0) {
				leftIndex = i;
				leftMarker = true;
			}
			if (strcmp(placeHolderArgs[i], ">") == 0) {
				rightIndex = i;
				rightMarker = true;
			}
			continue; 				
		}
		strcpy(args[j], placeHolderArgs[i]);	
		j++;
	}
	numArgs = j;
}

/* If the last item in args is & set flag to true */
void checkBackground(){
	background = false;
	if (strcmp(args[numArgs-1], "&") == 0){
		background = true;
		numArgs--;
	}
	if (sigtstpFlag) { background = false; }
}

/* This function will print the strings contained in the args array */
void printArgs(){
	int i;
	//printf("leftIndex %d, rightIndex %d\n", leftIndex, rightIndex);
	//printf("leftMarker: %d, rightMarker %d\n", leftMarker, rightMarker);
	printf("Num args: %d\n", numArgs);
	for (i = 0; i < numArgs; i++){		
		printf("%s\n", args[i]);
		fflush(stdout);
	}
}

/* This function prints the addresses in args */
void printArgAddresses(){
	int i;
	for (i = 0; i < numArgs; i++){		
		printf("%p\n", &args[i]);
		fflush(stdout);
	}	
}

/* This function will create a child process */
void forkChild(){
	pid_t spawnPid = -5;
	int childExitStatus = -5;
	int writeFD, readFD;

	spawnPid = fork();
	switch (spawnPid){
		case -1: { // something bad happened...
			perror("HULL BREACH!!!\n"); 
			exit(1); 
			break; 
		}
		case 0: { // child process
			// COPY ADDRESSES HERE
			char *addresses[numArgs+1];
			int i;
			for(i = 0; i < numArgs; i++){
				addresses[i] = args[i];
			}
			// Set last index to NULL
			addresses[numArgs] = NULL;
			

			if(leftMarker){ // redirect w/ dup2
				readFD = open(args[leftIndex], O_RDONLY, 0);
				if ( readFD < 0){
					printf("Can't open %s for input\n", args[leftIndex]);
					fflush(stdout);
					exit(1);
				}
				else {
					dup2(readFD, 0);
					close(readFD);
				}			
			}

			// if there was a > in the entered command, create a file at the argument location of n+1 from the index of >
			if(rightMarker){ 
				writeFD = open(args[rightIndex-leftIndex], O_WRONLY | O_CREAT | O_TRUNC, 0644);
				dup2(writeFD, 1);		
				addresses[rightIndex-leftIndex] = 0;	
				close(writeFD);		
			}
			
			if(background){ // redirect to /dev/null if background process
				writeFD = open("/dev/null", O_WRONLY, 0644);
				dup2(writeFD, 1);
				addresses[rightIndex - leftIndex];
				close(writeFD);
			}
			// call execvp
			if (execvp(addresses[0], addresses) < 0){
				perror("badfile: ");
				exit(1);
			}					
		}
		default: { // parent
			if(background){
				// hold spawnPID in array of processes
				backgroundPC[numProcesses] = spawnPid;
				// increment the number of processes
				numProcesses++;
				// parent doesnt wait
				waitpid(spawnPid, &childExitStatus, WNOHANG);
				printf("spawnPid of child process: %d\n", spawnPid);
				fflush(stdout);				
			}
			else{
				// parent will wait for child process to finish
				pid_t actualPid = waitpid(spawnPid, &childExitStatus, 0);			
				// if the process terminates normally then WIFEXITED returns non zero
				if (WIFEXITED(childExitStatus) != 0 ) {
					// save the status code
					exitStatus = WEXITSTATUS(childExitStatus);
				}
				else if (WIFSIGNALED(childExitStatus != 0)){ // child terminated with signal
					exitStatus = WTERMSIG(childExitStatus);
				}		
			}			 				
		}
	}
}

/* this will remove the $$ from any argument that contains &&.
The function was adapted from https://www.geeksforgeeks.org/remove-all-occurrences-of-a-character-in-a-string/ */
removeChars(char *arg, int c){
	int j, i;
	//printf("c: %c\n", arg);
	//printf("old string: %s\n", s);
	int n = strlen(arg);
	for(i=j=0; i < n; i++){
		if(arg[i] != c)
			arg[j++] = arg[i];
	}
	arg[j] = '\0';
}

/* This function will check for $$ in the arguments supplied by the user. 
If they exist, $$ will be replaced by the PID of the shell. */
void checkDollars(){
	int i;
	char pid[15];
	memset(pid,'\0', 15); // fill pid with \0
	for (i = 0; i < numArgs; i++){
		if (strstr(args[i], "$$")){ // if an arg contains $$ 
			removeChars(args[i], '$'); // remove the $ chars
			sprintf(pid, "%d", getpid()); // save pid 
			strcat(args[i], pid); // append pid to arg			
		}
	}
}

/* This function will determine which command to execute. If the command is not a "built in",
it will fork a child process. The child will then call exec on the first argument supplied. */
void commands(){
	if(strcmp(args[0], "exit") == 0){
		//printf("Exit!\n");
		exit(0);
	}
	else if(strcmp(args[0], "cd") == 0){
		//printArgs();
		if(numArgs == 1){ // no arguments supplied then go to home directory
			chdir(getenv("HOME"));			
		}
		else{ // navigate around weeeeeee!
			char dir[500];
			getcwd(dir, sizeof(dir));
			strcat(dir, "/");
			printf("%s\n", args[1]);
			strcat(dir, args[1]);
			chdir(dir);
		}		
	}
	else if(strcmp(args[0], "status") == 0){
		printf("exit value: %d\n", exitStatus);
	}
	else { // for a child and use exec on args[0]		
		forkChild();
	}
}

/* This function will reset the value of the global variables */
void resetGlobals(){
	leftIndex = 0;
	rightIndex = 0;
	leftMarker = false;
	rightMarker = false;
	background = false;
}

/* This function prints a message once a process has been terminated */
void runningProcesses(){
	int i;
	int childExitStatus = -5; // set value to a garbo number
	// get the termination status/value from all items in backgroundPC[]
	for (i = 0; i < numProcesses; i++){
		// check that the return values of waitpid are not 0 and dont wait using WNOHANG
		if(waitpid(backgroundPC[i], &childExitStatus, WNOHANG) > 0){
			if (WIFEXITED(childExitStatus)){ // normal termination
				printf("\nBackground processID %d ended normally with exit status: %d\n", backgroundPC[i] , WEXITSTATUS(childExitStatus));
			}
			if (WIFSIGNALED(childExitStatus)){ // terminated with signal
				printf("\nBackground processID: %d ended with a signal: %d\n", backgroundPC[i], WTERMSIG(childExitStatus));
				fflush(stdout);
			}			
		}
	}	
}

void main()
{
	// sigint
	struct sigaction SIGINT_action = {0};
	// handler for SIGINT
	SIGINT_action.sa_handler = catchSIGINT;
	sigfillset(&SIGINT_action.sa_mask);
	//SIGINT_action.sa_flags = SA_RESTART;
  	sigaction(SIGINT, &SIGINT_action, NULL);

	// sigtstp
	struct sigaction SIGTSTP_action = {0};
	// handler for SIGTSTP
	SIGTSTP_action.sa_handler = catchSIGTSTP;	
	sigfillset(&SIGTSTP_action.sa_mask);
	//SIGTSTP_action.sa_flags = SA_RESTART;
	sigaction(SIGTSTP, &SIGTSTP_action, NULL);	

  	while(running){		
		runningProcesses();
    	// acquire command line argument
    	getCommand();
		// split command into args array
    	tokenizeCommand();
		// remove < and > characters
		removeTokens();
		// expand $$ into PID if present
		checkDollars();
		// Set background to true if input ends with &
    	checkBackground();
		//printArgs();
		// Run shell commands
    	commands(); 
		
		//printArgAddresses();
		// free memory allocated for commandLineArg string
	    freeMemory();
		resetGlobals();
  	}  
}