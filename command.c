// Librerias necesarias
#include <stdbool.h>
#include <assert.h>
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "strextra.h"
#include "command.h"

// Para debuggear ,no necesarias

struct scommand_s
{
    GList *args;
    char *redir_in;
    char *redir_out;
};

struct string_builder
{
    char *str;
    unsigned int last_char_pos;
};

scommand scommand_new(void)
{
    scommand new_scmd = NULL;
    new_scmd = malloc(sizeof(struct scommand_s));

    if (new_scmd == NULL) // En caso de que malloc falle
    {
        printf("No se pudo hacer malloc para scommand_new\n");
        exit(EXIT_FAILURE);
    }

    // Inicializacion
    new_scmd->args = NULL; // Las listas de GLib se inicializan al appendar el primer objeto
    new_scmd->redir_in = NULL;
    new_scmd->redir_out = NULL;

    assert(new_scmd != NULL && scommand_is_empty(new_scmd) &&
           scommand_get_redir_in(new_scmd) == NULL &&
           scommand_get_redir_out(new_scmd) == NULL);
    return new_scmd;
}

scommand scommand_destroy(scommand self)
{ // Cuando revisen con valgrind puede que haya memleaks ,pero es porque glib hace triquiñuelas que valgrind no entiende
    assert(self != NULL);
    // Liberando las lista de argumentos
    g_list_free_full(self->args, free);

    // Liberando los otros campos
    free(self->redir_in);
    free(self->redir_out);
    self->redir_in = NULL;
    self->redir_out = NULL;

    // Liberando el scommand
    free(self);
    self = NULL;

    assert(self == NULL);
    return self;
}

void scommand_push_back(scommand self, char *argument)
{
    assert(self != NULL && argument != NULL);
    self->args = g_list_append(self->args, argument);
    assert(!scommand_is_empty(self));
}

void scommand_pop_front(scommand self) // Parece raro pero lo re testee en gdb
{
    assert(self != NULL && !scommand_is_empty(self));
    GList *head = self->args;                          // hacer a head un alias de self->args
    self->args = g_list_remove_link(self->args, head); // Se hace "slice"

    // La cabeza de la lista queda como unico elemento de head
    g_list_free_full(head, free); // Liberar head y sus elementos
    head = NULL;
}

void scommand_set_redir_in(scommand self, char *filename)
{
    assert(self != NULL);
    if (self->redir_in != NULL)
    {
        free(self->redir_in);
    }
    self->redir_in = filename; // El TAD se apropia de la referencia ,no se hace copia
}

void scommand_set_redir_out(scommand self, char *filename)
{
    assert(self != NULL);
    if (self->redir_out != NULL)
    {
        free(self->redir_out);
    }
    self->redir_out = filename; // El TAD se apropia de la referencia ,no se hace copia
}

bool scommand_is_empty(const scommand self)
{ // Nos aprovechamos de que scommand_pop_front() ,deja 'self->args' en NULL, al popear todo
    assert(self != NULL);
    bool ret = self->args == NULL;
    return ret;
}

unsigned int scommand_length(const scommand self)
{
    assert(self != NULL);
    unsigned int ret;
    ret = g_list_length(self->args);
    assert((ret == 0) == scommand_is_empty(self));
    return ret;
}

char *scommand_front(const scommand self)
{
    assert(self != NULL && !scommand_is_empty(self));
    char *ret = NULL;
    ret = g_list_first(self->args)->data;
    assert(ret != NULL);
    return ret;
}

char *scommand_get_redir_in(const scommand self)
{
    assert(self != NULL);
    char *ret = self->redir_in;
    return ret;
}

char *scommand_get_redir_out(const scommand self)
{
    assert(self != NULL);
    char *ret = self->redir_out;
    return ret;
}

// Tener en mente que queremos algo de la forma "wc -l > a.out < b.in"
char *scommand_to_string(const scommand self)
{
    assert(self != NULL);

    GList *current_str = self->args;
    char *result = strdup("");

    while (current_str != NULL)
    {
        result = destructive_concat(&result, current_str->data);
        result = destructive_concat(&result, " ");
        current_str = g_list_next(current_str);
    }

    if (self->redir_in != NULL)
    {
        result = destructive_concat(&result, "< ");
        result = destructive_concat(&result, scommand_get_redir_in(self));
        result = destructive_concat(&result, " ");
    }

    if (self->redir_out != NULL)
    {
        result = destructive_concat(&result, "> ");
        result = destructive_concat(&result, scommand_get_redir_out(self));
        result = destructive_concat(&result, " ");
    }

    assert(scommand_is_empty(self) ||
           scommand_get_redir_in(self) == NULL ||
           scommand_get_redir_out(self) == NULL ||
           strlen(result) > 0);

    return result;
}

struct pipeline_s
{
    GList *cmd;
    bool w_o_r;
};

pipeline pipeline_new()
{
    pipeline result = malloc(sizeof(struct pipeline_s));
    if (result == NULL)
    {
        fprintf(stderr, "No se pudo hacer malloc para pipeline_new\n");
        exit(EXIT_FAILURE);
    }
    result->cmd = NULL;
    result->w_o_r = true;
    assert(result != NULL && pipeline_is_empty(result) && pipeline_get_wait(result));
    return result;
}

static void destroy_everyone(void *data)
{
    scommand kill = data;
    kill = scommand_destroy(kill);
}

pipeline pipeline_destroy(pipeline self) // No confío al 100% esta implementación revisar luego de test
{
    assert(self != NULL);
    g_list_free_full(self->cmd, destroy_everyone);
    self->cmd = NULL;
    free(self);
    self = NULL;
    assert(self == NULL);
    return self;
}

// Modificadores //

void pipeline_push_back(pipeline self, scommand sc)
{
    assert(self != NULL && sc != NULL);
    self->cmd = g_list_append(self->cmd, sc);
    assert(!pipeline_is_empty(self));
}

void pipeline_pop_front(pipeline self)
{
    assert((self != NULL) && !pipeline_is_empty(self));
    GList *linked = g_list_nth(self->cmd, 0);
    scommand killme = linked->data;
    self->cmd = g_list_delete_link(self->cmd, linked);
    scommand_destroy(killme);
    linked = NULL;
}

void pipeline_set_wait(pipeline self, const bool w)
{
    assert(self != NULL);
    self->w_o_r = w;
}

// Proyectores //

bool pipeline_is_empty(const pipeline self)
{
    assert(self != NULL);
    return (self->cmd == NULL);
}

unsigned int pipeline_length(const pipeline self)
{
    assert(self != NULL);
    unsigned int res = g_list_length(self->cmd);
    assert((res == 0) == pipeline_is_empty(self));
    return res;
}

scommand pipeline_front(const pipeline self)
{
    assert(self != NULL && !pipeline_is_empty(self));
    scommand result = g_list_nth_data(self->cmd, 0u);
    assert(result != NULL);
    return result;
}

bool pipeline_get_wait(const pipeline self)
{
    assert(self != NULL);
    return self->w_o_r;
}

char *pipeline_to_string(const pipeline self)
{
    assert(self != NULL);
    GList *cmsaconvertir = self->cmd;
    char *result = strdup("");

    if (cmsaconvertir == NULL)
    {
        printf("WARNING: Se recibió un comando vacío\n");
        // Simplemente retorna un string vacío, que luego el llamador deberá verificar
        return result;
    }
    else
    {

        while (g_list_next(cmsaconvertir) != NULL)
        {
            char *cmd_str = scommand_to_string(cmsaconvertir->data);
            result = destructive_concat(&result, cmd_str);
            result = destructive_concat(&result, "| ");
            cmsaconvertir = g_list_next(cmsaconvertir);
            free(cmd_str);
        }

        char *cmd_str = scommand_to_string(cmsaconvertir->data);
        result = destructive_concat(&result, cmd_str);
        free(cmd_str);

        if (!(self->w_o_r))
        {
            result = destructive_concat(&result, "& ");
        }
    }
    assert(pipeline_is_empty(self) || pipeline_get_wait(self) || strlen(result) > 0);
    return result;
}
