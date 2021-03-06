#include <assert.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "job_list.h"
#include "string_vector.h"
#include "swish_funcs.h"

//Takes in a condition that results in a truthy value if an error happened, an error message, and error return value.
//Handles the error with those
#define HANDLE_ERR(errorCond,msg,retVal) if(errorCond){perror(msg);return retVal;}

#define MAX_ARGS 10

int tokenize(char *s, strvec_t *tokens) {
    // TODO Task 0: Tokenize string s
    // Assume each token is separated by a single space (" ")
    // Use the strtok() function to accomplish this
    // Add each token to the 'tokens' parameter (a string vector)
    // Return 0 on success, 1 on error

    //MAYBE clear tokens. For now I'm gonna assume it might already have information in it, and we're just ADDING strings.

    char *currentS = strtok(s, " ");
    while(currentS != NULL){
        strvec_add(tokens,currentS);
        currentS = strtok(NULL," ");
    }
    
    return 0;
}

int run_command(strvec_t *tokens) {
    // TODO Task 2: Execute the specified program (token 0) with the
    // specified command-line arguments
    // THIS FUNCTION SHOULD BE CALLED FROM A CHILD OF THE MAIN SHELL PROCESS
    // Hint: Build a string array from the 'tokens' vector and pass this into execvp()
    // Another Hint: You have a guarantee of the longest possible needed array, so you
    // won't have to use malloc.

    //What happens if there's too many tokens!?!?!? Return too many tokens error or something...

    //ok well here
    if (tokens->length > MAX_ARGS+1){
        perror("Too many tokens");
        return 1;
    }

    //doing task 3
    //setting default values if strvec doesn't find any redirect args. dup2 will be called with these values
    int fileDes=STDOUT_FILENO;
    int newFileDes=STDOUT_FILENO;

    //THIS PROBABLY WONT WORK. You redirect input and output
    //Ok so I can RECIEVE up to MAX_ARG arguments, It's just if they're >, >>, or <, then they don't get passed in
    //So you trim them I guess

    int redirIndex = strvec_find(tokens,">");
    //keeping track of the index of lowest redirect argument;
    int lowerRedirIndex;
    if(redirIndex>=0){
        //Found redirect output
        fileDes = open(strvec_get(tokens,redirIndex+1),O_WRONLY|O_TRUNC|O_CREAT,S_IRUSR | S_IWUSR);
        newFileDes=STDOUT_FILENO;
    } else {
        redirIndex = strvec_find(tokens,">>");
        if(redirIndex>=0){
            //Found redirect append output (and redirect regular output wasn't found.)
            //The project 2 page says we can assume only 1 output arg will be present.
            //Also only 1 redirect in and 1 redirect out.
            fileDes = open(strvec_get(tokens,redirIndex+1),O_WRONLY|O_APPEND|O_CREAT,S_IRUSR | S_IWUSR);
            newFileDes=STDOUT_FILENO;
        }
    }
    HANDLE_ERR(fileDes==-1,"Failed to open output file",1)

    //updating lowerRedirIndex
    lowerRedirIndex=redirIndex;

    dup2(fileDes,newFileDes);
    redirIndex = strvec_find(tokens,"<");

    if(redirIndex>=0){
        //Found redirect input
        
        //updating lowerRedirIndex
        if(lowerRedirIndex>redirIndex || lowerRedirIndex==-1){lowerRedirIndex=redirIndex;}

        fileDes = open(strvec_get(tokens,redirIndex+1),O_RDONLY);
        HANDLE_ERR(fileDes==-1,"Failed to open input file",1)

        newFileDes=STDIN_FILENO;
    }
    dup2(fileDes,newFileDes);

    //taking out the redirect args
    if(lowerRedirIndex!=-1){strvec_take(tokens,lowerRedirIndex);}

    //making an array with 1 more than the amount of arguments. Then making that last argument the NULL sentinel
    char * args[tokens->length + 1];
    args[tokens->length] = NULL;

    //putting each of the tokens into the args array
    for(int i=0;i<(tokens->length);i++){
        args[i]=strvec_get(tokens,i);
    }

    //calling exec
    if (execvp(args[0],args)==-1){
        perror("exec");
        return 1;
    }

    //um make sure to | perror("exec") | if exec fails.

    // TODO Task 3: Extend this function to perform output redirection before exec()'ing
    // Check for '<' (redirect input), '>' (redirect output), '>>' (redirect and append output)
    // entries inside of 'tokens' (the strvec_find() function will do this for you)
    // Open the necessary file for reading (<), writing (>), or appending (>>)
    // Use dup2() to redirect stdin (<), stdout (> or >>)
    // DO NOT pass redirection operators and file names to exec()'d program
    // E.g., "ls -l > out.txt" should be exec()'d with strings "ls", "-l", NULL

    // TODO Task 4: You need to do two items of setup before exec()'ing
    // 1. Restore the signal handlers for SIGTTOU and SIGTTIN to their defaults.
    // The code in main() within swish.c sets these handlers to the SIG_IGN value.
    // Adapt this code to use sigaction() to set the handlers to the SIG_DFL value.
    // 2. Change the process group of this process (a child of the main shell).
    // Call getpid() to get its process ID then call setpgid() and use this process
    // ID as the value for the new process group ID

    // Not reachable after a successful exec(), but retain here to keep compiler happy
    return 0;
}

int resume_job(strvec_t *tokens, job_list_t *jobs, int is_foreground) {
    // TODO Task 5: Implement the ability to resume stopped jobs in the foreground
    // 1. Look up the relevant job information (in a job_t) from the jobs list
    //    using the index supplied by the user (in tokens index 1)
    //    Feel free to use sscanf() or atoi() to convert this string to an int
    // 2. Call tcsetpgrp(STDIN_FILENO, <job_pid>) where job_pid is the job's process ID
    // 3. Send the process the SIGCONT signal with the kill() system call
    // 4. Use the same waitpid() logic as in main -- dont' forget WUNTRACED
    // 5. If the job has terminated (not stopped), remove it from the 'jobs' list
    // 6. Call tcsetpgrp(STDIN_FILENO, <shell_pid>). shell_pid is the *current*
    //    process's pid, since we call this function from the main shell process

    // TODO Task 6: Implement the ability to resume stopped jobs in the background.
    // This really just means omitting some of the steps used to resume a job in the foreground:
    // 1. DO NOT call tcsetpgrp() to manipulate foreground/background terminal process group
    // 2. DO NOT call waitpid() to wait on the job
    // 3. Make sure to modify the 'status' field of the relevant job list entry to JOB_BACKGROUND
    //    (as it was JOB_STOPPED before this)

    return 0;
}

int await_background_job(strvec_t *tokens, job_list_t *jobs) {
    // TODO Task 6: Wait for a specific job to stop or terminate
    // 1. Look up the relevant job information (in a job_t) from the jobs list
    //    using the index supplied by the user (in tokens index 1)
    // 2. Make sure the job's status is JOB_BACKGROUND (no sense waiting for a stopped job)
    // 3. Use waitpid() to wait for the job to terminate, as you have in resume_job() and main().
    // 4. If the process terminates (is not stopped by a signal) remove it from the jobs list

    return 0;
}

int await_all_background_jobs(job_list_t *jobs) {
    // TODO Task 6: Wait for all background jobs to stop or terminate
    // 1. Iterate throught the jobs list, ignoring any stopped jobs
    // 2. For a background job, call waitpid() with WUNTRACED.
    // 3. If the job has stopped (check with WIFSTOPPED), change its
    //    status to JOB_STOPPED. If the job has terminated, do nothing until the
    //    next step (don't attempt to remove it while iterating through the list).
    // 4. Remove all background jobs (which have all just terminated) from jobs list.
    //    Use the job_list_remove_by_status() function.

    return 0;
}
