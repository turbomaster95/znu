#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linenoise.h>

/* Minimal completion: Tab will suggest "hello" or "help" if you type 'h' */
void completion(const char *buf, linenoiseCompletions *lc) {
    if (buf[0] == 'h') {
        linenoiseAddCompletion(lc, "hello");
        linenoiseAddCompletion(lc, "help");
    }
}

int main(int argc, char **argv) {
    char *line;
    const char *prompt = "znu> ";

    /* Set the completion callback for basic TAB support */
    linenoiseSetCompletionCallback(completion);

    printf("ZNU Shell Initialized. Type 'exit' to quit.\n");

    while(1) {
        /* This is the simplest way to use linenoise. 
           It blocks until the user presses Enter. */
        line = linenoise(prompt);

        /* Check for Ctrl+C or Ctrl+D (EOF) */
        if (line == NULL) break;

        /* Skip empty lines */
        if (line[0] != '\0') {
            
            /* Simple command handling */
            if (strcmp(line, "exit") == 0) {
                free(line);
                break;
            }

            /* Echo the command back (replace this with your shell logic) */
            printf("You entered: %s\n", line);

            /* Add to in-memory history (up arrow will work) */
            linenoiseHistoryAdd(line);
        }

        /* linenoise returns a heap-allocated string; we must free it */
        free(line);
    }

    return 0;
}
