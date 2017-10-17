//
// Created by alex on 17/10/2017.
//

/* ED4: stack.c */

#include <stdio.h>
#include <stdlib.h>
#include "stack.h"

void stack_init(STACK *p) {
    *p = NULL;
}

STACK stack_push(STACK *stack, STACK_RECORD stack_record) {
    STACK queue;
    queue = (STACK) malloc(sizeof(STACK_RECORD));
    *queue = stack_record;
    queue->next = *stack;
    *stack = queue;
    return queue;
}

STACK_RECORD *stack_next(STACK *stack) {
    return *stack;
}

STACK_RECORD stack_pop(STACK *stack) {
    STACK_RECORD stack_record;
    STACK queue;

    stack_record = **stack;
    queue = *stack;
    *stack = (*stack)->next;
    free(queue);
    return stack_record;
}

int stack_empty(STACK *stack) {
    return *stack == NULL;
}