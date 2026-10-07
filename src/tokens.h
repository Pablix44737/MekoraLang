#ifndef TOKENS_H
#define TOKENS_H

/* Devuelve el nombre legible de un token, para el modo --tokens.
 * Permite probar el analizador lexico aislado, antes de que el parser
 * exista o consuma su salida. */
const char *nombre_token(int token);

#endif
