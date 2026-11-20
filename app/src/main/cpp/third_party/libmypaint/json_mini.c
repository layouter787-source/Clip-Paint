/* Mini parser JSON (RFC 8259) compativel com o subconjunto de json-c usado pela libmypaint. */
#include "json.h"

#include <stdlib.h>
#include <string.h>

static char *dupstr(const char *s) {
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

static json_object *node(json_type t) {
    json_object *o = (json_object *)calloc(1, sizeof(json_object));
    if (o) o->type = t;
    return o;
}

static int push(json_object *o, const char *key, json_object *v) {
    if (o->n == o->cap) {
        int nc = o->cap ? o->cap * 2 : 8;
        json_object **nv = (json_object **)realloc(o->vals, (size_t)nc * sizeof(*nv));
        if (!nv) return 0;
        o->vals = nv;
        if (o->type == json_type_object) {
            char **nk = (char **)realloc(o->keys, (size_t)nc * sizeof(*nk));
            if (!nk) return 0;
            o->keys = nk;
        }
        o->cap = nc;
    }
    if (o->type == json_type_object) o->keys[o->n] = key ? dupstr(key) : dupstr("");
    o->vals[o->n++] = v;
    return 1;
}

void json_object_put(json_object *o) {
    if (!o) return;
    for (int i = 0; i < o->n; ++i) {
        if (o->keys) free(o->keys[i]);
        json_object_put(o->vals[i]);
    }
    free(o->keys);
    free(o->vals);
    free(o->str);
    free(o);
}

json_object *json_object_new_object(void) { return node(json_type_object); }

typedef struct { const char *p; } P;

static void ws(P *s) { while (*s->p == ' ' || *s->p == '\n' || *s->p == '\r' || *s->p == '\t') s->p++; }

static char *parse_string(P *s) {
    if (*s->p != '"') return NULL;
    s->p++;
    size_t cap = 32, len = 0;
    char *out = (char *)malloc(cap);
    while (*s->p && *s->p != '"') {
        char c = *s->p++;
        if (c == '\\' && *s->p) {
            char e = *s->p++;
            switch (e) {
                case 'n': c = '\n'; break;
                case 't': c = '\t'; break;
                case 'r': c = '\r'; break;
                case 'b': c = '\b'; break;
                case 'f': c = '\f'; break;
                case 'u': c = '?'; for (int k = 0; k < 4 && *s->p; ++k) s->p++; break;
                default: c = e; break;
            }
        }
        if (len + 2 > cap) { cap *= 2; out = (char *)realloc(out, cap); }
        out[len++] = c;
    }
    if (*s->p != '"') { free(out); return NULL; }
    s->p++;
    out[len] = 0;
    return out;
}

static json_object *parse_value(P *s, int depth) {
    if (depth > 32) return NULL;
    ws(s);
    char c = *s->p;
    if (c == '{') {
        json_object *o = node(json_type_object);
        s->p++;
        ws(s);
        if (*s->p == '}') { s->p++; return o; }
        for (;;) {
            ws(s);
            char *k = parse_string(s);
            if (!k) { json_object_put(o); return NULL; }
            ws(s);
            if (*s->p != ':') { free(k); json_object_put(o); return NULL; }
            s->p++;
            json_object *v = parse_value(s, depth + 1);
            if (!v) { free(k); json_object_put(o); return NULL; }
            push(o, k, v);
            free(k);
            ws(s);
            if (*s->p == ',') { s->p++; continue; }
            if (*s->p == '}') { s->p++; return o; }
            json_object_put(o);
            return NULL;
        }
    }
    if (c == '[') {
        json_object *o = node(json_type_array);
        s->p++;
        ws(s);
        if (*s->p == ']') { s->p++; return o; }
        for (;;) {
            json_object *v = parse_value(s, depth + 1);
            if (!v) { json_object_put(o); return NULL; }
            push(o, NULL, v);
            ws(s);
            if (*s->p == ',') { s->p++; continue; }
            if (*s->p == ']') { s->p++; return o; }
            json_object_put(o);
            return NULL;
        }
    }
    if (c == '"') {
        json_object *o = node(json_type_string);
        o->str = parse_string(s);
        if (!o->str) { json_object_put(o); return NULL; }
        return o;
    }
    if (strncmp(s->p, "true", 4) == 0) { s->p += 4; json_object *o = node(json_type_boolean); o->num = 1; return o; }
    if (strncmp(s->p, "false", 5) == 0) { s->p += 5; return node(json_type_boolean); }
    if (strncmp(s->p, "null", 4) == 0) { s->p += 4; return node(json_type_null); }
    char *end = NULL;
    double d = strtod(s->p, &end);
    if (end == s->p) return NULL;
    s->p = end;
    json_object *o = node(json_type_double);
    o->num = d;
    return o;
}

json_object *json_tokener_parse(const char *str) {
    if (!str) return NULL;
    P s = {str};
    json_object *o = parse_value(&s, 0);
    return o;
}

int json_object_object_get_ex(json_object *o, const char *key, json_object **out) {
    if (!o || o->type != json_type_object) return 0;
    for (int i = 0; i < o->n; ++i) {
        if (strcmp(o->keys[i], key) == 0) { if (out) *out = o->vals[i]; return 1; }
    }
    return 0;
}

int json_object_is_type(json_object *o, json_type t) {
    if (!o) return t == json_type_null;
    if (t == json_type_int || t == json_type_double) return o->type == json_type_double || o->type == json_type_int;
    return o->type == t;
}

double json_object_get_double(json_object *o) { return (o && (o->type == json_type_double || o->type == json_type_boolean)) ? o->num : 0.0; }
int json_object_get_int(json_object *o) { return (int)json_object_get_double(o); }
int json_object_array_length(json_object *o) { return (o && o->type == json_type_array) ? o->n : 0; }
json_object *json_object_array_get_idx(json_object *o, int i) {
    return (o && o->type == json_type_array && i >= 0 && i < o->n) ? o->vals[i] : NULL;
}
