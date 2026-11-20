/* Mini substituto de json-c: so o subconjunto usado por mypaint-brush.c (sem dependencias externas). */
#ifndef CP_MINI_JSON_H
#define CP_MINI_JSON_H
#ifdef __cplusplus
extern "C" {
#endif

#define JSON_C_MINOR_VERSION 13

typedef enum { json_type_null, json_type_boolean, json_type_double, json_type_int, json_type_object,
               json_type_array, json_type_string } json_type;

typedef struct json_object {
    json_type type;
    double num;
    char *str;
    int n;
    int cap;
    char **keys;               /* objetos */
    struct json_object **vals; /* objetos e arrays */
} json_object;

json_object *json_tokener_parse(const char *s);
json_object *json_object_new_object(void);
void json_object_put(json_object *o);
int json_object_object_get_ex(json_object *o, const char *key, json_object **out);
int json_object_is_type(json_object *o, json_type t);
double json_object_get_double(json_object *o);
int json_object_get_int(json_object *o);
int json_object_array_length(json_object *o);
json_object *json_object_array_get_idx(json_object *o, int i);

#define json_object_object_foreach(obj, key, val)                                                    \
    char *key = NULL;                                                                                \
    json_object *val = NULL;                                                                         \
    for (int key##_i = 0; (obj) && key##_i < (obj)->n && ((key = (obj)->keys[key##_i]), (val = (obj)->vals[key##_i]), 1); \
         ++key##_i)

#ifdef __cplusplus
}
#endif
#endif
