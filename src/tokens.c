#include "parser.tab.h"
#include "tokens.h"

const char *nombre_token(int token)
{
    switch (token) {
    case MACHINE:      return "MACHINE";
    case PROCEDURE:    return "PROCEDURE";
    case MAIN:         return "MAIN";
    case CONST:        return "CONST";
    case VAR:          return "VAR";
    case IF:           return "IF";
    case ELSE:         return "ELSE";
    case REPEAT:       return "REPEAT";
    case AS:           return "AS";
    case TIPO_NUMBER:  return "TIPO_NUMBER";
    case TIPO_BOOL:    return "TIPO_BOOL";
    case TRUE:         return "TRUE";
    case FALSE:        return "FALSE";
    case AND:          return "AND";
    case OR:           return "OR";
    case NOT:          return "NOT";

    case EJE_X:        return "EJE_X";
    case EJE_Y:        return "EJE_Y";
    case EJE_Z:        return "EJE_Z";
    case EJE_E:        return "EJE_E";
    case EJE_F:        return "EJE_F";
    case EJE_S:        return "EJE_S";

    case NUMERO:       return "NUMERO";
    case ID:           return "ID";
    case CADENA:       return "CADENA";

    case MAS:          return "MAS";
    case MENOS:        return "MENOS";
    case POR:          return "POR";
    case DIV:          return "DIV";
    case MOD:          return "MOD";

    case IGUAL_IGUAL:  return "IGUAL_IGUAL";
    case DISTINTO:     return "DISTINTO";
    case MENOR:        return "MENOR";
    case MAYOR:        return "MAYOR";
    case MENOR_IGUAL:  return "MENOR_IGUAL";
    case MAYOR_IGUAL:  return "MAYOR_IGUAL";

    case ASIGNA:       return "ASIGNA";
    case PUNTO:        return "PUNTO";
    case COMA:         return "COMA";
    case PUNTO_COMA:   return "PUNTO_COMA";
    case DOS_PUNTOS:   return "DOS_PUNTOS";
    case PAREN_ABRE:   return "PAREN_ABRE";
    case PAREN_CIERRA: return "PAREN_CIERRA";
    case LLAVE_ABRE:   return "LLAVE_ABRE";
    case LLAVE_CIERRA: return "LLAVE_CIERRA";

    default:           return "DESCONOCIDO";
    }
}
