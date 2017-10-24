//
// Created by alex on 17/10/2017.
//

#ifndef PUBLIQUE2_7BETA_PYTHON_STACK_H
#define PUBLIQUE2_7BETA_PYTHON_STACK_H
/* ED4: stack.h */


typedef struct stackrecord STACK_RECORD;

struct stackrecord {
    STACK_RECORD *next;
};

typedef STACK_RECORD *STACK;

void stack_init(STACK *p);

STACK_RECORD stack_pop(STACK *stack);

STACK stack_push(STACK *stack, STACK_RECORD stack_record);

STACK_RECORD *stack_next(STACK *stack);

int stack_empty(STACK *stack);

#endif //PUBLIQUE2_7BETA_PYTHON_STACK_H
