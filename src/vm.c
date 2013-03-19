/*
 * vm.c
 *
 *
 *
 * Copyright (C) 2013-03-18 liutos <mat.liutos@gmail.com>
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "types.h"
#include "object.h"

extern void write_object(lisp_object_t, lisp_object_t);

enum code_type {
  CONST,
  LVAR,
  FJUMP,
  JUMP,
  LSET,
};

enum code_type code_name(lisp_object_t code) {
#define S(name) find_or_create_symbol(name)
  lisp_object_t name = pair_car(code);
  if (S("CONST") == name)
    return CONST;
  if (S("LVAR") == name)
    return LVAR;
  if (S("FJUMP") == name) return FJUMP;
  if (S("JUMP") == name) return JUMP;
  if (S("LSET") == name) return LSET;
  fprintf(stderr, "Unsupported code: %s\n", symbol_name(pair_car(code)));
  exit(1);
}

/* lisp_object_t code_arg0(lisp_object_t code) { */
/*   return pair_cadr(code); */
/* } */
#define code_arg0(x) pair_cadr(code)
/* lisp_object_t code_arg1(lisp_object_t code) { */
/*   return pair_caddr(code); */
/* } */
#define code_arg1(x) pair_caddr(code)
/* lisp_object_t code_arg2(lisp_object_t code) { */
/*   return pair_cadddr(code); */
/* } */
#define code_arg2(x) pair_cadddr(code)

lisp_object_t get_variable_by_index(int i, int j, lisp_object_t environment) {
  while (i != 0) {
    environment = enclosing_environment(environment);
    i--;
  }
  lisp_object_t vals = environment_vals(environment);
  while (j != 0) {
    vals = pair_cdr(vals);
    j--;
  }
  return pair_car(vals);
}

/* Run the code generated from compiling an S-exp by function `assemble_code'. */
lisp_object_t run_compiled_code(lisp_object_t compiled_code, lisp_object_t environment, lisp_object_t stack) {
  assert(is_vector(compiled_code));
  /* if (is_compiled_proc(compiled_code)) */
  /*   compiled_code = compiled_proc_code(compiled_code); */
  int pc = 0;
  /* while (!is_null(compiled_code)) { */
  while (1) {
    lisp_object_t code = /* pair_car(compiled_code); */vector_data_at(compiled_code, pc);
    switch (code_name(code)) {
      case CONST: return code_arg0(code);
      case LVAR: {
        int i = fixnum_value(code_arg0(code));
        int j = fixnum_value(code_arg1(code));
        return get_variable_by_index(i, j, environment);
      }
      default :
        fprintf(stderr, "Unknown code\n");
        exit(1);
    }
    /* compiled_code = pair_cdr(compiled_code); */
    pc++;
  }
  return make_undefined();
}

int is_label(lisp_object_t code) {
  return is_symbol(code);
}

lisp_object_t extract_labels_aux(lisp_object_t compiled_code, int offset, int *length) {
  if (is_null(compiled_code)) {
    *length = offset;
    return make_empty_list();
  } else {
    lisp_object_t first = pair_car(compiled_code);
    lisp_object_t rest = pair_cdr(compiled_code);
    if (is_label(first)) {
      lisp_object_t lo = make_pair(first, make_fixnum(offset));
      return make_pair(lo, extract_labels_aux(rest, offset, length));
    } else
      return extract_labels_aux(rest, offset + 1, length);
  }
}

lisp_object_t extract_labels(lisp_object_t compiled_code, int *length) {
  return extract_labels_aux(compiled_code, 0, length);
}

int is_with_label(lisp_object_t code) {
  switch (code_name(code)) {
    case FJUMP:
    case JUMP: return 1;
    default : return 0;
  }
}

lisp_object_t search_label_offset(lisp_object_t label, lisp_object_t label_table) {
  if (is_null(label_table)) {
    fprintf(stderr, "Impossible - SEARCH_LABEL_OFFSET\n");
    exit(1);
  }
  lisp_object_t lo = pair_car(label_table);
  if (label == pair_car(lo))
    return pair_cdr(lo);
  else
    return search_label_offset(label, pair_cdr(label_table));
}

/* Convert the byte code stored as a list in COMPILED_PROC into a vector filled of the same code, except the label in instructions with label will be replace by an integer offset. */
lisp_object_t vectorize_code(lisp_object_t compiled_code, int length, lisp_object_t label_table) {
  lisp_object_t code_vector = make_vector(length);
  int i = 0;
  while (is_pair(compiled_code)) {
    lisp_object_t code = pair_car(compiled_code);
    if (!is_label(code)) {
      if (is_with_label(code)) {
        code_arg0(code) = search_label_offset(code_arg0(code), label_table);
        label_table = pair_cdr(label_table);
      }
      vector_data_at(code_vector, i) = code;
      i++;
    }
    compiled_code = pair_cdr(compiled_code);
  }
  return code_vector;
}

lisp_object_t assemble_code(lisp_object_t compiled_code) {
  int length;
  lisp_object_t label_table = extract_labels(compiled_code, &length);
  return vectorize_code(compiled_code, length, label_table);
}
