/*
 * vm.c
 *
 * The implementation of the virtual machine
 *
 * Copyright (C) 2013-03-18 liutos <mat.liutos@gmail.com>
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "eval.h"
#include "object.h"
#include "types.h"
#include "write.h"

#define SR(x) if (S(#x) == name) return x
#define code_arg0(x) pair_cadr(code)
#define code_arg1(x) pair_caddr(code)
#define code_arg2(x) pair_cadddr(code)
#define push(e, stack) stack = make_pair(e, stack)
#define pop(stack) stack = pair_cdr(stack)
#define nth_pop(stack, n) stack = pair_nthcdr(stack, n)

enum code_type {
  CONST,
  LVAR,
  FJUMP,
  JUMP,
  LSET,
  POP,
  GVAR,
  CALL,
  FN,
  ARGS,
  RETURN,
  TJUMP,
};

enum code_type code_name(lisp_object_t code) {
  lisp_object_t name = pair_car(code);
  /* if (S("CONST") == name) */
  /*   return CONST; */
  SR(CONST);
  /* if (S("LVAR") == name) */
  /*   return LVAR; */
  SR(LVAR);
  /* if (S("FJUMP") == name) return FJUMP; */
  SR(FJUMP);
  /* if (S("JUMP") == name) return JUMP; */
  SR(JUMP);
  /* if (S("LSET") == name) return LSET; */
  SR(LSET);
  /* if (S("POP") == name) return POP; */
  SR(POP);
  /* if (S("GVAR") == name) return GVAR; */
  SR(GVAR);
  /* if (S("CALL") == name) return CALL; */
  SR(CALL);
  /* if (S("FN") == name) return FN; */
  SR(FN);
  /* if (S("ARGS") == name) return ARGS; */
  SR(ARGS);
  /* if (S("RETURN") == name) return RETURN; */
  SR(RETURN);
  fprintf(stderr, "code_name - Unsupported code: %s\n", symbol_name(pair_car(code)));
  exit(1);
}

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

void set_variable_by_index(int i, int j, lisp_object_t new_value, lisp_object_t environment) {
  while (i-- != 0)
    environment = enclosing_environment(environment);
  lisp_object_t vals = environment_vals(environment);
  while (j-- != 0)
    vals = pair_cdr(vals);
  pair_car(vals) = new_value;
}

lisp_object_t make_arguments(lisp_object_t stack, int n) {
  if (0 == n)
    return make_empty_list();
  else
    return make_pair(pair_car(stack),
                     make_arguments(pair_cdr(stack), n - 1));
}

void push_value2env(lisp_object_t stack, int n, lisp_object_t environment) {
  lisp_object_t vals = environment_vals(environment);
  for (; n > 0; n--) {
    lisp_object_t top = pair_car(stack);
    pop(stack);
    push(top, vals);
  }
  environment_vals(environment) = vals;
}

/* Assembler */
sexp extract_labels_aux(sexp compiled_code, int offset, int *length) {
  if (is_null(compiled_code)) {
    *length = offset;
    return EOL;
  } else {
    sexp first = pair_car(compiled_code);
    sexp rest = pair_cdr(compiled_code);
    if (is_label(first)) {
      sexp lo = make_pair(first, make_fixnum(offset));
      return make_pair(lo, extract_labels_aux(rest, offset, length));
    } else
      return extract_labels_aux(rest, offset + 1, length);
  }
}

/* Returns an a-list contains label-offset pairs. Parameter `length' stores the length of compiled code. */
sexp extract_labels(sexp compiled_code, int *length) {
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
  assert(is_pair(compiled_code));
  int length;
  lisp_object_t label_table = extract_labels(compiled_code, &length);
  return vectorize_code(compiled_code, length, label_table);
}

/* Virtual Machine */
void nth_insert_pair(int n, lisp_object_t object, lisp_object_t pair) {
  while (n - 1 != 0) {
    pair = pair_cdr(pair);
    n--;
  }
  lisp_object_t new_cdr = make_pair(object, pair_cdr(pair));
  pair_cdr(pair) = new_cdr;
}

/* Run the code generated from compiling an S-exp by function `assemble_code'. */
lisp_object_t run_compiled_code(lisp_object_t compiled_code, lisp_object_t environment, lisp_object_t stack) {
  assert(is_vector(compiled_code));
  int pc = 0;
  while (pc < vector_length(compiled_code)) {
    lisp_object_t code = vector_data_at(compiled_code, pc);
    printf("++ ");
    write_object(code, make_file_out_port(stdout));
    switch (code_name(code)) {
      case ARGS: {
        lisp_object_t n = code_arg0(code);
        push_value2env(stack, fixnum_value(n), environment);
        nth_pop(stack, fixnum_value(n));
        pc++;
      }
        break;
      case CALL: {
        lisp_object_t n = code_arg0(code);
        lisp_object_t op = pair_car(stack);
        pop(stack);
        if (is_compiled_proc(op)) {
          /* Save the current context */
          lisp_object_t info = make_return_info(compiled_code, pc, environment);
          /* push(info, stack); */
          if (fixnum_value(n) != 0)
            nth_insert_pair(fixnum_value(n), info, stack);
          else
            push(info, stack);
          /* Set the new context */
          lisp_object_t code = compiled_proc_code(op);
          lisp_object_t env = compiled_proc_env(op);
          code = assemble_code(code);
          /* stack = run_compiled_code(code, env, stack); */
          /* pc++; */
          compiled_code = code;
          environment = env;
          pc = 0;
        } else {
          lisp_object_t args = make_arguments(stack, fixnum_value(n));
          nth_pop(stack, fixnum_value(n));
          push(eval_application(op, args), stack);
          pc++;
        }
      }
        break;
      case CONST:
        push(code_arg0(code), stack);
        pc++;
        break;
      case FJUMP: {
        lisp_object_t top = pair_car(stack);
        pop(stack);
        if (is_false(top)) {
          pc = fixnum_value(code_arg0(code));
        } else
          pc++;
      }
        break;
      case FN:
        push(code_arg0(code), stack);
        pc++;
        break;
      case JUMP:
        pc = fixnum_value(code_arg0(code));
        break;
      case LSET: {
        int i = fixnum_value(code_arg0(code));
        int j = fixnum_value(code_arg1(code));
        lisp_object_t value = pair_car(stack);
        pop(stack);
        set_variable_by_index(i, j, value, environment);
        push(make_undefined(), stack);
      }
        pc++;
        break;
      case LVAR: {
        int i = fixnum_value(code_arg0(code));
        int j = fixnum_value(code_arg1(code));
        push(get_variable_by_index(i, j, environment), stack);
      }
        pc++;
        break;
      case POP:
        pop(stack);
        pc++;
        break;
      case RETURN: {
        lisp_object_t value = pair_car(stack);
        pop(stack);
        lisp_object_t info = pair_car(stack);
        pop(stack);
        /* Restore the last context */
        compiled_code = return_code(info);
        environment = return_env(info);
        pc = return_pc(info);
        push(value, stack);
        pc++;
      }
        break;
      case TJUMP: {
        lisp_object_t top = pair_car(stack);
        pop(stack);
        if (is_true(top)) {
          pc = fixnum_value(code_arg0(code));
        } else
          pc++;
      }
        break;
      default :
        fprintf(stderr, "run_compiled_code - Unknown code ");
        write_object(pair_car(code), make_file_out_port(stdout));
        /* exit(1); */
        return stack;
    }
    printf(" <> ");
    write_object(stack, make_file_out_port(stdout));
    putchar('\n');
  }
  return pair_car(stack);
}
