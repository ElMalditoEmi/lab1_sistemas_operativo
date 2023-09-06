#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>
#include "parsing.h"
#include "parser.h"
#include "command.h"

static void cmd_set_string(scommand cmd, char *string, arg_kind_t type)
{
    switch (type)
    {
    case ARG_NORMAL:
        scommand_push_back(cmd, string);
        break;
    case ARG_INPUT:
        scommand_set_redir_in(cmd, string);
        break;
    default:
        scommand_set_redir_out(cmd, string);
        break;
    }
}

static bool missing_io = false;

static scommand parse_scommand(Parser p)
{
    assert(p != NULL);

    scommand cmd = NULL;
    char *c_string = NULL;
    arg_kind_t c_type = ARG_NORMAL;
    bool missing_str = false, error = false;
    unsigned int counter = 0u;

    if (p != NULL && !parser_at_eof(p))
    {
        cmd = scommand_new();

        // parser_op_pipe y parser_op_background se comen "|" y "&", lo cual puede traer problemas
        while (!parser_at_eof(p) && !error && !missing_str)
        {
            c_string = parser_next_argument(p, &c_type);
            ++counter;

            // Almacena si y solo si c_string != NULL.
            // NOTA: (s_string == NULL && c_type == ARG_NORMAL) no necesariamente significa error (puede ser porque hay un "|" o "&")
            if (c_string != NULL)
            {
                cmd_set_string(cmd, c_string, c_type);
            }
            else
            {
                missing_str = true;
                error = (counter == 1);
            }
        }

        // Verificacion por si se da que el bucle termina por falta de string de entrada o salida
        if ((missing_str && c_type != ARG_NORMAL))
        {
            cmd = scommand_destroy(cmd);
            missing_io = true;
        }
        else
        {
            if (error)
            {
                cmd = scommand_destroy(cmd);
            }
        }
    }

    return cmd;
}

pipeline parse_pipeline(Parser p)
{
    assert(p != NULL && !parser_at_eof(p));

    pipeline result = pipeline_new();
    pipeline_set_wait(result, true);
    missing_io = false;

    scommand cmd = NULL;
    bool error = false, more = true, on_pipe = false, on_back = false, background = false;

    parser_op_pipe(p, &on_pipe);
    parser_op_background(p, &on_back);
    // Solo se ejecutarán el resto de partes si el primer simbolo no es "|" ni "&"
    error = on_pipe || on_back;

    if (!error)
    {
        cmd = parse_scommand(p);
        error = cmd == NULL || scommand_is_empty(cmd);

        if (!error)
        {
            pipeline_push_back(result, cmd);
            parser_op_pipe(p, &on_pipe);
        }

        while (!parser_at_eof(p) && more && !error && !background)
        {
            cmd = parse_scommand(p);

            if (cmd != NULL)
            {
                pipeline_push_back(result, cmd);
                parser_op_pipe(p, &on_pipe);
            }
            else
            {
                parser_op_background(p, &on_back);
                background = background || on_back;
            }

            more = cmd != NULL;
        }

        if (background)
        {
            pipeline_set_wait(result, false);
        }

        if (!parser_at_eof(p))
        {
            bool garbage = false;
            parser_garbage(p, &garbage);
            error = error || garbage;
        }
    }

    if (error || pipeline_is_empty(result) || missing_io)
    {
        result = pipeline_destroy(result); // Se libera la memoria usada y se retorna NULL
    }

    return result;
}
