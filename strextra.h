#ifndef _STREXTRA_H_
#define _STREXTRA_H_


char * strmerge(char *s1, char *s2);
/*
 * Concatena las cadenas en s1 y s2 devolviendo nueva memoria (debe ser
 * liberada por el llamador con free())
 *
 * USAGE:
 *
 * merge = strmerge(s1, s2);
 *
 * REQUIRES:
 *     s1 != NULL &&  s2 != NULL
 *
 * ENSURES:
 *     merge != NULL && strlen(merge) == strlen(s1) + strlen(s2)
 *
 */

char * destructive_concat(char **dst, char *src);
/**
 * Retorna la concatenación de *dst y src, pero además libera la memoria utilizada por *dst. 
 * 
 * USAGE:
 * 
 * concat = desctructive_concat(char *dst, char *src);
 * 
 * REQUIRES:
 *     dst != NULL && *dst != NULL && src != NULL && dst está alojado en memoria dinámica
 * 
 * ENSURES:
 *     *dst == NULL && Memoria usada por el antiguo dst liberada
 * 
 * 
 */


#endif
