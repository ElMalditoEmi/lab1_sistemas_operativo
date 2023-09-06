#include <stdbool.h>
#include <stdio.h>

#include "command.h"

#include "builtin.h"
#include "strextra.h"
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <libgen.h>
#include "./tests/syscall_mock.h"


/*
 * indica si el comando es un "cd"
 *
 * REQUIRES: cmd != NULL 
 *
 */
static bool builtin_scommand_is_cd(const scommand cmd)
{
    assert(cmd != NULL);

    return strcmp(scommand_front(cmd), BUILTIN_CD) == 0;
}

/*
 * Ejecuta el comando interno "cd"
 *
 * REQUIRES: 
 *
 */
static void builtin_run_cd(const scommand cmd)
{
    assert(cmd != NULL && builtin_scommand_is_cd(cmd));

    char *final_path = NULL;

    int chdir_ret = 0;

    scommand_pop_front(cmd); // elimino cd

    if (scommand_length(cmd) <= 1u)
    {
        char *input_path = NULL;

        if (!scommand_is_empty(cmd))
        {
            input_path = scommand_front(cmd);
            unsigned int path_len = strlen(input_path);
            
            if (input_path[0] == '~' && path_len == 1)
            { // caso ~ va a home
                final_path = getenv("HOME");
            }
            else
            { // otros casos
                final_path = scommand_front(cmd);
            }
        }
        else
        {
            final_path = getenv("HOME");
        }

        chdir_ret = chdir(final_path);

        if (chdir_ret != 0)
        {
            perror("mybash: cd"); // usa los errores de chdir
        }

    }

    else
    {
        printf("Error de cd: demasiados argumentos\n");
    }
}

/*
 * indica si el comando es un "exit"
 *
 * REQUIRES: cmd != NULL 
 *
 */
static bool builtin_scommand_is_exit(const scommand cmd)
{
    assert(cmd != NULL);

    return strcmp(scommand_front(cmd), BUILTIN_EXIT) == 0;
}

/*
 * Ejecuta el comando interno "exit"
 *
 * REQUIRES: 
 *
 */
static void builtin_run_exit(const scommand cmd)
{
    assert(builtin_scommand_is_exit(cmd) && cmd != NULL);
    // quit = true; se usa el quit de mybash.c ?
    exit(EXIT_SUCCESS);
}

/*
 * Ejecuta el comando interno "help"
 *
 * REQUIRES: cmd != NULL
 *
 */
static bool builtin_scommand_is_help(const scommand cmd)
{
    assert(cmd != NULL);

    return strcmp(scommand_front(cmd), BUILTIN_HELP) == 0;
}

/*
 * Ejecuta el comando interno "help"
 *
 * REQUIRES: 
 *
 */
static void builtin_run_help(const scommand cmd)
{
    assert(cmd != NULL && builtin_scommand_is_help(cmd));
    scommand_pop_front(cmd); // elimino help
    if (scommand_is_empty(cmd))
    {
        fprintf(stdout,
"\t,--. ,--.                                            ,-----.              ,--.      \n"
"\t|  .'   /,---.,--.--.,---.,--.--,--.,--,--,--,--.    |  |) /_ ,--,--.,---.|  ,---.  \n"
"\t|  .   '| .-. |  .--| .-. |  .--|  ||  |        |    |  .-.  ' ,-.  (  .-'|  .-.  | \n"
"\t|  |.   ' '-' |  |  ' '-' |  |  '  ''  |  |  |  |    |  '--':  '-'  .-'  `|  | |  | \n"
"\t`--' '--'`---'`--'   `---'`--'   `----'`--`--`--'    `------' `--`--`----'`--' `--' \n"
        "\t\t\t\t\x1b[32mKororum Bash, versión 1.1.13.01-release\x1b[37m"
                        "\n"
                        "\n"
                        "Autores:\n"
                        "\tElMalditoEmi (Emilio Pereyra)\n"
                        "\tArylu (Ariel Leiva)\n"
                        "\tDarkWolf (Camila Nanini)\n"
                        "\tXtian (Cristian Nieto)\n"
                        "\n"
                        "Comandos Internos:\n"
                        "\n"
                        "NOMBRE\n"
                        "\tcd - cambiar directorio\n"
                        "SINOPSIS\n"
                        "\tcd [FILE]\n"
                        "DESCRIPCIÓN\n"
                        "\tCambia al directorio especificado en FILE\n"
                        "OTROS USOS DE CD\n"
                        "\tcd ..  -> Retrocederá un directorio\n"
                        "\n"
                        "NOMBRE\n"
                        "\texit - salir\n"
                        "SINOPSIS\n"
                        "\texit \n"
                        "DESCRIPCIÓN\n"
                        "\tFinaliza la ejecución de Kororum Bash cerrandolo definitivamente\n" 
                        "\n"
        );
    }

    else
    {
        fprintf(stderr, "Error de help: demasiados argumentos. Intentar 'help'\n");
    }
}

bool builtin_is_internal(scommand cmd)
{
    assert(cmd != NULL);

    return (builtin_scommand_is_cd(cmd) || builtin_scommand_is_exit(cmd) || builtin_scommand_is_help(cmd));
}

bool builtin_alone(pipeline p)
{
    assert(p != NULL);

    return (pipeline_length(p) == 1) &&
           builtin_is_internal(pipeline_front(p));
}

void builtin_run(scommand cmd)
{

    assert(builtin_is_internal(cmd));

    if (builtin_scommand_is_cd(cmd))
    {
        builtin_run_cd(cmd);
    }

    else if (builtin_scommand_is_help(cmd))
    {
        builtin_run_help(cmd);
    }

    else if (builtin_scommand_is_exit(cmd))
    {
        builtin_run_exit(cmd);
    }
}