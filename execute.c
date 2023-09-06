#include <unistd.h>
#include <assert.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h> // Para perror()
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "execute.h"
#include "builtin.h"
#include "command.h"
#include "./tests/syscall_mock.h"

typedef enum
{
    IN_FDESC,
    OUT_FDESC
} fdescriptor_t;

/*Se encarga de configurar el file descriptor (ysi)*/
static int configure_descriptor(const char *redir, fdescriptor_t fd_type)
{
    int retrn = -1; // En caso de error devolver un descriptor que expresa error
    /*Si pasaron una redirrecion no nula*/
    if (fd_type == OUT_FDESC && redir != NULL)
    {
        retrn = open(redir, O_WRONLY | O_CREAT, 0644); // 0644 es un codigo de chmod para que no pida permisos elevados
    }
    else if (fd_type == IN_FDESC && redir != NULL)
    {
        retrn = open(redir, O_RDONLY ,0444);
    }

    return retrn;
}

static void configure_all_io(scommand child_cmd)
{
    int in_descriptor;
    int out_descriptor;
    char *in_redir = scommand_get_redir_in(child_cmd);
    char *out_redir = scommand_get_redir_out(child_cmd);

    if (in_redir != NULL)
    {
        in_descriptor = configure_descriptor(in_redir, IN_FDESC);
        if (in_descriptor == -1)
        {
            exit(EXIT_FAILURE);
        }
        dup2(in_descriptor, STDIN_FILENO);
        close(in_descriptor);
    }

    if (out_redir != NULL)
    {
        out_descriptor = configure_descriptor(out_redir, OUT_FDESC);
        if (out_descriptor == -1)
        {
            exit(EXIT_FAILURE);
        }
        dup2(out_descriptor, STDOUT_FILENO);
        close(out_descriptor);
    }
}

/*
Poner en ejecucion al comando scmd ,que
no tiene que conectar pipe con otro.

Requires:assert(scmd!=NULL && !scommand_is_empty(scmd))
*/
static void external_single_run(scommand scmd, bool is_fg)
{
    assert(scmd != NULL && !scommand_is_empty(scmd));

    int pid = fork();
    /*Checkear si forkeamos bien*/
    if (pid == -1) // No pudimos forkear
    {
        perror("ERROR:no se pudo realizar el fork");
        exit(EXIT_FAILURE); // Capaz es muy extremo matar todo ,en todo caso dejar volver a myBash
    }

    /*En caso de haber forkeado bien comenzar la ejecucion*/
    if (pid == 0) // Camino del hijo
    {
        configure_all_io(scmd);

        char *exec_name = strdup(scommand_front(scmd));

        // WARNING: No debería, pero quizás arroje leaks
        char **args = malloc((scommand_length(scmd) + 1) * sizeof(char *));
        unsigned int i = 0u;
        while (!scommand_is_empty(scmd))
        {
            args[i] = strdup(scommand_front(scmd));
            scommand_pop_front(scmd);
            i++;
        }
        args[i] = NULL; // execvp pide NULL al final sabe dios para que

        execvp(exec_name, args);

        // Si execvp falla
        printf("El comando ingresado no existe\n");
        exit(EXIT_FAILURE);
    }
    else if (pid > 0 && is_fg) // Camino del padre si es foreground
    {
        wait(NULL); // Si no es foreground ,simplemente no llamamos a wait() con el padre
    }
}

static void external_piped_run(pipeline apipe)
{
    bool is_fg = pipeline_get_wait(apipe); // DECIME SI ES FOREGROUND LOCO NO TE HAGAS EL VIVO
    int pipe_fd[2];
    pid_t pid_child_1, pid_child_2;

    // Crear la tubería
    if (pipe(pipe_fd) == -1)
    {
        perror("Error al crear la tubería");
        exit(EXIT_FAILURE);
    }

    // Crear un proceso hijo para ejecutar "ls"
    pid_child_1 = fork();

    if (pid_child_1 == -1)
    {
        perror("Error al crear el proceso hijo para ls");
        exit(EXIT_FAILURE);
    }

    if (pid_child_1 == 0)
    {
        scommand child_1_scmd = pipeline_front(apipe);
        // Código del proceso hijo para "ls"
        close(pipe_fd[0]); // Cerrar el extremo de lectura de la tubería en el hijo

        // Redirigir la salida estándar al extremo de escritura de la tubería
        dup2(pipe_fd[1], STDOUT_FILENO);

        // Cerrar el descriptor de escritura de la tubería, ya no lo necesitamos
        close(pipe_fd[1]);

        configure_all_io(child_1_scmd);

        // WARNING: No debería, pero quizás arroje leaks
        char **args1 = malloc((scommand_length(child_1_scmd) + 1) * sizeof(char *));
        args1[scommand_length(child_1_scmd)] = NULL;
        int i = 0;
        while (!scommand_is_empty(child_1_scmd))
        {
            args1[i] = strdup(scommand_front(child_1_scmd));
            scommand_pop_front(child_1_scmd);
            i++;
        }
        execvp(args1[0], args1);

        // Si execvp falla
        printf("El primer comando ingresado no existe\n");
        exit(EXIT_FAILURE);
    }
    else
    {
        // Crear un proceso hijo para ejecutar "wc"
        pid_child_2 = fork();

        if (pid_child_2 == -1)
        {
            perror("Error al crear el proceso hijo para wc");
            exit(EXIT_FAILURE);
        }
        pipeline_pop_front(apipe);
        if (pid_child_2 == 0)
        {
            scommand child_2_scmd = pipeline_front(apipe);
            // Código del proceso hijo para "wc"
            close(pipe_fd[1]); // Cerrar el extremo de escritura de la tubería en el hijo

            // Redirigir la entrada estándar al extremo de lectura de la tubería
            dup2(pipe_fd[0], STDIN_FILENO);

            // Cerrar el descriptor de lectura de la tubería, ya no lo necesitamos
            close(pipe_fd[0]);

            configure_all_io(child_2_scmd);

            char **args2 = malloc((scommand_length(child_2_scmd) + 1) * sizeof(char *));
            args2[scommand_length(child_2_scmd)] = NULL;
            int u = 0;
            while (!scommand_is_empty(child_2_scmd))
            {
                args2[u] = strdup(scommand_front(child_2_scmd));
                scommand_pop_front(child_2_scmd);
                u++;
            }

            execvp(args2[0], args2);
            printf("El segundo comando ingresado no existe\n");
            exit(EXIT_FAILURE);
        }
        else
        {
            // Código del proceso padre
            close(pipe_fd[0]); // Cerrar el extremo de lectura de la tubería en el padre
            close(pipe_fd[1]); // Cerrar el extremo de escritura de la tubería en el padre

            // Esperar a que ambos procesos hijos terminen
            if (is_fg)
            {
                waitpid(pid_child_1, NULL, 0);
                waitpid(pid_child_2, NULL, 0);
            }
        }
    }
}

/*
Es el camino que la pipeline si su head no era un comando builtin

Principalmente esta para no anidar codigo y
contemplar los casos facilmente

Requires:(apipe != NULL && !pipeline_is_empty(apipe))
*/
static void handle_externals(pipeline apipe)
{
    assert(apipe != NULL && !pipeline_is_empty(apipe));

    scommand scmd = NULL;                  // Solo se usa esta var en caso de tener un solo comando en la pipe
    bool is_fg = pipeline_get_wait(apipe); // DECIME SI ES FOREGROUND LOCO NO TE HAGAS EL VIVO

    if (pipeline_length(apipe) == 1)
    {
        scmd = pipeline_front(apipe);
        external_single_run(scmd, is_fg); // Para este caso no es necesaria toda la pipe
    }

    /*NOTA:No defini mas este camino porque quizas estaria bueno generalizar un poco mas y q no importe si es fg o bg para cuando la pipe es de 2 cmds */
    else if (pipeline_length(apipe) > 1)
    {
        external_piped_run(apipe);
    }
}
/*------------------------------------------------------------------------------------------------------------*/

/* void execute_pipeline(pipeline apipe)
{
    handle_externals(apipe);
} */
/*LLAMADA PRINCIPAL*/
void execute_pipeline(pipeline apipe)
{
    assert(apipe != NULL);

    if (!pipeline_is_empty(apipe))
    {
        scommand scmd = pipeline_front(apipe);

        if (builtin_is_internal(scmd)) // Si el comando es builtin se encarga su respectivo modulo
        {
            builtin_run(scmd);
        }
        else if (!builtin_is_internal(scmd)) // Si el comando no es builtin, puede estar en bg o fg
        {
            handle_externals(apipe);
        }
    }
    else
    {
        printf("exec: Se recibió una pipeline vacía\n");
    }
}
