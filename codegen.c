#include <stdio.h>

#include "ccatd.h"

char *arg_regs[6] = { "rdi", "rsi", "rdx", "rcx", "r8", "r9", };

void gen(Node*);
void gen_stmt(Node*);
bool is_expr(Node_kind);

// generate

int label_num = 0;

void gen(Node *node) {
    if (node->kind == ND_NUM) {
        printf("  mov rax, %d\n", node->val);
        printf("  push %d\n", node->val);
        return;
    }

    if (node->kind == ND_LVAR) {
        printf("  mov rax, rbp\n");
        printf("  sub rax, %d\n", node->offset);
        printf("  mov rax, [rax]\n");
        printf("  push rax\n");
        return;
    }

    if (node->kind == ND_ASGN) {
        if (node->lhs->kind != ND_LVAR)
            error("left hand side of an assignment should be a left value");

        printf("  mov rax, rbp\n");
        printf("  sub rax, %d\n", node->lhs->offset);
        printf("  push rax\n");

        gen(node->rhs);

        printf("  pop rax\n"
               "  pop rdi\n"
               "  mov [rdi], rax\n"
               "  push rax\n");
       return;
    }

    if (node->kind == ND_RETURN) {
        gen(node->lhs);
        printf("  pop rax\n"
               "  mov rsp, rbp\n"
               "  pop rbp\n"
               "  ret\n");
        return;
    }

    if (node->kind == ND_IF) {
        gen(node->cond);
        printf("  pop rax\n"
               "  cmp rax, 0\n");
        if (node->rhs == NULL) {
            printf("  je .Lend_if%d\n", label_num);
            gen_stmt(node->lhs);
            printf(".Lend_if%d:\n", label_num);
        } else {
            printf("  je  .Lelse%d\n", label_num);
            gen_stmt(node->lhs);
            printf("  jmp .Lend_if%d\n", label_num);
            printf(".Lelse%d:\n", label_num);
            gen_stmt(node->rhs);
            printf(".Lend_if%d:\n", label_num);
        }

        label_num += 1;
        return;
    }

    if (node->kind == ND_WHILE) {
        printf(".Lwhile%d:\n", label_num);
        gen(node->cond);
        printf("  pop rax\n"
               "  cmp rax, 0\n");
        printf("  je .Lend_while%d\n", label_num);
        gen_stmt(node->body);
        printf("  jmp .Lwhile%d\n", label_num);
        printf(".Lend_while%d:\n", label_num);

        label_num += 1;
        return;
    }

    if (node->kind == ND_FOR) {
        gen_stmt(node->lhs);
        printf(".Lfor%d:\n", label_num);
        gen(node->cond);
        printf("  pop rax\n"
               "  cmp rax, 0\n");
        printf("  je .Lend_for%d\n", label_num);
        gen_stmt(node->body);
        gen_stmt(node->rhs);
        printf("  jmp .Lfor%d\n", label_num);
        printf(".Lend_for%d:\n", label_num);
        return;
    }

    if (node->kind == ND_BLOCK) {
        int len = vec_len(node->block);
        for (int i = 0; i < len; i++)
            gen_stmt(vec_at(node->block, i));
        return;
    }

    if (node->kind == ND_CALL) {
        int arg_len = vec_len(node->block);
        for (int i = 0; i < arg_len; i++)
            gen(vec_at(node->block, i));
        for (int i = arg_len-1; i >= 0; i--)
            printf("  pop %s\n", arg_regs[i]);
        printf("  and rsp, -16\n");
        printf("  call ");
        for (int i = 0; i < node->len; i++)
            putc(node->func[i], stdout);
        printf("\n");
        printf("  push rax\n");
        return;
    }

    gen(node->lhs);
    gen(node->rhs);

    printf("  pop rdi\n");
    printf("  pop rax\n");

    switch (node->kind) {
        case ND_ADD:
            printf("  add rax, rdi\n");
            break;
        case ND_SUB:
            printf("  sub rax, rdi\n");
            break;
        case ND_MUL:
            printf("  imul rax, rdi\n");
            break;
        case ND_DIV:
            printf("  cqo\nidiv rdi\n");
            break;
        default:
            printf("  cmp rax, rdi\n");
            switch (node->kind) {
                case ND_EQ:
                    printf("  sete al\n");
                    break;
                case ND_NEQ:
                    printf("  setne al\n");
                    break;
                case ND_LT:
                    printf("  setl al\n");
                    break;
                case ND_LTE:
                    printf("  setle al\n");
                    break;
                default:
                    error("should be unreachable");
            }
            printf("  movzb rax, al\n");
    }
    printf("  push rax\n");
}

void gen_stmt(Node* node) {
    gen(node);
    if (is_expr(node->kind))
        printf("  pop rax\n");
}

bool is_expr(Node_kind kind) {
    static Node_kind expr_kinds[] = {
        ND_NUM, ND_LVAR, ND_ASGN, ND_ADD, ND_SUB, ND_MUL, ND_DIV, ND_EQ, ND_NEQ, ND_LT, ND_LTE
    };
    for (int i = 0; i < sizeof(expr_kinds) / sizeof(Node_kind); i++)
        if (expr_kinds[i] == kind) return true;
    return false;
}

