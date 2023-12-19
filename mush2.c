#include <stdio.h>

#include <mush.h>

#include <unistd.h>

#include <sys/stat.h>

#include <fcntl.h>

#include <signal.h>

#include  <stdlib.h>

#include   <pwd.h>

#include <errno.h>

int forkData(struct pipeline * );
void sig_handler(int);
int interrupt;
int runFile;
int main(int argc, char * argv[]) {
  interrupt = 0;
  char * userInput, * longString;
  struct pipeline * outPipeline;
  FILE * fp;
  int running;
  running = 1;
  runFile = 1;
  if (argc == 2) {
    fp = fopen(argv[1], "r");
    while(runFile){
         // read commands from input file
    signal(SIGINT, sig_handler);
    if (NULL == fp) {
      exit(1);
      // Failed to Open file
    }
    longString = readLongString(fp); //A function written by my proffessor. Reads a string of any length into a buffer. Stops at EOF or \n.
    if (NULL != longString) {
      outPipeline = crack_pipeline(longString); //Returns a list of structs that contain each command. Also written by proffessor. 
      if (outPipeline != NULL) {
        forkData(outPipeline);
        free_pipeline(outPipeline); //Frees pipeline mem. Also written by proffessor 
      }
    }
    if(longString == NULL && errno != EINTR) {
      break;
    }
    }
  }
  if (argc == 1) {
    // Prompt user for commands
    signal(SIGINT, sig_handler);
    while (running) {
      printf("%s", "8-P ");
      fp = fdopen(STDIN_FILENO, "r");
      if (NULL == fp) {
        // Failed to open STDIN
        exit(1);
      }
      longString = readLongString(fp);
      if (NULL != longString) {
        outPipeline = crack_pipeline(longString);
        if (outPipeline != NULL) {
          forkData(outPipeline);
          free_pipeline(outPipeline);
        }
      }
      if (longString == NULL && errno != EINTR) {
        break;
      }
    }
  }
  return 0;
}

void sig_handler(int sig) {
  // Interrupt handler
  interrupt = 1;
  char c;
runFile = 0;
  signal(sig, SIG_IGN);
  printf("%s", "Interrupted \n");
  signal(SIGINT, sig_handler);

}

void closePipe(int pipe[][2], int length) {
  // Closes pipe pairs
  int i;
  for (i = 0; i < length; i++) {
    close(pipe[i][0]);
    close(pipe[i][1]);
  }
}

int forkData(struct pipeline * p) {
  int i, child, execStatus, chdirStatus, numChildren, inFD, outFD, status;
  status = 0;

  // Create list of pipes
  int lastPipe[p -> length][2];
  for (i = 0; i < p -> length; i++) {
    if (-1 == pipe(lastPipe[i])) {
      perror("Failed pipe");
    }
  }

  for (i = 0; i < p -> length; i++) {
    if (interrupt) {
      interrupt = 0;
      return 1;
    }
    if (strcmp(p -> stage[i].argv[0], "cd") == 0) {
      char * env;
      struct passwd * pw;
      if (p -> stage[i].argv[1] != NULL) {
        chdirStatus = chdir(p -> stage[i].argv[1]);
      } else {
        env = getenv("HOME");
        if (NULL == env) {
          pw = getpwuid(getuid());
          env = pw -> pw_dir;
        }
        if (NULL == env) {
          printf("“unable to determine home directory ");
          exit(1);
        }
        chdirStatus = chdir(env);
      }
      if (-1 == chdirStatus) {
        exit(1);
        // Failed to change dir
      }
      break;
    }
    child = fork();
    numChildren++;
    int j;
    if (0 == child) {
      if (1 == p -> length) {
        if (p -> stage[i].inname != NULL) {
          inFD = open(p -> stage[i].inname, O_RDONLY);
          if (-1 == inFD) {
            // Error Opening file
            exit(1);
          }
          dup2(inFD, STDIN_FILENO);
          close(inFD);
          execStatus = execvp(p -> stage[i].argv[0], p -> stage[i].argv);
          if (execStatus == -1) {
            // Exec Error
            exit(1);
          }
        }
        if (p -> stage[i].outname != NULL) {
          remove(p -> stage[i].outname);
          outFD = open(p -> stage[i].outname, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
          if (-1 == outFD) {
            // Error Creating file
            exit(1);
          }
          dup2(outFD, STDOUT_FILENO);
          close(outFD);
          execStatus = execvp(p -> stage[i].argv[0], p -> stage[i].argv);
          if (execStatus == -1) {
            // Exec Error
            exit(1);
          }
        }
        if (p -> stage[i].outname == NULL && p -> stage[i].inname == NULL) {
          execStatus = execvp(p -> stage[i].argv[0], p -> stage[i].argv);
          if (execStatus == -1) {
            // Exec Error
            exit(1);
          }
        }

      }
      // CODE INSIDE CHILD
      if (0 == i) {
        // First Case
        if (p -> stage[i].inname != NULL) {
          inFD = open(p -> stage[i].inname, O_RDONLY);
          if (-1 == inFD) {
            // Error Opening file
            exit(1);
          }
          dup2(inFD, STDIN_FILENO);
          close(inFD);
        }
        if (-1 == dup2(lastPipe[i][1], STDOUT_FILENO)) {
          perror("Error on first pipe");
        }
        closePipe(lastPipe, p -> length);
        execStatus = execvp(p -> stage[i].argv[0], p -> stage[i].argv);

      } else if (p -> length - 1 == i) {
        if (p -> stage[i].outname != NULL) {
          remove(p -> stage[i].outname);
          outFD = open(p -> stage[i].outname, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
          if (-1 == outFD) {
            // Error Creating file
            exit(1);
          }
          dup2(outFD, STDOUT_FILENO);
          close(outFD);
        }
        if (-1 == dup2(lastPipe[i - 1][0], STDIN_FILENO)) {
          perror("Error on last pipe");
        }
        closePipe(lastPipe, p -> length);
        execStatus = execvp(p -> stage[i].argv[0], p -> stage[i].argv);
        // Last Case

      } else {
        if (-1 == dup2(lastPipe[i - 1][0], STDIN_FILENO)) {
          perror("Error on mid pipe in ");
        }
        if (-1 == dup2(lastPipe[i][1], STDOUT_FILENO)) {
          perror("Error on mid pipe out");
        }
        closePipe(lastPipe, p -> length);
        execStatus = execvp(p -> stage[i].argv[0], p -> stage[i].argv);

        return 1;
        // Middle Case

      }
    }
  }
// Close pipes and wait for children.
  closePipe(lastPipe, p -> length);
  int num;
  num = 0;
  while (wait( & status) > 0);

  return 0;
}