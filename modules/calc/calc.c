// Thx to https://github.com/maxicombina/c-calculator/tree/master

#include "calc.h"


/* Stack implementation */
void stack_init(simple_stack_t *s) {
    s->pos = -1;
    memset(s->data, 0x00, sizeof(s->data));
}

void stack_push(int64_t data, simple_stack_t *s) {
    if (s->pos + 1 >= MAX_ADT_SIZE) exit(1);
    s->pos++;
    s->data[s->pos] = data;
}

int64_t stack_pop(simple_stack_t *s) {
    if (stack_is_empty(s)) exit(11);
    return s->data[s->pos--];
}

int stack_is_empty(const simple_stack_t *s) {
    return s->pos == -1;
}

int64_t stack_peek(const simple_stack_t *s) {
    if (stack_is_empty(s)) exit(2);
    return s->data[s->pos];
}

/* Queue implementation */
void queue_init(simple_queue_t* q) {
    q->front = q->back = -1;
    memset(q->data, 0x00, sizeof(q->data));
}

void queue_enqueue(token_t data, simple_queue_t* q) {
    if (q->back + 1 >= MAX_ADT_SIZE) exit(3);
    if (q->front == -1) {
        /* Empty queue */
        q->front = 0;
    }

    q->back++;
    q->data[q->back] = data;
}

token_t queue_dequeue (simple_queue_t* q) {
    if (queue_is_empty(q)) exit(4);
    return q->data[q->front++];
}

int queue_is_empty(const simple_queue_t* q) {
    return (q->front == -1 || q->front > q->back);
}

int get_op_precedence(char op) {
    int res = 0;
    if (op == '+' || op == '-') res = 9;
    else if (op == '*' || op == '/' || op == '%') res = 10;
    else if (op == '^') res = 11;
    return res;
}

static int is_higher_or_equal_precedence(char op1, char op2) {
    return get_op_precedence(op1) >= get_op_precedence(op2);
}

static int is_higher_precedence(char op1, char op2) {
    return get_op_precedence(op1) > get_op_precedence(op2);
}

static int is_left_assoc_operator(char op) {
    return (op == '+' || op == '-' || op == '*' || op == '/' || op == '%');
}

static int is_right_assoc_operator(char op) {
    return (op == '^');
}

void shunting_yard(const char* input, simple_stack_t* ops, simple_queue_t* output) {
    size_t i = 0;
    simple_stack_t parenthesis; /* Check balanced parenthesis */
    stack_init(&parenthesis);

    while (i < strlen(input)) {
        char c = input[i];
        token_t new_token = {0};

        /* A Tokenizer should be implemented. Right now it does not support negative numbers */
        switch (c) {
            /* Skip blanks */
            case ' ':
            case '\t':
                i++;
                break;

            /* Process operators */
            case '+':
            case '-':
            case '/':
            case '*':
            case '%':
            case '^': {
                while ( !stack_is_empty(ops)              &&                       /* there is an operator in the stack */
                        /*is_operator(stack_peek(ops)) &&*/
                        (
                            (
                                is_left_assoc_operator(c) &&                       /* it is a left-assoc operator and ... */
                                is_higher_or_equal_precedence(stack_peek(ops), c)  /* ... has higher/equal preference than the one in the stack */
                            )
                            ||                                /* OR */
                            (
                                is_right_assoc_operator(c) &&                       /* it is a right-assoc operator and ... */
                                is_higher_precedence(stack_peek(ops), c)            /* ... has higher preference than the one in the stack */
                            )
                        )
                    ) {
                    int64_t stack_top = stack_pop(ops);
                    new_token.type = TOKEN_OPERATOR;
                    new_token.val = stack_top;
                    queue_enqueue(new_token, output);
                }

                stack_push(c, ops);
                i++;
                break;
            }

            case '(': {
                stack_push(c, ops);
                stack_push(c, &parenthesis);
                i++;
                break;
            }
            case ')': {
                while ( !stack_is_empty(ops) &&
                        /*is_operator(stack_peek(ops)) &&*/
                        (char) stack_peek(ops) != '('
                      ) {
                    int64_t stack_top = stack_pop(ops);
                    new_token.type = TOKEN_OPERATOR;
                    new_token.val = stack_top;
                    queue_enqueue(new_token, output);
                }

                /* Balanced parenthesis check */
                if (stack_is_empty(&parenthesis)) {
                    /* We must have a '(' */
                    exit(8);
                }
                
                stack_pop(&parenthesis);

                i++;
                break;
            }

            default: {
                int64_t number = 0;
                if (!isdigit(c)){
                    exit(7);
                }

                while (isdigit(input[i])){
                    /* Convert into number */
                    number *= 10;
                    number += input[i] - '0';
                    i++;
                }

                new_token.type = TOKEN_NUMBER;
                new_token.val = number;
                queue_enqueue(new_token, output);
                break;
            }
        }
    }


    /* Parenthesis stack must be empty */
    if (!stack_is_empty(&parenthesis)) {
        exit(6);
    }

    /* Put remaining operators in the tokens queue */
    while (!stack_is_empty(ops)) {
        int64_t stack_top = stack_pop(ops);
        token_t new_token = {TOKEN_OPERATOR, stack_top};

        queue_enqueue(new_token, output);
    }

}

int64_t compute_rpn(simple_queue_t *q) {
    int64_t res = 0;
    simple_stack_t work_area;
    stack_init(&work_area);

    while (!queue_is_empty(q)) {
        token_t queue_front = queue_dequeue(q);
        if (queue_front.type == TOKEN_OPERATOR) {
            switch (queue_front.val) {
                case '+': {
                    int64_t op1, op2;
                    op2 = stack_pop(&work_area);
                    op1 = stack_pop(&work_area);
                    res = op1 + op2;
                    stack_push(res, &work_area);
                    break;
                }
                case '-': {
                    int64_t op1, op2;
                    op2 = stack_pop(&work_area);
                    op1 = stack_pop(&work_area);
                    res = op1 - op2;
                    stack_push(res, &work_area);
                    break;
                }
                case '*': {
                    int64_t op1, op2;
                    op2 = stack_pop(&work_area);
                    op1 = stack_pop(&work_area);
                    res = op1 * op2;
                    stack_push(res, &work_area);
                    break;
                }
                case '/': {
                    int64_t op1, op2;
                    op2 = stack_pop(&work_area);
                    op1 = stack_pop(&work_area);
                    if (op2 == 0) {
                         exit(10);
                    }

                    res = op1 / op2;
                    stack_push(res, &work_area);
                    break;
                }
                case '%': {
                    int64_t op1, op2;
                    op2 = stack_pop(&work_area);
                    op1 = stack_pop(&work_area);
                    if (op2 == 0) {
                        exit(9);
                    }

                    res = op1 % op2;
                    stack_push(res, &work_area);
                    break;
                }
                case '^': {
                    int64_t op1, op2;
                    op2 = stack_pop(&work_area);
                    op1 = stack_pop(&work_area);
                    res = (int64_t) pow(op1, op2);
                    stack_push(res, &work_area);
                    break;
                }
            }
        }
        else {
            stack_push(queue_front.val, &work_area);
        }
    }

    if (work_area.pos != 0) {
        exit(10);
    }

    return stack_peek(&work_area);
}

int main(int argc, char* argv[]) {
    int64_t result;
    simple_queue_t output;
    simple_stack_t ops;

    if (argc != 2) {
        return 13;
    }

    queue_init(&output);
    stack_init(&ops);

    shunting_yard(argv[1], &ops, &output);

    result = compute_rpn(&output);
    printf("%lld", result);
    return 100;
}