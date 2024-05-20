#include <fcntl.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <readline/readline.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>

typedef enum status {
    RUNNING,
    STOPPED,
    DONE
} status;

typedef enum cmd_type {
    BG,
    FG,
}cmd_type;

typedef enum pipe_arg {
    LEFT_ARG_PIPE,
    RIGHT_ARG_PIPE
}pipe_arg;

typedef struct process {
    char **argv;
    char *output_file;
    char *error_file;
    char *input_file;
    pipe_arg side;
}process;

typedef struct job {
    int jobID;
    int pgID;
    char *full_command;
    int *num_tokens;
    bool pipe_found;
    status status_job;
    cmd_type cmd_type;
    struct job *next;
    struct job *prev;
}job;

typedef struct job_type {
    bool background_job;
    bool bg_cmd;
    bool fg_cmd;
    bool jobs_cmd;
}job_type;


#define MAX_JOBS 20
#define PROMPT "#"

job *root_control_table = NULL;


int return_job_id(job **root_control_table) {
    int max_job_id = 1;
    job **moving_node = root_control_table;
    job *root = (*root_control_table);
    if((*moving_node) == NULL) {
        return (max_job_id);
    }
    while((*moving_node) != NULL) {
        if((*moving_node)->jobID > max_job_id) {
            max_job_id = (*moving_node)->jobID;
        }
        (*moving_node) = (*moving_node)-> next;
    }
    (*root_control_table) = (root);
    return (max_job_id+1);
}

void add_job(job **root_control_table, job *new_job) {
    job **moving_node = root_control_table;
    job *root = (*root_control_table);
    if((*moving_node) == NULL) {
        *(moving_node) = new_job;
        return;
    }
    while((*moving_node) != NULL) {
        if ((*moving_node)->next == NULL) {
            new_job->prev = *(moving_node);
            new_job->next = NULL;
            (*moving_node)->next = new_job;
            (*root_control_table) = (root);
            return;
        }
        else {
            *(moving_node) = (*moving_node)->next;
        }
    }

}

void remove_job(int jobid, job **root_control_table) {
     if(*root_control_table == NULL) {
        return;
    }
    job **moving_node = root_control_table;
    job *previous_node = *(root_control_table);
    job *root = (*root_control_table);
    job *next_node = (*moving_node)->next;
    // this is just for the first example 
    if((*moving_node) != NULL) {
        if((*moving_node)->jobID == jobid) {
            (*root_control_table) = (next_node);
            return;
        }
        (*moving_node) = (*moving_node)->next;
        (previous_node) = (*moving_node)->prev;
        (next_node) = (*moving_node)->next;
    }
    while((*moving_node) != NULL) {
        if((*moving_node)->jobID == jobid) {
            (previous_node)->next = (next_node);
            if(next_node!= NULL) {
                (next_node)->prev = (previous_node);
            }
            (*root_control_table) = (root);
            return;
        }
        else {
            (*moving_node) = (*moving_node)->next;
            if((*moving_node) != NULL) {
                (previous_node) = (*moving_node)->prev;
                (next_node) = (*moving_node)->next;
            }
            else {
                (previous_node) = NULL;
                (next_node) = NULL;
            }
        }
    }
    (*root_control_table) = (root);
    return;

}
void handle_stop_child() {
    int status;
    job *moving_node = root_control_table;
    while(moving_node != NULL) {
        if(moving_node->next == NULL) {
            moving_node->status_job = STOPPED;
            moving_node->cmd_type = BG;
            kill(-1*(moving_node->pgID), SIGTSTP);
            waitpid(-1 * (moving_node->pgID), &status, WNOHANG | WUNTRACED);
            break;
        }
         else {
                    moving_node = moving_node ->next;
            }
        }
 }

void bg_handler(job **root_control_table) {
    job *moving_node = *root_control_table;
    bool bg_executed = false;
    int pgID_executed = 0;
    int status;
    bool last_command = true;
    while (moving_node != NULL) {
        if (moving_node->next == NULL) {
            while(moving_node!= NULL) {
                if(moving_node->status_job == STOPPED) {
                    pgID_executed = moving_node->pgID;
                    char add_on[] = " &";
                    moving_node->full_command = strcat(moving_node->full_command, add_on);
                    moving_node->cmd_type = BG;
                    kill(-(pgID_executed), SIGCONT);
                    moving_node->status_job = RUNNING;
                    bg_executed = true;
                    if(last_command) {
                        printf("[%d]+ \t%s\n", moving_node->jobID, moving_node->full_command);
                    }
                    else {
                        printf("[%d]- \t%s\n", moving_node->jobID, moving_node->full_command);
                    }
                    return;

                }
                else {
                    moving_node = moving_node->prev;
                    last_command = false;
                }
            }
            
        }
        else {
            moving_node = moving_node->next;
        }

    }
    if (bg_executed) {
         waitpid(-(pgID_executed), &status, WNOHANG | WUNTRACED);
            if(moving_node->pipe_found) {
                waitpid(-(pgID_executed), &status, WNOHANG | WUNTRACED);
            }
    }
    return;
}

void job_handler(job **root_control_table) {
    job *moving_node = *root_control_table;
    while (moving_node != NULL) {
        char *status_job_string;
        if(moving_node->status_job == RUNNING) {
            status_job_string = "RUNNING";
        }
        else if (moving_node->status_job == STOPPED) {
            status_job_string = "STOPPED";
        }
        else if (moving_node->status_job == DONE) {
            status_job_string = "DONE";
        }

        if (moving_node->next == NULL) {
            printf("[%d]+ %s\t\t%s\n", moving_node->jobID, status_job_string, moving_node->full_command);
        }
        else {
            printf("[%d]- %s\t\t%s\n", moving_node->jobID, status_job_string, moving_node->full_command);
        }
        moving_node = moving_node->next;
    }
    return;
}

void fg_handler(job **root_control_table) {
    job *moving_node = *root_control_table;
    bool fg_executed = false;
    int pgID_executed = 0;
    int status;
    int jobID_final;
    while (moving_node != NULL) {
        if (moving_node->next == NULL) {
            jobID_final = moving_node->jobID;
            pgID_executed = moving_node->pgID;
            moving_node->cmd_type = FG;
            kill(-(pgID_executed), SIGCONT);
            kill((pgID_executed), SIGCONT);
            moving_node->status_job = RUNNING;
            fg_executed = true;
            printf("%s\n", moving_node->full_command);
            break;
        }
        else {
            moving_node = moving_node->next;
        }
    }
    if (fg_executed) {
        waitpid(-1 * pgID_executed, &status, WUNTRACED);
        if(WIFSTOPPED(status)) {
            handle_stop_child();
        }
        else if(WIFSIGNALED(status)) {
                    if(WTERMSIG(status) == SIGINT) {
                        remove_job(jobID_final, root_control_table);
                    }
                }
        if(moving_node->pipe_found) {
            waitpid(-1 * pgID_executed, &status, WUNTRACED);
            if (!WIFSTOPPED(status)) {
                remove_job(jobID_final, root_control_table);
            }
            else if(WIFSTOPPED(status)) {
                handle_stop_child();
            }
            else if(WIFSIGNALED(status)) {
                    if(WTERMSIG(status) == SIGINT) {
                        remove_job(jobID_final, root_control_table);
                    }
                }
        }
        if (!(moving_node->pipe_found)) {
            if (!WIFSTOPPED(status)) {
                remove_job(jobID_final, root_control_table);
            }
            else if(WIFSIGNALED(status)) {
                    if(WTERMSIG(status) == SIGINT) {
                        remove_job(jobID_final, root_control_table);
                    }
                }
        }
    }
}

void check_commands(char **token_array[], int num_tokens, job_type *job_cond, job **root_control_table) {
    if(num_tokens > 0){
        if (strcmp((*token_array)[num_tokens-1],"&") == 0) {
            job_cond->background_job = true;
        }
        if(strcmp((*token_array)[0],"bg") == 0) {
            job_cond->bg_cmd = true;
            bg_handler(root_control_table);
        }
        if (strcmp((*token_array)[0],"jobs") == 0) {
            job_cond->jobs_cmd = true;
            job_handler(root_control_table);
        }
        if (strcmp((*token_array)[0],"fg") == 0) {
            job_cond->fg_cmd = true;
            fg_handler(root_control_table);
        }
    }
}

void signal_handler(int signal) {
    job *moving_node = root_control_table;
    int status = 0;
    if (signal == SIGCHLD) {
        while(moving_node != NULL) {
            int return_pgid = waitpid(-1*(moving_node->pgID), &status, WNOHANG | WUNTRACED);
            bool last_command = false;
            if(moving_node->next == NULL) {
                last_command = true;
            }
            if(return_pgid == moving_node->pgID) {
                if(!WIFSTOPPED(status)){
                    
                    if(moving_node->cmd_type == BG) {
                        remove_job(moving_node->jobID, &root_control_table);
                        if(last_command) {
                            printf("[%d] + Done\t\t%s\t\n", moving_node->jobID, moving_node->full_command);
                        }
                        else {
                            printf("[%d] - Done\t\t%s\t\n", moving_node->jobID, moving_node->full_command);
                        }
                    }
                    else {
                        remove_job(moving_node->jobID, &root_control_table);
                    }
                }
                else if(WIFSTOPPED(status)) {
                    handle_stop_child();
                }
                else if(WIFEXITED(status)) {
                    remove_job(moving_node->jobID, &root_control_table);
                }
                else if(WIFSIGNALED(status)) {
                    if(WTERMSIG(status) == SIGINT) {
                        remove_job(moving_node->jobID, &root_control_table);
                    }
                }
            }
            moving_node = moving_node->next;
        }
    }
}

int create_fork_processes(process *cmd, bool pipe_found, int pipfd[], int pgID, bool no_pg, job_type job_type, bool *unsuccessful_cmd) {
    int cpid = fork();
    bool file_redirection = false;
    if(cpid == 0) {
        signal(SIGTSTP, SIG_DFL);
        signal(SIGCHLD,SIG_IGN);
        signal(SIGINT, SIG_DFL);
        signal(SIGTTOU, SIG_IGN);
        if(no_pg) {
            int error = setpgid(0,0);
            if(error < 0) {
                return -1;
            }
        }
        else if(!no_pg){
            int error = setpgid(0,pgID);
            if(error < 0) {
                return -1;
            }
        }
        if(pipe_found) {
            if(cmd->side == LEFT_ARG_PIPE) {
                close(pipfd[0]);
                dup2(pipfd[1], STDOUT_FILENO);
            }
            //right side is 
            else if(cmd->side == RIGHT_ARG_PIPE) {
                close(pipfd[1]);
                dup2(pipfd[0], STDIN_FILENO);
            }
        }

        if(cmd->input_file !=NULL) {
            int fd = open(cmd->input_file,O_RDONLY);
            if(fd < 0) {
                return -1;
            }
            dup2(fd,STDIN_FILENO);
            file_redirection = true;
        }
        if(cmd->output_file !=NULL) {
            int fd = open(cmd->output_file, O_CREAT | O_WRONLY, 0644); 
            if(fd < 0) {
                return -1;
            }
            dup2(fd,STDOUT_FILENO);
            file_redirection = true;
        }
        if(cmd->error_file !=NULL) {
            int fd = open(cmd->error_file, O_CREAT | O_WRONLY, 0644);  
            if(fd < 0) {
                return -1;
            }
            dup2(fd, STDERR_FILENO);
            file_redirection = true;
        }
        if(job_type.background_job != true ) {
                if(no_pg) {
                tcsetpgrp(STDIN_FILENO,getpid()); 
            }
            if(!no_pg) {
                tcsetpgrp(STDIN_FILENO,pgID); 
            }

        }
        if(cmd->argv[0] != NULL) {
                int valid_cmd = execvp(cmd->argv[0],cmd->argv);
                if(valid_cmd == -1) {
                    *unsuccessful_cmd = true;
                    exit(-1);
                }
        }
    
    } else {
        if(no_pg) {
            setpgid(cpid, 0);
        }
        else {
            setpgid(cpid,pgID);
        }
    }
    return cpid;

}
//token array has the arguemnts for the command including the command 
//process includes the actual process that is oging to be running 
//end_index has the index of where the process ends 
bool redirect_pip_checker(char **token_array[], process *proc, int *end_index, bool *pipe_found, int num_tokens) {
    bool error_found = false; 
    bool redirect_found = false;
    for(int i = *(end_index); i< num_tokens; i++) {
        if ((*token_array)[i] == NULL) {
            continue;
        }
        if (strcmp((*token_array)[i],"|") == 0) {
            if((i+1) < num_tokens && i!=0) {
                *pipe_found = true;
                *end_index = i+1;
                (*token_array)[i] = NULL;
                break;
            }
            else {
                error_found = true;
                break;
            }
        }

        if (strcmp((*token_array)[i],"<") == 0) {
            if((i+1) < num_tokens && i!=0) {
                redirect_found = true;
                proc->input_file = (*token_array)[i+1];
            }
            else {
                error_found = true;
                break;
            }
            
        }else if (strcmp((*token_array)[i],">") == 0) {
            if((i+1) < num_tokens && i!=0) {
                redirect_found = true;
                proc->output_file = (*token_array)[i+1];
            }
            else {
                error_found = true;
                break;
            }
            
        }else if (strcmp((*token_array)[i],"2>") == 0) {
            if((i+1) < num_tokens && i!=0) {
                redirect_found = true;
                proc->error_file = (*token_array)[i+1];
            }
            else {
                error_found = true;
                break;
            }
            
        }

        if (redirect_found) {
            (*token_array)[i] = NULL;
            (*token_array)[i+1] = NULL;
        }
    }
    return error_found;
}


int main() {
    int global_main_pg = getpgrp();
    signal(SIGCHLD, signal_handler);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGINT, SIG_IGN);
    signal(SIGTTOU,SIG_IGN);

    while(true) {

        char *value = readline(PROMPT);
        char **token_array = (char **)malloc(sizeof(char*) * 70);
        
        //if there is no command 
        if (value == NULL) {
            printf("\n");
             break;
        }
        char *full_com = strdup(value); //copying the whole command - need to store it for future commands 
        char *token = strtok(value, " ");
        int num_tokens = 0;

        while(token != NULL){
            token_array[num_tokens++] = token;
            token = strtok(NULL, " ");
        }
        token_array[num_tokens] = NULL;
        if(num_tokens == 0) {
            continue;
        }
        job_type job_cond = {false, false, false,false};
        check_commands(&token_array, num_tokens, &job_cond, &root_control_table);
        if(job_cond.bg_cmd || job_cond.fg_cmd || job_cond.jobs_cmd) {
            continue;
        }
        if(job_cond.background_job) {
            num_tokens = num_tokens-1;
            token_array[num_tokens] = NULL;
        }
        // all the piping and redirection will be done here 
        process cmd_left = {token_array, NULL, NULL, NULL, LEFT_ARG_PIPE};
        process cmd_right = {NULL, NULL, NULL, NULL, RIGHT_ARG_PIPE};
        bool pipe_found = false;
        int end_index = 0;
        bool error_found;

        error_found = redirect_pip_checker(&token_array, &cmd_left, &end_index, &pipe_found, num_tokens);
        if(pipe_found) {
            //need to seperate the two processes 
            cmd_right.argv = token_array+(end_index);
            error_found = redirect_pip_checker(&token_array, &cmd_right, &end_index, &pipe_found, num_tokens);
        }
        if(error_found) {
            continue;
            //if an error exists then go to the next command wait for input   
        }

        int pipefd[2];
        pipe(pipefd);

        bool unsuccessful_cmd = false;
        int pgid = create_fork_processes(&cmd_left,pipe_found,pipefd,0,true, job_cond, &unsuccessful_cmd);
        if (pgid < 0 || unsuccessful_cmd == true) {
            continue;
        }
        int pg_right_id = 0;
        if(pipe_found) {
             pg_right_id = create_fork_processes(&cmd_right,pipe_found,pipefd,pgid,false, job_cond, &unsuccessful_cmd);
        }

        job *job_v = malloc(sizeof(job));
        job_v->jobID = return_job_id(&root_control_table);
        job_v->pgID = pgid;
        job_v->full_command = full_com;
        job_v->num_tokens = &num_tokens;
        job_v->status_job = RUNNING;
        job_v->next = NULL;
        job_v->prev = NULL;
        job_v->pipe_found = pipe_found;
        if(job_cond.background_job) {
            job_v->cmd_type = BG;
        }
        else {
            job_v->cmd_type = FG;
        }
        add_job(&root_control_table, job_v);

        close(pipefd[0]);
        close(pipefd[1]);
        int status = 0;

        if(job_cond.background_job) {
            waitpid(-1 * pgid, &status, WNOHANG | WUNTRACED);
            if(pipe_found) {
                waitpid(-1 * pgid, &status, WNOHANG | WUNTRACED);
            }
        }
        else {
            waitpid(-1 * pgid, &status, WUNTRACED);
            if(WIFSTOPPED(status)) {
                    handle_stop_child();
                }
            if(pipe_found) {
                waitpid(-1 * pgid, &status, WUNTRACED);
                if (!WIFSTOPPED(status)) {
                    remove_job(job_v->jobID, &root_control_table);
                }
                else if(WIFSTOPPED(status)) {
                    handle_stop_child();
                }
            }
            if (!pipe_found) {
                if (!WIFSTOPPED(status)) {
                remove_job(job_v->jobID, &root_control_table);
             }
            }
        }
        tcsetpgrp(STDIN_FILENO,global_main_pg); 
    }
    return 0;
}
 
