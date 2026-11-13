#include <stdio.h>
#include <string.h>
#include <math.h>
#include "semantico.h"
#include "simbolos.h"
#include "builtins.h"
#include "evaluador.h"
#include "errores.h"

/* semantico.c - Primer recorrido del analisis semantico
 *
 * Nombres, tipos y forma de las llamadas (libro 2.8.3). El tipo se
 * sintetiza de las hojas a la raiz (6.5.1), sin conversiones implicitas.
 * DATO_ERROR evita repetir el mismo error. */

static Alcance *predefinido;      /* los builtins                        */
static Alcance *global;           /* maquina, constantes, procedimientos */
static Simbolo *maquina;          /* la unica maquina del programa       */
static Simbolo *proc_actual;      /* el procedimiento que se analiza     */
static Nodo    *raiz;

static const char *const campos_obligatorios[] = {
    CAMPO_LIMITE_X, CAMPO_LIMITE_Y, CAMPO_LIMITE_Z, CAMPO_FEEDRATE_MAX, NULL
};

static void     analizar_lista(Nodo *s, Alcance *a);
static TipoDato tipo_expr(Nodo *e, Alcance *a);

/* --- Utilidades --- */

#define ERROR_EN(n, ...)  error_semantico((n)->linea, (n)->columna, __VA_ARGS__)

static const char *plural(int n, const char *uno, const char *varios)
{
    return n == 1 ? uno : varios;
}

static int largo_lista(const Nodo *n)
{
    int k = 0;
    for (; n; n = n->sig)
        k++;
    return k;
}

const char *problema_repeticiones(double v)
{
    if (v < 0)
        return "no puede ser negativa";
    if (v != floor(v))
        return "tiene que ser un numero entero";
    return NULL;
}

/* Nombre de una declaracion global, sea del tipo que sea. */
static const char *nombre_decl(const Nodo *d)
{
    switch (d->tipo) {
    case N_MAQUINA:       return d->maquina.nombre;
    case N_CONST:         return d->decl.nombre;
    case N_PROCEDIMIENTO: return d->proc.nombre;
    default:              return NULL;
    }
}

/* no declarado, o declarado mas abajo */
static void reportar_no_declarado(const char *nombre, const Nodo *uso)
{
    const Nodo *d;

    for (d = raiz->programa.declaraciones; d; d = d->sig) {
        const char *n = nombre_decl(d);
        if (n && strcmp(n, nombre) == 0 && d->linea > uso->linea) {
            ERROR_EN(uso, "'%s' se usa antes de declararse (se declara en la "
                     "linea %d): todo nombre se declara antes de usarlo",
                     nombre, d->linea);
            return;
        }
    }
    ERROR_EN(uso, "'%s' no esta declarado", nombre);
}

/* chequea builtin y redeclaracion; ocultar uno de afuera vale */
static Simbolo *declarar(Alcance *a, const char *nombre, ClaseSimbolo clase,
                         TipoDato tipo, Nodo *decl)
{
    Simbolo *previo;

    if (alcance_buscar_local(predefinido, nombre)) {
        ERROR_EN(decl, "'%s' es un comando predefinido y no se puede usar "
                 "como nombre", nombre);
        return NULL;
    }
    previo = alcance_buscar_local(a, nombre);
    if (previo) {
        ERROR_EN(decl, "'%s' ya esta declarado en este alcance (linea %d)",
                 nombre, previo->decl->linea);
        return NULL;
    }
    return alcance_insertar(a, nombre, clase, tipo, decl);
}

/* division por cero entre constantes */
static void revisar_division(const Nodo *e)
{
    const Nodo *culpable;
    double v;

    if (evaluar(e, &v, &culpable) == EVAL_DIV_CERO)
        ERROR_EN(culpable, "division o resto por cero");
}

/* tipo de e contra el esperado; contexto es para el mensaje */
static TipoDato esperar_tipo(Nodo *e, Alcance *a, TipoDato esperado,
                             const char *contexto)
{
    TipoDato t = tipo_expr(e, a);

    if (t != DATO_ERROR && esperado != DATO_ERROR && t != esperado)
        ERROR_EN(e, "%s tiene que ser %s y es %s", contexto,
                 ast_nombre_tipo(esperado), ast_nombre_tipo(t));
    if (t != DATO_ERROR)
        revisar_division(e);
    return t;
}

/* valor de una const o un campo */
static void fijar_valor(Simbolo *s, const Nodo *valor)
{
    const Nodo *culpable;
    double v;

    if (evaluar(valor, &v, &culpable) == EVAL_OK) {
        s->conocido = 1;
        s->valor    = v;
    }
}

/* primer uso de una variable en e, o NULL */
static const Nodo *primera_variable(const Nodo *e)
{
    const Nodo *v;

    switch (e->tipo) {
    case N_ID:
        return e->simbolo && e->simbolo->clase == S_VAR ? e : NULL;
    case N_BINARIA:
        v = primera_variable(e->binaria.izq);
        return v ? v : primera_variable(e->binaria.der);
    case N_UNARIA:
        return primera_variable(e->unaria.operando);
    default:
        return NULL;
    }
}

/* --- Expresiones: tipo sintetizado --- */

static TipoDato tipo_id(Nodo *e, Alcance *a)
{
    Simbolo *s = alcance_buscar(a, e->id);

    if (!s) {
        reportar_no_declarado(e->id, e);
        return DATO_ERROR;
    }
    if (s->clase == S_PROC || s->clase == S_BUILTIN) {
        ERROR_EN(e, "'%s' es un procedimiento y no tiene valor", e->id);
        return DATO_ERROR;
    }
    e->simbolo = s;
    return s->tipo;
}

/* m.campo: hay una sola maquina, se verifica aca */
static TipoDato tipo_acceso(Nodo *e, Alcance *a)
{
    Simbolo *s = alcance_buscar(a, e->acceso.objeto);
    Simbolo *m, *campo;

    if (!s) {
        reportar_no_declarado(e->acceso.objeto, e);
        return DATO_ERROR;
    }
    if (s->clase == S_MAQUINA) {
        m = s;
    } else if (s->clase == S_PARAM && s->tipo == DATO_MACHINE) {
        m = maquina;
    } else {
        ERROR_EN(e, "'%s' no es una maquina: el punto solo se usa para leer "
                 "un campo de la maquina", e->acceso.objeto);
        return DATO_ERROR;
    }
    if (!m)
        return DATO_ERROR;   /* no hay maquina: ya se reporto */

    campo = alcance_buscar_local(m->campos, e->acceso.campo);
    if (!campo) {
        ERROR_EN(e, "la maquina '%s' no tiene el campo '%s'",
                 m->nombre, e->acceso.campo);
        return DATO_ERROR;
    }
    e->simbolo = campo;
    return campo->tipo;
}

static TipoDato tipo_binaria(Nodo *e, Alcance *a)
{
    TipoDato ti = tipo_expr(e->binaria.izq, a);
    TipoDato td = tipo_expr(e->binaria.der, a);
    const char *op = ast_nombre_operador(e->binaria.op);

    if (ti == DATO_ERROR || td == DATO_ERROR)
        return DATO_ERROR;

    switch (e->binaria.op) {
    case OP_SUMA: case OP_RESTA: case OP_MULT: case OP_DIV: case OP_MOD:
        if (ti == DATO_NUMBER && td == DATO_NUMBER)
            return DATO_NUMBER;
        ERROR_EN(e, "el operador '%s' se aplica a dos number, y aca recibe "
                 "%s y %s", op, ast_nombre_tipo(ti), ast_nombre_tipo(td));
        return DATO_ERROR;

    case OP_MENOR: case OP_MAYOR: case OP_MENOR_IGUAL: case OP_MAYOR_IGUAL:
        if (ti == DATO_NUMBER && td == DATO_NUMBER)
            return DATO_BOOL;
        ERROR_EN(e, "el operador '%s' compara dos number, y aca recibe "
                 "%s y %s", op, ast_nombre_tipo(ti), ast_nombre_tipo(td));
        return DATO_ERROR;

    case OP_IGUAL: case OP_DISTINTO:
        if (ti == td && ti != DATO_MACHINE)
            return DATO_BOOL;
        ERROR_EN(e, "el operador '%s' compara dos number o dos bool, y aca "
                 "recibe %s y %s", op, ast_nombre_tipo(ti), ast_nombre_tipo(td));
        return DATO_ERROR;

    case OP_AND: case OP_OR:
        if (ti == DATO_BOOL && td == DATO_BOOL)
            return DATO_BOOL;
        ERROR_EN(e, "el operador '%s' se aplica a dos bool, y aca recibe "
                 "%s y %s", op, ast_nombre_tipo(ti), ast_nombre_tipo(td));
        return DATO_ERROR;

    case OP_NOT: case OP_NEG:
        break;   /* unarios, no llegan aca */
    }
    return DATO_ERROR;
}

static TipoDato tipo_unaria(Nodo *e, Alcance *a)
{
    TipoDato t = tipo_expr(e->unaria.operando, a);
    TipoDato pide = e->unaria.op == OP_NOT ? DATO_BOOL : DATO_NUMBER;

    if (t == DATO_ERROR)
        return DATO_ERROR;
    if (t != pide) {
        ERROR_EN(e, "el operador '%s' se aplica a %s, y aca recibe %s",
                 ast_nombre_operador(e->unaria.op), ast_nombre_tipo(pide),
                 ast_nombre_tipo(t));
        return DATO_ERROR;
    }
    return t;
}

static TipoDato tipo_expr(Nodo *e, Alcance *a)
{
    switch (e->tipo) {
    case N_NUMERO:  return DATO_NUMBER;
    case N_BOOL:    return DATO_BOOL;
    case N_ID:      return tipo_id(e, a);
    case N_ACCESO:  return tipo_acceso(e, a);
    case N_BINARIA: return tipo_binaria(e, a);
    case N_UNARIA:  return tipo_unaria(e, a);
    default:        return DATO_ERROR;
    }
}

/* --- Llamadas --- */

/* solo los argumentos, cuando lo llamado no existe o no es procedimiento */
static void analizar_args_sueltos(Nodo *arg, Alcance *a)
{
    for (; arg; arg = arg->sig) {
        if (arg->tipo == N_ARG_EJE) {
            if (arg->arg_eje.valor)
                tipo_expr(arg->arg_eje.valor, a);
        } else if (arg->tipo != N_CADENA) {
            tipo_expr(arg, a);
        }
    }
}

/* valida recorriendo la fila del builtin */
static void validar_builtin(Nodo *s, const Builtin *b, Alcance *a)
{
    int presente[MAX_PARAMS_BUILTIN] = { 0 };
    int n_pos = builtin_n_posicionales(b);
    int pos = 0;
    int i;
    char ctx[96];
    Nodo *arg;

    for (arg = s->llamada.args; arg; arg = arg->sig) {
        if (arg->tipo == N_ARG_EJE) {
            char eje = arg->arg_eje.eje;
            Nodo *valor = arg->arg_eje.valor;
            const ParamBuiltin *pb = builtin_param_eje(b, eje);

            if (!pb) {
                if (b->pista)
                    ERROR_EN(arg, "'%s' no admite el parametro %c (%s)",
                             b->nombre, eje, b->pista);
                else
                    ERROR_EN(arg, "'%s' no admite el parametro %c",
                             b->nombre, eje);
            } else if (presente[pb - b->params]) {
                ERROR_EN(arg, "el eje %c esta repetido en la llamada a '%s'",
                         eje, b->nombre);
            } else {
                presente[pb - b->params] = 1;
                if (pb->clase == P_EJE_SOLO && valor) {
                    ERROR_EN(arg, "'%s' recibe los ejes sin valor, por ejemplo "
                             "%s(%c)", b->nombre, b->nombre, eje);
                } else if (pb->clase == P_NOMBRADO && !valor) {
                    ERROR_EN(arg, "el parametro %c de '%s' necesita un valor "
                             "(%c: ...); los ejes sin valor son solo para "
                             "AutoHome", eje, b->nombre, eje);
                } else if (valor) {
                    snprintf(ctx, sizeof ctx, "%c: en '%s'", eje, b->nombre);
                    esperar_tipo(valor, a, pb->tipo, ctx);
                    valor = NULL;   /* ya analizado */
                }
            }
            if (valor)
                tipo_expr(valor, a);
            continue;
        }

        /* Argumento posicional: una expresion o una cadena. */
        {
            const ParamBuiltin *pb = builtin_posicional(b, pos++);

            if (!pb) {
                if (arg->tipo != N_CADENA)
                    tipo_expr(arg, a);
                continue;   /* sobra: lo reporta el chequeo de aridad */
            }
            presente[pb - b->params] = 1;

            if (pb->clase == P_CADENA) {
                if (arg->tipo != N_CADENA) {
                    ERROR_EN(arg, "'%s' recibe un texto entre comillas",
                             b->nombre);
                    tipo_expr(arg, a);
                } else if (b->texto_sin_comentario && strchr(arg->cadena, ';')) {
                    ERROR_EN(arg, "el texto de '%s' no puede tener ';': en "
                             "G-code empieza un comentario y el resto del "
                             "texto se perderia", b->nombre);
                }
            } else if (arg->tipo == N_CADENA) {
                ERROR_EN(arg, "el argumento %d de '%s' tiene que ser %s, no un "
                         "texto", pos, b->nombre, ast_nombre_tipo(pb->tipo));
            } else {
                snprintf(ctx, sizeof ctx, "el argumento %d de '%s'",
                         pos, b->nombre);
                esperar_tipo(arg, a, pb->tipo, ctx);
            }
        }
    }

    /* Aridad de los posicionales. */
    if (pos != n_pos) {
        if (n_pos == 0)
            ERROR_EN(s, "'%s' no recibe argumentos sin nombre", b->nombre);
        else
            ERROR_EN(s, "'%s' recibe %d %s y se le %s %d", b->nombre, n_pos,
                     plural(n_pos, "argumento", "argumentos"),
                     plural(pos, "paso", "pasaron"), pos);
    }

    /* Parametros nombrados obligatorios (el E de Draw y de Extrude). */
    for (i = 0; i < b->n_params; i++)
        if (b->params[i].clase == P_NOMBRADO && b->params[i].obligatorio &&
            !presente[i])
            ERROR_EN(s, "'%s' necesita el parametro %c:", b->nombre,
                     b->params[i].eje);

    /* Al menos un eje de posicion (Move y Draw). */
    if (b->requiere_posicion) {
        int alguno = 0;
        for (i = 0; i < b->n_params; i++)
            if (presente[i] && strchr("XYZ", b->params[i].eje) &&
                b->params[i].eje != 0)
                alguno = 1;
        if (!alguno)
            ERROR_EN(s, "'%s' necesita al menos un eje de posicion (X, Y o Z)",
                     b->nombre);
    }
}

/* procedimiento: solo posicionales, cantidad y tipos */
static void validar_args_proc(Nodo *s, Simbolo *p, Alcance *a)
{
    Nodo *param = p->decl->proc.params;
    Nodo *arg;
    int n_params = largo_lista(param);
    int n_args   = largo_lista(s->llamada.args);
    int i = 1;
    char ctx[96];

    for (arg = s->llamada.args; arg; arg = arg->sig, i++) {
        if (arg->tipo == N_ARG_EJE) {
            ERROR_EN(arg, "los procedimientos reciben solo argumentos sin "
                     "nombre: '%c:' no corresponde en la llamada a '%s'",
                     arg->arg_eje.eje, p->nombre);
            if (arg->arg_eje.valor)
                tipo_expr(arg->arg_eje.valor, a);
        } else if (arg->tipo == N_CADENA) {
            ERROR_EN(arg, "los procedimientos no reciben textos entre comillas");
        } else if (param) {
            snprintf(ctx, sizeof ctx, "el argumento %d de '%s'", i, p->nombre);
            esperar_tipo(arg, a, param->param.tipo, ctx);
        } else {
            tipo_expr(arg, a);
        }
        if (param)
            param = param->sig;
    }

    if (n_args != n_params)
        ERROR_EN(s, "'%s' recibe %d %s y se le %s %d", p->nombre, n_params,
                 plural(n_params, "argumento", "argumentos"),
                 plural(n_args, "paso", "pasaron"), n_args);
}

static void analizar_llamada(Nodo *s, Alcance *a)
{
    Simbolo *p = alcance_buscar(a, s->llamada.nombre);

    if (!p) {
        reportar_no_declarado(s->llamada.nombre, s);
        analizar_args_sueltos(s->llamada.args, a);
        return;
    }
    s->simbolo = p;

    switch (p->clase) {
    case S_BUILTIN:
        validar_builtin(s, p->builtin, a);
        break;
    case S_PROC:
        /* solo puede ser recursion directa */
        if (p == proc_actual)
            ERROR_EN(s, "'%s' se llama a si mismo: la recursion no esta "
                     "permitida, porque cada llamada se expande en linea",
                     p->nombre);
        validar_args_proc(s, p, a);
        break;
    default:
        ERROR_EN(s, "'%s' no es un procedimiento", s->llamada.nombre);
        analizar_args_sueltos(s->llamada.args, a);
        break;
    }
}

/* --- Sentencias --- */

/* Un bloque { } abre un alcance nuevo dentro del actual. */
static void analizar_bloque(Nodo *bloque, Alcance *a, const char *descripcion)
{
    Alcance *interno = alcance_abrir(a, bloque->linea, "%s", descripcion);
    analizar_lista(bloque->bloque.sentencias, interno);
}

static void analizar_var(Nodo *s, Alcance *a)
{
    TipoDato t = s->decl.tipo;
    char ctx[96];

    if (t == DATO_MACHINE) {
        ERROR_EN(s, "'%s' no puede ser de tipo machine: las maquinas solo se "
                 "reciben como parametro", s->decl.nombre);
        tipo_expr(s->decl.valor, a);
        t = DATO_ERROR;
    } else {
        snprintf(ctx, sizeof ctx, "el valor inicial de '%s'", s->decl.nombre);
        esperar_tipo(s->decl.valor, a, t, ctx);
    }

    /* entra despues del valor: en var a = a + 1, la a es la de afuera */
    declarar(a, s->decl.nombre, S_VAR, t, s);
}

static void analizar_asignacion(Nodo *s, Alcance *a)
{
    Simbolo *v = alcance_buscar(a, s->asignacion.nombre);
    char ctx[96];

    if (!v) {
        reportar_no_declarado(s->asignacion.nombre, s);
        tipo_expr(s->asignacion.valor, a);
        return;
    }
    s->simbolo = v;

    /* Solo las variables son asignables (el chequeo de l-valores, 2.8.3). */
    switch (v->clase) {
    case S_VAR:
        snprintf(ctx, sizeof ctx, "el valor asignado a '%s'", v->nombre);
        esperar_tipo(s->asignacion.valor, a, v->tipo, ctx);
        return;
    case S_CONST:
        ERROR_EN(s, "'%s' es una constante y no se le puede asignar un valor",
                 v->nombre);
        break;
    case S_INDICE:
        ERROR_EN(s, "'%s' es el indice de un repeat y es de solo lectura",
                 v->nombre);
        break;
    case S_PARAM:
        ERROR_EN(s, "'%s' es un parametro y es de solo lectura", v->nombre);
        break;
    default:
        ERROR_EN(s, "'%s' no es una variable", v->nombre);
        break;
    }
    tipo_expr(s->asignacion.valor, a);
}

static void analizar_if(Nodo *s, Alcance *a)
{
    esperar_tipo(s->si.cond, a, DATO_BOOL, "la condicion del if");
    analizar_bloque(s->si.entonces, a, "bloque del if");
    if (s->si.sino) {
        if (s->si.sino->tipo == N_IF)
            analizar_if(s->si.sino, a);   /* else if */
        else
            analizar_bloque(s->si.sino, a, "bloque del else");
    }
}

static void analizar_repeat(Nodo *s, Alcance *a)
{
    Nodo *veces = s->repetir.veces;
    TipoDato t = esperar_tipo(veces, a, DATO_NUMBER,
                              "la cantidad de repeticiones");
    Alcance *interno;

    /* sin variables; parametros e indices se ven en el segundo recorrido */
    if (t == DATO_NUMBER) {
        const Nodo *var = primera_variable(veces);
        const Nodo *culpable;
        const char *problema;
        double v;

        if (var) {
            ERROR_EN(var, "la cantidad de repeticiones tiene que ser constante, "
                     "y '%s' es una variable", var->id);
        } else {
            switch (evaluar(veces, &v, &culpable)) {
            case EVAL_OK:
                problema = problema_repeticiones(v);
                if (problema)
                    ERROR_EN(veces, "la cantidad de repeticiones %s, y vale %g",
                             problema, v);
                break;
            case EVAL_DIV_CERO:       /* ya la reporto esperar_tipo */
            case EVAL_NO_CONSTANTE:
                break;
            }
        }
    }

    /* indice y cuerpo comparten alcance */
    interno = alcance_abrir(a, s->linea, "repeat");
    if (s->repetir.indice)
        declarar(interno, s->repetir.indice, S_INDICE, DATO_NUMBER, s);
    analizar_lista(s->repetir.cuerpo->bloque.sentencias, interno);
}

static void analizar_sentencia(Nodo *s, Alcance *a)
{
    switch (s->tipo) {
    case N_BLOQUE:      analizar_bloque(s, a, "bloque"); break;
    case N_VAR:         analizar_var(s, a);              break;
    case N_ASIGNACION:  analizar_asignacion(s, a);       break;
    case N_IF:          analizar_if(s, a);               break;
    case N_REPEAT:      analizar_repeat(s, a);           break;
    case N_LLAMADA:     analizar_llamada(s, a);          break;
    default:            break;
    }
}

static void analizar_lista(Nodo *s, Alcance *a)
{
    for (; s; s = s->sig)
        analizar_sentencia(s, a);
}

/* --- Declaraciones globales --- */

static void analizar_maquina(Nodo *m, int es_primera)
{
    Simbolo *s;
    Nodo *c;
    int i;

    /* Un programa se compila para una impresora concreta. */
    if (maquina)
        ERROR_EN(m, "solo se puede declarar una maquina por programa ('%s' "
                 "ya esta declarada en la linea %d)",
                 maquina->nombre, maquina->decl->linea);
    else if (!es_primera)
        ERROR_EN(m, "la maquina tiene que ser la primera declaracion del "
                 "programa");

    s = declarar(global, m->maquina.nombre, S_MAQUINA, DATO_MACHINE, m);
    if (!s)
        return;
    /* fuera de la cadena: solo con el punto */
    s->campos = alcance_abrir(NULL, m->linea, "campos de %s", m->maquina.nombre);
    s->campos->nivel = global->nivel + 1;
    if (!maquina)
        maquina = s;

    for (c = m->maquina.campos; c; c = c->sig) {
        TipoDato t = tipo_expr(c->campo.valor, global);
        Simbolo *f;

        if (t != DATO_ERROR && t != DATO_NUMBER) {
            ERROR_EN(c->campo.valor, "el campo '%s' tiene que ser number y es "
                     "%s", c->campo.nombre, ast_nombre_tipo(t));
            t = DATO_ERROR;
        }
        if (t != DATO_ERROR)
            revisar_division(c->campo.valor);
        f = declarar(s->campos, c->campo.nombre, S_CAMPO, t, c);
        if (f && t == DATO_NUMBER)
            fijar_valor(f, c->campo.valor);
    }

    for (i = 0; campos_obligatorios[i]; i++) {
        Simbolo *f = alcance_buscar_local(s->campos, campos_obligatorios[i]);
        if (!f)
            ERROR_EN(m, "a la maquina '%s' le falta el campo obligatorio '%s'",
                     m->maquina.nombre, campos_obligatorios[i]);
        else if (f->conocido && f->valor <= 0)
            ERROR_EN(f->decl, "el campo '%s' tiene que ser mayor que 0, y vale "
                     "%g", f->nombre, f->valor);
    }
}

static void analizar_const(Nodo *c)
{
    TipoDato t = c->decl.tipo;
    TipoDato calculado;
    Simbolo *s;
    char ctx[96];

    if (t == DATO_MACHINE) {
        ERROR_EN(c, "la constante '%s' no puede ser de tipo machine",
                 c->decl.nombre);
        tipo_expr(c->decl.valor, global);
        t = DATO_ERROR;
        calculado = DATO_ERROR;
    } else {
        snprintf(ctx, sizeof ctx, "el valor de la constante '%s'",
                 c->decl.nombre);
        calculado = esperar_tipo(c->decl.valor, global, t, ctx);
    }

    s = declarar(global, c->decl.nombre, S_CONST, t, c);
    if (s && calculado == t)
        fijar_valor(s, c->decl.valor);
}

static void analizar_procedimiento(Nodo *p)
{
    Alcance *alcance;
    Nodo *param;

    /* el nombre entra antes del cuerpo, para detectar la recursion */
    proc_actual = declarar(global, p->proc.nombre, S_PROC, DATO_ERROR, p);

    /* parametros y cuerpo comparten alcance */
    alcance = alcance_abrir(global, p->linea, "procedure %s", p->proc.nombre);
    for (param = p->proc.params; param; param = param->sig)
        declarar(alcance, param->param.nombre, S_PARAM, param->param.tipo,
                 param);

    analizar_lista(p->proc.cuerpo->bloque.sentencias, alcance);
    proc_actual = NULL;
}

void analizar_semantica(Nodo *programa)
{
    Nodo *d;
    Nodo *principal = programa->programa.principal;
    Alcance *alcance_main;
    int i, hay_maquina = 0;

    raiz = programa;
    maquina = NULL;
    proc_actual = NULL;

    /* predefinido: los builtins, el alcance mas externo */
    predefinido = alcance_abrir(NULL, 0, "predefinido");
    for (i = 0; i < n_builtins; i++) {
        Simbolo *s = alcance_insertar(predefinido, builtins[i].nombre,
                                      S_BUILTIN, DATO_ERROR, NULL);
        s->builtin = &builtins[i];
    }
    global = alcance_abrir(predefinido, 0, "global");

    for (d = programa->programa.declaraciones; d; d = d->sig)
        if (d->tipo == N_MAQUINA)
            hay_maquina = 1;
    if (!hay_maquina) {
        Nodo *donde = programa->programa.declaraciones;
        if (!donde)
            donde = principal;
        ERROR_EN(donde, "falta la declaracion de la maquina: todo programa "
                 "empieza declarando la impresora para la que se compila");
    }

    /* en orden del fuente */
    for (d = programa->programa.declaraciones; d; d = d->sig) {
        switch (d->tipo) {
        case N_MAQUINA:
            analizar_maquina(d, d == programa->programa.declaraciones);
            break;
        case N_CONST:
            analizar_const(d);
            break;
        case N_PROCEDIMIENTO:
            analizar_procedimiento(d);
            break;
        default:
            break;
        }
    }

    /* main: alcance dentro del global */
    alcance_main = alcance_abrir(global, principal->linea, "main");
    analizar_lista(principal->bloque.sentencias, alcance_main);
}
