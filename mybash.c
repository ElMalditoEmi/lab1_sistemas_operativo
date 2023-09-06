#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>

#include "command.h"
#include "execute.h"
#include "parser.h"
#include "parsing.h"
#include "builtin.h"
#define MAX_CWD_SIZE 500

static void show_prompt(void)
{
    char cwd[MAX_CWD_SIZE];
    getcwd(cwd, MAX_CWD_SIZE);
    printf("\033[1;31mKororum:\033[1;36m%s$> \033[1;0m",cwd);
    fflush(stdout);
}

int main(int argc, char *argv[])
{
    pipeline pipe;
    Parser input;
    bool quit = false;

    input = parser_new(stdin);

    while (!quit)
    {
        show_prompt();
        pipe = parse_pipeline(input);

        quit = parser_at_eof(input);
        if (pipe != NULL)
        { // Si el pipe tiene comandos lo/s ejecuto y dsp lo destruyo para liberar memoria
            execute_pipeline(pipe);
            pipe = pipeline_destroy(pipe);
        }
        else if (pipe == NULL)
        {
            printf("\nERROR DE SINTAXIS:escriba 'help' para obtener mas información\n");
        }
    }
    parser_destroy(input);
    input = NULL;
    return EXIT_SUCCESS;
}