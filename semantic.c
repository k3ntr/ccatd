#include <string.h>

#include "ccatd.h"

void sema_func(Func*);
void sema_block(Vec*, Func*);
void sema_stmt(Node*, Func*, int scope_start);
void sema_expr(Node*);
bool eq_type(Type*, Type*);

Vec *func_env;
Vec *local_vars;

Node *find_lvar_sema(Node *node);
int index_of_lvar(Node *node);

Func *find_func(Node *node);

// ------------------------------------------------------------

void sema_func(Func *func) {
    int params_len = vec_len(func->params);
    // no duplicate parameter
    for (int i = 0; i < params_len; i++) {
        Node *pi = vec_at(func->params, i);
        for (int j = i+1; j < params_len; j++) {
            Node *pj = vec_at(func->params, j);
            if (!memcmp(pi->name, pj->name, pi->len)) {
                fnputs(stderr, func->name, func->len);
                fprintf(stderr, ": duplicate parameter `");
                fnputs(stderr, pi->name, pj->len);
                error("'");
            }
        }
    }

    vec_push(func_env, func);

    // block
    local_vars = vec_new();
    for (int i = 0; i < params_len; i++)
        vec_push(local_vars, vec_at(func->params, i));

    sema_block(func->block, func);
}

void sema_block(Vec *block, Func *func) {
    int scope_start = vec_len(local_vars);
    int block_len = vec_len(block);
    for (int i = 0; i < block_len; i++)
        sema_stmt(vec_at(block, i), func, scope_start);

    while (vec_len(local_vars) > scope_start)
        vec_pop(local_vars);
}

void sema_stmt(Node *node, Func *func, int scope_start) {
    if (node->kind == ND_VARDECL) {
        int idx = index_of_lvar(node->lhs);
        if (scope_start <= idx && idx < vec_len(local_vars))
            error("duplicate variable");

        sema_expr(node->rhs);
        eq_type(node->lhs->type, node->rhs->type);
        vec_push(local_vars, node->lhs);
        node->type = node->rhs->type;
        return;
    }
    if (node->kind == ND_RETURN) {
        sema_expr(node->lhs);
        eq_type(node->lhs->type, func->ret_type);
        return;
    }
    if (node->kind == ND_IF) {
        sema_expr(node->cond);
        sema_stmt(node->lhs, func, scope_start);
        if (node->rhs != NULL)
            sema_stmt(node->rhs, func, scope_start);
        return;
    }
    if (node->kind == ND_WHILE) {
        sema_expr(node->cond);
        sema_stmt(node->body, func, scope_start);
        return;
    }
    if (node->kind == ND_FOR) {
        sema_expr(node->cond);
        sema_expr(node->lhs);
        sema_expr(node->rhs);
        sema_stmt(node->body, func, scope_start);
        return;
    }
    if (node->kind == ND_BLOCK) {
        sema_block(node->block, func);
        return; 
    }

    sema_expr(node);
}

void sema_expr(Node* node) {
    // ND_NUM,
    if (node->kind == ND_NUM) {
        eq_type(type_int, node->type);
        return;
    }
    // ND_LVAR,
    if (node->kind == ND_LVAR) {
        Node *resolved = find_lvar_sema(node);
        if (resolved == NULL)
            error("undefined variable");
        node->type = resolved->type;
        return;
    }
    // ND_ASGN,
    if (node->kind == ND_ASGN) {
        sema_expr(node->lhs);
        sema_expr(node->rhs);
        if (!eq_type(node->lhs->type, node->rhs->type))
            error("type mismatch in an assignment statment");
        node->type = node->lhs->type;
        return;
    }
    // ND_ADDR,
    if (node->kind == ND_ADDR) {
        sema_expr(node->lhs);
        node->type = ptr_of(node->lhs->type);
        return;
    }
    // ND_DEREF,
    if (node->kind == ND_DEREF) {
        sema_expr(node->lhs);
        if (node->lhs->type->ty != TY_PTR) {
            error("dereferencing non-pointer is not allowed");
        }
        node->type = node->lhs->type->ptr_to;
        return;
    }
    // ND_CALL,
    if (node->kind == ND_CALL) {
        Func *f = find_func(node);
        if (f == NULL)
            error("undefined function");

        int params_len = vec_len(node->block);
        if (params_len != vec_len(f->params))
            error("invalid number of argument(s)");
        for (int i = 0; i < params_len; i++)
            sema_expr(vec_at(node->block, i));

        node->type = f->ret_type;
        return;
    }

    sema_expr(node->lhs);
    sema_expr(node->rhs);
    Type* lty = node->lhs->type;
    Type* rty = node->rhs->type;
    // ND_ADD,
    if (node->kind == ND_ADD) {
        if (is_int(lty) && is_int(rty)) {
            node->type = type_int;
        } else if (is_ptr(lty) && is_int(rty)) {
            node->type = lty;
        } else if (is_int(lty) && is_ptr(rty)) {
            node->type = rty;
        } else {
            error("unsupported addition");
        }
        return;
    }
    // ND_SUB,
    if (node->kind == ND_SUB) {
        if (is_int(lty) && is_int(rty)) {
            node->type = type_int;
        } else if (is_ptr(lty) && is_int(rty)) {
            node->type = lty;
        } else {
            error("unsupported subtraction");
        }
        return;
    }
    // ND_MUL,
    if (node->kind == ND_MUL) {
        if (is_int(lty) && is_int(rty)) {
            node->type = type_int;
        } else {
            error("unsupported addition");
        }
        return;
    }
    // ND_DIV,
    if (node->kind == ND_DIV) {
        if (is_int(lty) && is_int(rty)) {
            node->type = type_int;
        } else {
            error("unsupported addition");
        }
        return;
    }
    // ND_EQ,
    if (node->kind == ND_EQ) {
        node->type = type_int;
        return;
    }
    // ND_NEQ,
    if (node->kind == ND_NEQ) {
        node->type = type_int;
        return;
    }
    // ND_LT,
    if (node->kind == ND_LT) {
        node->type = type_int;
        return;
    }
    // ND_LTE,
    if (node->kind == ND_LTE) {
        node->type = type_int;
        return;
    }
    error("unsupported feature");
}

bool eq_type(Type* t1, Type* t2) {
    if (t1->ty == TY_PTR && t2->ty == TY_PTR) {
        return eq_type(t1->ptr_to, t2->ptr_to);
    } else {
        return t1->ty == t2->ty;
    }
}

Node *find_lvar_sema(Node *target) {
    int idx = index_of_lvar(target);
    return (0 <= idx && idx < vec_len(local_vars)) ? vec_at(local_vars, idx) : NULL;
}

int index_of_lvar(Node *target) {
    int local_vars_len = vec_len(local_vars);
    int ret = local_vars_len;
    for (int i = 0; i < local_vars_len; i++) {
        Node *var = vec_at(local_vars, i);
        if (var->len == target->len &&
                memcmp(var->name, target->name, var->len) == 0)
            ret = i;
    }
    return ret;
}

Func *find_func(Node *node) {
    int funcs_len = vec_len(func_env);
    for (int i = 0; i < funcs_len; i++) {
        Func *func = vec_at(func_env, i);
        if (func->len == node->len &&
                memcmp(func->name, node->name, func->len) == 0)
            return func;
    }
    return NULL;
}
