/** Lon programming language bootstrap (v0) compiler written in C.
 * Made by RedHolms ( http://redholms.dev ) in Jun-Jul 2026.
 *
 * This code implements bare minimum lexer, parser and LLVM-IR gen. At this
 * stage language looks more like naked version of C with stupid syntax.
 * This code uses only Win32 API without any crt function for easier porting to
 * the Lon later.
 */

#include <Windows.h>
#include <stdint.h>

typedef enum _E_TokenKind {
  T_OPENING_CUR = '{',
  T_CLOSING_CUR = '}',
  T_OPENING_PAR = '(',
  T_CLOSING_PAR = ')',
  T_PLUS = '+',
  T_MINUS = '-',
  T_STAR = '*',
  T_AMP = '&',
  T_COLON = ':',
  T_SEMI = ';',
  T_COMMA = ',',
  T_EQUAL = '=',
  T_GT = '>',
  T_LT = '<',
  T_EXC = '!',

  _T_LAST_CHAR_TOKEN = 255,

  T_INC,                /* '++' */
  T_DEC,                /* '--' */
  T_ARROW,              /* '->' */
  T_EQ,                 /* '==' */
  T_NE,                 /* '!=' */
  T_GE,                 /* '>=' */
  T_LE,                 /* '<=' */

  /* tokens with payloads */
  T_ID,
  T_STR,
  T_INT,

  /* keywords */
  KW_DLLIMPORT,
  KW_FUNCTION,
  KW_MUTABLE,
  KW_CONST,
  KW_LET,
  KW_WHILE,
  KW_RETURN,
  KW_NULLPTR,
  KW_IF,
  KW_ELSE,
  KW___CXX_WCHAR_T,
  KW___ASCII_CHAR,
  KW_INT,
  KW_UINT,
  KW_ULONG,
  KW_POINTER,

  /* pseudo token (specials) */
  T_ERR,
  T_EOF
} TokenKind;

typedef struct _S_Token {
  TokenKind kind;
  char *s;
  int64_t n;
  int tline, tcol;
  struct _S_Token* next; /* we keep all tokens in a linked list (for garbage
   collection maybe?) */
} Token;

enum _E_EOLFormat {
  /* 0 = undefined */
  LF = 1,
  CRLF,
  CR
};

enum _E_ASTNodeType {
  AST_TYPE,
  AST_FUNC_ARG,

  AST_MODULE,

  AST_FUNC_DECL,
  AST_FUNC_DEF,
  AST_VAR_DECL,

  /* statements */
  AST_WHILE,
  AST_IF,
  AST_RETURN,

  /* expressions */
  AST_VAR,
  AST_INT_LIT,
  AST_STR_LIT,
  AST_UNARY_OP,
  AST_BINARY_OP,
  AST_CALL,
  AST_NULLPTR
};

#define AST_NODE_SHARED_FIELDS                                                 \
  int tp; /* type of the node (_E_ASTNodeType) */                              \
  struct _S_ASTNode* next /* next node in a linked list (if applicable) */

typedef struct _S_ASTNode {
  AST_NODE_SHARED_FIELDS;
  /* content is determined by the type */
} ASTNode;

enum _E_TypeKind {
  TK_BUILTIN,
  TK_POINTER
};

enum _E_BuiltinTypes {
  BIT_VOID,
  BIT___CXX_WCHAR_T,
  BIT___ASCII_CHAR,
  BIT_INT,
  BIT_UINT,
  BIT_ULONG,
  BIT_POINTER
};

typedef struct _S_ASTNode_Type {
  AST_NODE_SHARED_FIELDS;
  int tp_kind;
  union {
    struct {
      int tp;
    } builtin;
    struct {
      int is_const;
      struct _S_ASTNode_Type *underlying;
    } ptr;
  };
} ASTNode_Type;

typedef struct _S_ASTNode_FuncArg {
  AST_NODE_SHARED_FIELDS;
  char *name;
  ASTNode_Type *var_tp;
} ASTNode_FuncArg;

typedef struct _S_ASTNode_Module {
  AST_NODE_SHARED_FIELDS;
  char *file;
  ASTNode *decls_ll;
} ASTNode_Module;

typedef struct _S_ASTNode_FuncDecl {
  AST_NODE_SHARED_FIELDS;
  char is_dllimport;
  char *name;
  ASTNode_Type *ret_tp;
  ASTNode_FuncArg *args_ll;
} ASTNode_FuncDecl;

typedef struct _S_ASTNode_FuncDef {
  AST_NODE_SHARED_FIELDS;
  char *name;
  ASTNode_Type *ret_tp;
  ASTNode_FuncArg *args_ll;
  ASTNode *body_ll;
} ASTNode_FuncDef;

typedef struct _S_ASTNode_VarDecl {
  AST_NODE_SHARED_FIELDS;
  char *name;
  ASTNode_Type *var_tp;
  ASTNode *init_expr;
} ASTNode_VarDecl;

typedef struct _S_ASTNode_While {
  AST_NODE_SHARED_FIELDS;
  ASTNode *cond_expr;
  ASTNode *body_ll;
} ASTNode_While;

typedef struct _S_ASTNode_If {
  AST_NODE_SHARED_FIELDS;
  ASTNode *cond_expr;
  ASTNode *body_ll;
  ASTNode *else_body_ll; /* can be NULL */
} ASTNode_If;

typedef struct _S_ASTNode_Return {
  AST_NODE_SHARED_FIELDS;
  ASTNode *retval_expr;
} ASTNode_Return;

typedef struct _S_ASTNode_Var {
  AST_NODE_SHARED_FIELDS;
  char *name;
} ASTNode_Var;

typedef struct _S_ASTNode_IntLit {
  AST_NODE_SHARED_FIELDS;
  int64_t n;
} ASTNode_IntLit;

typedef struct _S_ASTNode_StrLit {
  AST_NODE_SHARED_FIELDS;
  char *s;
} ASTNode_StrLit;

enum _E_UnaryOp {
  UOP_DEREF,
  UOP_ADDROF,
  UOP_PLUS,
  UOP_MINUS,
  UOP_NOT,
  UOP_INC,
  UOP_DEC
};

enum _E_BinaryOp {
  BOP_EQ,
  BOP_NEQ,
  BOP_LT,
  BOP_GT,
  BOP_LE,
  BOP_GE,
  BOP_ADD,
  BOP_SUB,
  BOP_MUL,
  BOP_ASSIGN,
  BOP_ARROW
};

enum _E_OpPrecedence {
  PREC_NONE = 0,
  PREC_ASSIGN,
  PREC_EQUALITY,
  PREC_RELATIONAL,
  PREC_ADDITIVE,
  PREC_MULTIPLICATIVE,
  PREC_UNARY,
  PREC_CALL,
  PREC_PRIMARY
};

typedef struct _S_ASTNode_UnaryOp {
  AST_NODE_SHARED_FIELDS;
  char is_pre;
  int op;
  ASTNode *expr;
} ASTNode_UnaryOp;

typedef struct _S_ASTNode_BinaryOp {
  AST_NODE_SHARED_FIELDS;
  int op;
  ASTNode *lhs, *rhs;
} ASTNode_BinaryOp;

typedef struct _S_ASTNode_Call {
  AST_NODE_SHARED_FIELDS;
  const char *name;
  ASTNode *args_ll;
} ASTNode_Call;

typedef struct _S_ASTNode_Nullptr {
  AST_NODE_SHARED_FIELDS;
} ASTNode_Nullptr;

char *source_begin = NULL, *src = NULL;
int line = 1, col = 1, lex_start_line, lex_start_col;
int eolf = 0;
Token *first_tk = NULL, *last_tk = NULL;
Token *cur_tk = NULL;
ASTNode *ast_root = NULL;

/* crt-replacement functions. all of them have original names but with 'my_'
prefix */

inline void *my_malloc(size_t n) {
  return HeapAlloc(GetProcessHeap(), 0, n);
}

inline void my_free(void *block) {
  HeapFree(GetProcessHeap(), 0, block);
}

size_t my_strlen(const char *str) {
  size_t len = 0;

  while (*str) {
    ++str;
    ++len;
  }

  return len;
}

int my_strcmp(const char *a, const char *b) {
  int i = 0;

  for (; a[i] && b[i]; ++i) {
    if (a[i] != b[i])
      return a[i] - b[i];
  }

  /* got a null term */

  if (a[i] == b[i])
    return 0; /* both strings ended, they're the same */

  return a[i] ? 1 : -1;
}

void my_strcpy(char *dest, const char *src_str) {
  while ((*dest++ = *src_str++));
}

char *my_strdup(const char *s) {
  char *r;

  r = my_malloc(my_strlen(s) + 1);
  my_strcpy(r, s);
  return r;
}

/* console output functions */

void print(const char *msg) {
  if (!msg) {
    print("<null>");
    return;
  }
  HANDLE con;
  DWORD written;

  con = GetStdHandle(STD_OUTPUT_HANDLE);
  WriteConsoleA(con, msg, my_strlen(msg), &written, NULL);
}

void print_int(int val) {
  if (val == 0) {
    print("0");
    return;
  }

  /* TODO: handle signed numbers */
  char buf[64];
  char *bp = buf + sizeof(buf) - 1;

  *bp = 0;
  while (val > 0) {
    *(--bp) = '0' + (val % 10);
    val /= 10;
  }

  print(bp);
}

void print_tok(Token *tk) {
  int fl = 0;

  switch (tk->kind) {
  case T_OPENING_CUR: print("T_OPENING_CUR "); break;
  case T_CLOSING_CUR: print("T_CLOSING_CUR "); break;
  case T_OPENING_PAR: print("T_OPENING_PAR "); break;
  case T_CLOSING_PAR: print("T_CLOSING_PAR "); break;
  case T_PLUS: print("T_PLUS "); break;
  case T_MINUS: print("T_MINUS "); break;
  case T_STAR: print("T_STAR "); break;
  case T_AMP: print("T_AMP "); break;
  case T_COLON: print("T_COLON "); break;
  case T_SEMI: print("T_SEMI "); break;
  case T_COMMA: print("T_COMMA "); break;
  case T_EQUAL: print("T_EQUAL "); break;
  case T_GT: print("T_GT "); break;
  case T_LT: print("T_LT "); break;
  case T_EXC: print("T_EXC "); break;
  case T_INC: print("T_INC "); break;
  case T_DEC: print("T_DEC "); break;
  case T_ARROW: print("T_ARROW "); break;
  case T_EQ: print("T_EQ "); break;
  case T_NE: print("T_NE "); break;
  case T_GE: print("T_GE "); break;
  case T_LE: print("T_LE "); break;
  case T_ID: print("T_ID "); fl=1; break;
  case T_STR: print("T_STR "); fl=1; break;
  case T_INT: print("T_INT "); fl=2; break;
  case KW_DLLIMPORT: print("KW_DLLIMPORT "); fl=1; break;
  case KW_FUNCTION: print("KW_FUNCTION "); fl=1; break;
  case KW_MUTABLE: print("KW_MUTABLE "); fl=1; break;
  case KW_CONST: print("KW_CONST "); fl=1; break;
  case KW_LET: print("KW_LET "); fl=1; break;
  case KW_WHILE: print("KW_WHILE "); fl=1; break;
  case KW_RETURN: print("KW_RETURN "); fl=1; break;
  case KW_NULLPTR: print("KW_NULLPTR "); fl=1; break;
  case KW_IF: print("KW_IF "); fl=1; break;
  case KW_ELSE: print("KW_ELSE "); fl=1; break;
  case KW___CXX_WCHAR_T: print("KW___CXX_WCHAR_T "); fl=1; break;
  case KW___ASCII_CHAR: print("KW___ASCII_CHAR "); fl=1; break;
  case KW_INT: print("KW_INT "); fl=1; break;
  case KW_UINT: print("KW_UINT "); fl=1; break;
  case KW_ULONG: print("KW_ULONG "); fl=1; break;
  case KW_POINTER: print("KW_POINTER "); fl=1; break;
  default:
    print_int(tk->kind);
    print(" ");
    break;
  }

  if (fl&1) {
    print(tk->s);
    print(" ");
  }

  if (fl&2) {
    print_int(tk->n);
  }

  print("\n");
}

void invalid_eol() {
  print("# invalid EOL\n");
  ExitProcess(1);
}

Token *mktok(TokenKind k) {
  Token *tk = my_malloc(sizeof(Token));

  if (last_tk)
    last_tk->next = tk;
  if (!first_tk)
    first_tk = tk;

  tk->kind = k;
  tk->s = NULL;
  tk->n = 0;
  tk->next = NULL;
  tk->tline = lex_start_line;
  tk->tcol = lex_start_col;
  return last_tk = tk;
}

inline int is_id_char(char ch) {
  switch (ch) {
  case 'A': case 'B': case 'C': case 'D': case 'E': case 'F': case 'G':
  case 'H': case 'I': case 'J': case 'K': case 'L': case 'M': case 'N':
  case 'O': case 'P': case 'Q': case 'R': case 'S': case 'T': case 'U':
  case 'V': case 'W': case 'X': case 'Y': case 'Z':
  case 'a': case 'b': case 'c': case 'd': case 'e': case 'f': case 'g':
  case 'h': case 'i': case 'j': case 'k': case 'l': case 'm': case 'n':
  case 'o': case 'p': case 'q': case 'r': case 's': case 't': case 'u':
  case 'v': case 'w': case 'x': case 'y': case 'z':
  case '_':
  case '0': case '1': case '2': case '3': case '4': case '5': case '6':
  case '7': case '8': case '9':
    return 1;
  }
  return 0;
}

inline int is_digit(char ch) {
  switch (ch) {
  case '0': case '1': case '2': case '3': case '4': case '5': case '6':
  case '7': case '8': case '9':
    return 1;
  }
  return 0;
}

void process_kws(Token *tk) {
  /* do the stupid way */
  if (my_strcmp(tk->s, "dllimport") == 0)
    tk->kind = KW_DLLIMPORT;
  else if (my_strcmp(tk->s, "function") == 0)
    tk->kind = KW_FUNCTION;
  else if (my_strcmp(tk->s, "mutable") == 0)
    tk->kind = KW_MUTABLE;
  else if (my_strcmp(tk->s, "const") == 0)
    tk->kind = KW_CONST;
  else if (my_strcmp(tk->s, "let") == 0)
    tk->kind = KW_LET;
  else if (my_strcmp(tk->s, "while") == 0)
    tk->kind = KW_WHILE;
  else if (my_strcmp(tk->s, "return") == 0)
    tk->kind = KW_RETURN;
  else if (my_strcmp(tk->s, "nullptr") == 0)
    tk->kind = KW_NULLPTR;
  else if (my_strcmp(tk->s, "if") == 0)
    tk->kind = KW_IF;
  else if (my_strcmp(tk->s, "else") == 0)
    tk->kind = KW_ELSE;
  else if (my_strcmp(tk->s, "__cxx_wchar_t") == 0)
    tk->kind = KW___CXX_WCHAR_T;
  else if (my_strcmp(tk->s, "__ascii_char") == 0)
    tk->kind = KW___ASCII_CHAR;
  else if (my_strcmp(tk->s, "int") == 0)
    tk->kind = KW_INT;
  else if (my_strcmp(tk->s, "uint") == 0)
    tk->kind = KW_UINT;
  else if (my_strcmp(tk->s, "ulong") == 0)
    tk->kind = KW_ULONG;
  else if (my_strcmp(tk->s, "pointer") == 0)
    tk->kind = KW_POINTER;
}

Token *lex_error(const char *err) {
  print("# lexer error at ");
  print_int(line);
  print(":");
  print_int(col);
  print("\n");
  print("# ");
  print(err);
  print("\n");
  return mktok(T_ERR);
}

/* consume next token from the text */
Token* lex() {
lex_begin:
  lex_start_line = line;
  lex_start_col = col;

  if (*src == 0)
    return mktok(T_EOF);

  switch (*src) {
  case ' ':
  case '\t': /* skip spaces */
    ++col; ++src;
    goto lex_begin;

  /* eol checking begin */
  case '\r':
    if (eolf == 0) {
      /* might be CR or CRLF */
      if (*(src+1) == '\n') {
        /* crlf */
        eolf = CRLF;
        src += 2;
        goto eol;
      }

      /* cr */
      eolf = CR;
      ++src;
      goto eol;
    }

    ++src;

    switch (eolf) {
    case CRLF:
      /* expect an LF */
      if (*src != '\n')
        invalid_eol();

      ++src;
    case CR:
      goto eol;
    case LF:
      invalid_eol();
    }

  case '\n':
    if (eolf != 0 && eolf != LF) {
      invalid_eol();
    }
    else {
      /* can only be LF */
      eolf = LF;
    }

    ++src;

  eol:
    col = 1;
    ++line;
    goto lex_begin;
  /* eol checking end */

  case '{':
  case '}':
  case '(':
  case ')':
  case '&':
  case '*':
  case ':':
  case ';':
  case ',':
  one_char_tok:
    ++col;
    return mktok(*src++);

  case '+':
    if (*(src+1) == '+') {
      src += 2; col += 2;
      return mktok(T_INC);
    }
    goto one_char_tok;
  case '-':
    if (*(src+1) == '-') {
      src += 2; col += 2;
      return mktok(T_DEC);
    }
    if (*(src+1) == '>') {
      src += 2; col += 2;
      return mktok(T_ARROW);
    }

    /* parse negative ints */
    if (is_digit(*(src+1))) {
      ++src; ++col;
      Token *tk = lex();
      tk->n = -tk->n;
      return tk;
    }

    goto one_char_tok;
  case '=':
    if (*(src+1) == '=') {
      src += 2; col += 2;
      return mktok(T_EQ);
    }
    goto one_char_tok;
  case '!':
    if (*(src+1) == '=') {
      src += 2; col += 2;
      return mktok(T_NE);
    }
    goto one_char_tok;
  case '>':
    if (*(src+1) == '=') {
      src += 2; col += 2;
      return mktok(T_GE);
    }
    goto one_char_tok;
  case '<':
    if (*(src+1) == '=') {
      src += 2; col += 2;
      return mktok(T_LE);
    }
    goto one_char_tok;

  /* ID (or keyword). Must begin with a letter or an underscore. Other chars can
   * be digits (see is_id_char) */
  case 'A': case 'B': case 'C': case 'D': case 'E': case 'F': case 'G':
  case 'H': case 'I': case 'J': case 'K': case 'L': case 'M': case 'N':
  case 'O': case 'P': case 'Q': case 'R': case 'S': case 'T': case 'U':
  case 'V': case 'W': case 'X': case 'Y': case 'Z':
  case 'a': case 'b': case 'c': case 'd': case 'e': case 'f': case 'g':
  case 'h': case 'i': case 'j': case 'k': case 'l': case 'm': case 'n':
  case 'o': case 'p': case 'q': case 'r': case 's': case 't': case 'u':
  case 'v': case 'w': case 'x': case 'y': case 'z':
  case '_': {
    char *start = src;

    ++src; ++col;
    while (is_id_char(*src)) {
      ++src; ++col;
    }

    Token *tk = mktok(T_ID);

    size_t len = src - start;
    tk->s = my_malloc(len+1);

    for (size_t i = 0; i < len; ++i)
      tk->s[i] = start[i];
    tk->s[len] = 0;

    process_kws(tk); /* check if token is a keyword */
    return tk;
  }

  case '0': case '1': case '2': case '3': case '4': case '5': case '6':
  case '7': case '8': case '9': {
    Token *tk = mktok(T_INT);

    tk->n = 0;
    while (is_digit(*src)) {
      tk->n *= 10;
      tk->n += (*src) - '0';
      ++src; ++col;
    }

    return tk;
  }

  case '"': {
    ++src; ++col;

    Token *tk = mktok(T_STR);
    size_t len = 0;

    for (char *p = src;;) {
      switch (*p) {
      case 0:
        return lex_error("unexpected end of file inside a string");

      case '\n':
      case '\r':
        return lex_error("unexpected end of line inside a string");

      case '\\': /* escape sequence. don't check it right now */
        /* consume 2 chars, but count only 1 */
        p += 2;
        ++len;
        continue;

      default: /* generic char */
        ++p;
        ++len;
        continue;

      case '"': /* end of string */
        break;
      }

      break;
    }

    tk->s = my_malloc(len + 1);
    tk->s[len] = 0;

    for (int i = 0; i < len;) {
      if (*src == '\\') {
        /* escape sequence */
        ++src; ++col;

        char ch;
        switch (*src) {
        case 'r': ch = '\r'; break;
        case 'n': ch = '\n'; break;
        case 't': ch = '\t'; break;
        default:
          return lex_error("invalid escape sequence");
        }

        tk->s[i] = ch;
        ++i; ++src; ++col;
        continue;
      }

      /* generic char */
      tk->s[i++] = *src++; ++col;
    }

    /* skip the ending quote */
    ++src; ++col;

    return tk;
  }

  default:
    return lex_error("unexpected characters");
  }
}

inline Token *lpeek() {
  if (cur_tk)
    return cur_tk;

  return cur_tk = lex();
}

/* get the next token */
inline Token *ladvance() {
  if (cur_tk) {
    Token *tk = cur_tk;
    cur_tk = NULL;
    return tk;
  }

  return lex();
}

inline void *mk_ast_node(int tp) {
  ASTNode *node;
  size_t size;

  switch (tp) {
  case AST_TYPE: size = sizeof(ASTNode_Type); break;
  case AST_FUNC_ARG: size = sizeof(ASTNode_FuncArg); break;
  case AST_MODULE: size = sizeof(ASTNode_Module); break;
  case AST_FUNC_DECL: size = sizeof(ASTNode_FuncDecl); break;
  case AST_FUNC_DEF: size = sizeof(ASTNode_FuncDef); break;
  case AST_VAR_DECL: size = sizeof(ASTNode_VarDecl); break;
  case AST_WHILE: size = sizeof(ASTNode_While); break;
  case AST_IF: size = sizeof(ASTNode_If); break;
  case AST_RETURN: size = sizeof(ASTNode_Return); break;
  case AST_VAR: size = sizeof(ASTNode_Var); break;
  case AST_INT_LIT: size = sizeof(ASTNode_IntLit); break;
  case AST_STR_LIT: size = sizeof(ASTNode_StrLit); break;
  case AST_UNARY_OP: size = sizeof(ASTNode_UnaryOp); break;
  case AST_BINARY_OP: size = sizeof(ASTNode_BinaryOp); break;
  case AST_CALL: size = sizeof(ASTNode_Call); break;
  case AST_NULLPTR: size = sizeof(ASTNode_Nullptr); break;
  }
  node = my_malloc(size);
  for (size_t i = 0; i < size; ++i) /* fill with zeroes */
    ((char*)node)[i] = 0;
  node->tp = tp;
  return node;
}

inline ASTNode_Type *mk_void_tp() {
  ASTNode_Type *node = mk_ast_node(AST_TYPE);
  node->tp_kind = TK_BUILTIN;
  node->builtin.tp = BIT_VOID;
  return node;
}

inline void ast_ll_insert(ASTNode **ll_first, ASTNode *el) {
  if (*ll_first == NULL) {
    *ll_first = el;
  }
  else {
    ASTNode *last = *ll_first;
    while (last->next)
      last = last->next;
    last->next = el;
  }
}

enum _E_Modifiers {
  HAS_DLLIMPORT = 0x1
};

void parse_error_at_tk(const char *err, Token* tk) {
  print("# parser error at ");
  print_int(tk->tline);
  print(":");
  print_int(tk->tcol);
  print("\n# ");
  print(err);
  print("\n");
  ExitProcess(1);
}

void parse_error(const char *err) {
  parse_error_at_tk(err, lpeek());
}

/* type parsing. parses leaf types */
ASTNode_Type *parse_type1() {
  int bit_tp;

  switch (lpeek()->kind) {
  case KW___CXX_WCHAR_T:
    bit_tp = BIT___CXX_WCHAR_T; break;
  case KW___ASCII_CHAR:
    bit_tp = BIT___ASCII_CHAR; break;
  case KW_INT:
    bit_tp = BIT_INT; break;
  case KW_UINT:
    bit_tp = BIT_UINT; break;
  case KW_ULONG:
    bit_tp = BIT_ULONG; break;
  case KW_POINTER:
    bit_tp = BIT_POINTER; break;
  default:
    parse_error("invalid type");
  }

  ladvance();
  ASTNode_Type *res = mk_ast_node(AST_TYPE);
  res->tp_kind = TK_BUILTIN;
  res->builtin.tp = bit_tp;
  return res;
}

/* parses complex types (pointers, etc.). returns NULL if type is complete */
ASTNode_Type *parse_type2(ASTNode_Type *underlying) {
  if (lpeek()->kind == KW_MUTABLE || lpeek()->kind == KW_CONST) {
    char is_const = lpeek()->kind == KW_CONST;

    ladvance();
    if (lpeek()->kind != T_STAR)
      parse_error("'*' expected for a pointer type");
    ladvance();

    ASTNode_Type *ptr_tp = mk_ast_node(AST_TYPE);
    ptr_tp->tp_kind = TK_POINTER;
    ptr_tp->ptr.is_const = is_const;
    ptr_tp->ptr.underlying = underlying;
    return ptr_tp;
  }

  return NULL;
}

/* type parsing. uses 'parse_type1' for leaf types parsing, then checks for
 pointers and other */
ASTNode_Type *parse_type() {
  ASTNode_Type *res;

  res = parse_type1();
  while (1) {
    ASTNode_Type *next = parse_type2(res);
    if (!next)
      return res;
    res = next;
  }
}

inline int get_precedence(TokenKind kind) {
  switch (kind) {
  case T_EQUAL: return PREC_ASSIGN;
  case T_EQ: case T_NE: return PREC_EQUALITY;
  case T_LT: case T_GT: case T_LE: case T_GE: return PREC_RELATIONAL;
  case T_PLUS: case T_MINUS: return PREC_ADDITIVE;
  case T_STAR: return PREC_MULTIPLICATIVE;
  case T_OPENING_PAR: case T_ARROW: return PREC_CALL;
  default: return PREC_NONE;
  }
}

ASTNode *parse_expression_with_precedence(int precedence);
ASTNode *parse_expression();

ASTNode *parse_prefix() {
  int uop;

  switch (lpeek()->kind) {
  case T_INT: {
    ASTNode_IntLit *node = mk_ast_node(AST_INT_LIT);
    node->n = lpeek()->n;
    ladvance();
    return (ASTNode *)node;
  }
  case T_STR: {
    ASTNode_StrLit *node = mk_ast_node(AST_STR_LIT);
    node->s = my_strdup(lpeek()->s);
    ladvance();
    return (ASTNode *)node;
  }
  case T_ID: {
    ASTNode_Var *node = mk_ast_node(AST_VAR);
    node->name = my_strdup(lpeek()->s);
    ladvance();
    return (ASTNode *)node;
  }

  case T_STAR:
    uop = UOP_DEREF;
    goto parse_unary_op;
  case T_AMP:
    uop = UOP_ADDROF;
    goto parse_unary_op;
  case T_PLUS:
    uop = UOP_PLUS;
    goto parse_unary_op;
  case T_MINUS:
    uop = UOP_MINUS;
    goto parse_unary_op;
  case T_EXC:
    uop = UOP_NOT;
    goto parse_unary_op;
  case T_INC:
    uop = UOP_INC;
    goto parse_unary_op;
  case T_DEC:
    uop = UOP_DEC;

  parse_unary_op: {
    ladvance();
    ASTNode_UnaryOp *node = mk_ast_node(AST_UNARY_OP);
    node->is_pre = 1;
    node->op = uop;
    node->expr = parse_expression_with_precedence(PREC_UNARY);
    return (ASTNode *)node;
  }

  case T_OPENING_PAR: {
    ladvance();
    ASTNode *expr = parse_expression();
    if (lpeek()->kind != T_CLOSING_PAR)
      parse_error("closing parenthesis expected");
    ladvance();
    return expr;
  }

  default:
    parse_error("unexpected token");
  }
}

/* current token must be opening parenthesis */
ASTNode *parse_call(const char *func_name) {
  ladvance();

  ASTNode_Call *call = mk_ast_node(AST_CALL);
  call->name = my_strdup(func_name);

  if (lpeek()->kind != T_CLOSING_PAR) {
    while (1) {
      ASTNode *arg = parse_expression_with_precedence(PREC_NONE);
      ast_ll_insert(&call->args_ll, arg);

      if (lpeek()->kind == T_CLOSING_PAR)
        break;

      if (lpeek()->kind != T_COMMA)
        parse_error("expected a comma");

      ladvance();
    }
  }

  ladvance();
  return (ASTNode *)call;
}

/* Second stage of expression parsing. Will try to parse a function call or some
 * binary operation. If expression cannot be further parsed, returns NULL. */
ASTNode *parse_infix(ASTNode *lhs, int precedence) {
  int op, prec;

  if (lpeek()->kind == T_OPENING_PAR) {
    if (lhs->tp != AST_VAR)
      parse_error("invalid call expression: lhs must be an ID");

    return parse_call(((ASTNode_Var*)lhs)->name);
  }

  switch (lpeek()->kind) {
  case T_INC:
    op = UOP_INC;
    goto parse_unary_op;
  case T_DEC:
    op = UOP_DEC;
  parse_unary_op: {
    ASTNode_UnaryOp *node = mk_ast_node(AST_UNARY_OP);
    node->is_pre = 0;
    node->expr = lhs;
    node->op = op;
    return (ASTNode*)node;
  }

  case T_NE: op = BOP_NEQ; prec = PREC_EQUALITY; goto parse_binary_op;
  case T_EQ: op = BOP_EQ; prec = PREC_EQUALITY; goto parse_binary_op;
  case T_LT: op = BOP_LT; prec = PREC_RELATIONAL; goto parse_binary_op;
  case T_GT: op = BOP_GT; prec = PREC_RELATIONAL; goto parse_binary_op;
  case T_LE: op = BOP_LE; prec = PREC_RELATIONAL; goto parse_binary_op;
  case T_GE: op = BOP_GE; prec = PREC_RELATIONAL; goto parse_binary_op;
  case T_PLUS: op = BOP_ADD; prec = PREC_ADDITIVE; goto parse_binary_op;
  case T_MINUS: op = BOP_SUB; prec = PREC_ADDITIVE; goto parse_binary_op;
  case T_STAR: op = BOP_MUL; prec = PREC_MULTIPLICATIVE; goto parse_binary_op;
  case T_EQUAL: op = BOP_ASSIGN; prec = PREC_ASSIGN; goto parse_binary_op;
  case T_ARROW: op = BOP_ARROW; prec = PREC_CALL;
  parse_binary_op: {
    if (precedence >= prec)
      return NULL;

    ladvance();
    ASTNode_BinaryOp *node = mk_ast_node(AST_BINARY_OP);
    node->lhs = lhs;
    node->op = op;
    node->rhs = parse_expression_with_precedence(prec);
    return (ASTNode*)node;
  }

  default:
    return NULL;
  }
}

ASTNode *parse_expression_with_precedence(int precedence) {
  if (lpeek()->kind == KW_NULLPTR) {
    ladvance();
    return mk_ast_node(AST_NULLPTR);
  }

  ASTNode *res = parse_prefix();

  while (1) {
    ASTNode *comp = parse_infix(res, precedence);

    if (comp == NULL)
      return res;

    res = comp;
  }
}

ASTNode *parse_expression() {
  return parse_expression_with_precedence(PREC_NONE);
}

/* current lex token must be KW_LET */
ASTNode *parse_let() {
  ASTNode_VarDecl *res;

  ladvance();

  if (lpeek()->kind != T_ID)
    parse_error("expected variable name");

  res = mk_ast_node(AST_VAR_DECL);
  res->name = my_strdup(ladvance()->s);

  if (lpeek()->kind != T_COLON)
    parse_error("expected colon");
  ladvance();

  res->var_tp = parse_type();

  if (lpeek()->kind == T_EQUAL) {
    /* has init expr */
    ladvance();
    res->init_expr = parse_expression();
  }

  if (lpeek()->kind != T_SEMI)
    parse_error("semicolon expected");
  ladvance();

  return (ASTNode*)res;
}

ASTNode *parse_statement();
void parse_body(ASTNode **ll);

/* current lex token must be KW_WHILE */
ASTNode *parse_while() {
  ASTNode_While *res;
  ASTNode *cond_expr;

  ladvance();

  if (lpeek()->kind != T_OPENING_PAR)
    parse_error("expected '(' (for 'while' condition)");
  ladvance();

  cond_expr = parse_expression();

  if (lpeek()->kind != T_CLOSING_PAR)
    parse_error("expected ')' (for 'while' condition)");
  ladvance();

  if (lpeek()->kind != T_OPENING_CUR)
    parse_error("'while' loop body expected ('{')");

  res = mk_ast_node(AST_WHILE);
  res->cond_expr = cond_expr;
  parse_body(&res->body_ll);

  return (ASTNode*)res;
}

ASTNode *parse_return() {
  ASTNode_Return *res;

  ladvance();

  res = mk_ast_node(AST_RETURN);
  res->retval_expr = parse_expression();

  if (lpeek()->kind != T_SEMI)
    parse_error("semicolon expected");
  ladvance();

  return (ASTNode*)res;
}

ASTNode *parse_statement();

ASTNode *parse_if() {
  ASTNode_If *res;

  ladvance();

  if (lpeek()->kind != T_OPENING_PAR)
    parse_error("expected '(' (for 'if' condition)");
  ladvance();

  res = mk_ast_node(AST_IF);
  res->cond_expr = parse_expression();

  if (lpeek()->kind != T_CLOSING_PAR)
    parse_error("expected ')' (for 'if' condition)");
  ladvance();

  if (lpeek()->kind == T_OPENING_CUR) {
    parse_body(&res->body_ll);
  }
  else {
    res->body_ll = parse_statement();
  }

  if (lpeek()->kind == KW_ELSE) {
    ladvance();
    if (lpeek()->kind == T_OPENING_CUR) {
      parse_body(&res->else_body_ll);
    }
    else {
      res->else_body_ll = parse_statement();
    }
  }

  return (ASTNode*)res;
}

ASTNode *parse_statement() {
  switch (lpeek()->kind) {
  case KW_LET:
    return parse_let();
  case KW_WHILE:
    return parse_while();
  case KW_RETURN:
    return parse_return();
  case KW_IF:
    return parse_if();
  default: {
    ASTNode *expr = parse_expression();
    if (lpeek()->kind != T_SEMI)
      parse_error("semicolon expected");
    ladvance();
    return expr;
  }
  }
}

/* current lex token must be '{' */
void parse_body(ASTNode **ll) {
  ladvance();
  while (1) {
    if (lpeek()->kind == T_CLOSING_CUR) {
      ladvance();
      return;
    }

    ast_ll_insert(ll, parse_statement());
  }
}

/* current lex token must be function name */
ASTNode *parse_func(int state) {
  Token *fname_tk;
  char *fname;
  ASTNode_FuncArg *args_ll = NULL, *arg;
  char expect_arg = 0;
  ASTNode_Type *ret_tp;

  fname_tk = ladvance();
  if (fname_tk->kind != T_ID)
    parse_error_at_tk("function name expected", fname_tk);
  fname = my_strdup(fname_tk->s);

  if (lpeek()->kind != T_OPENING_PAR)
    parse_error("opening parenthesis expected (for argument list)");
  ladvance();

  /* argument list */
  while (1) {
    switch (lpeek()->kind) {
    case T_COMMA:
      if (!args_ll || expect_arg)
        parse_error("expected an argument, but got a comma");
      expect_arg = 1;
      ladvance();
      break;
    case T_ID:
      if (args_ll && !expect_arg)
        parse_error("expected a comma or closing parenthesis");
      ast_ll_insert((ASTNode**)&args_ll, (ASTNode*)(arg
        = mk_ast_node(AST_FUNC_ARG)));
      arg->name = my_strdup(ladvance()->s);
      if (lpeek()->kind != T_COLON)
        parse_error("expected a colon in argument declaration");
      ladvance();
      arg->var_tp = parse_type();
      expect_arg = 0;
      break;
    case T_CLOSING_PAR:
      if (expect_arg)
        parse_error("expected one more argument, but argument list closed");
      ladvance();
      goto end_args_parse_loop;
    default:
      parse_error("unexpected token in function argument list");
    }
  } end_args_parse_loop:

  /* return type */
  if (lpeek()->kind == T_ARROW) {
    ladvance();
    ret_tp = parse_type();
  }
  else {
    ret_tp = mk_void_tp();
  }

  /* now we check if this is a declaration or a definition */

  if (lpeek()->kind == T_SEMI) {
    ladvance();
    ASTNode_FuncDecl *decl = mk_ast_node(AST_FUNC_DECL);
    decl->is_dllimport = state & HAS_DLLIMPORT;
    decl->name = fname;
    decl->args_ll = args_ll;
    decl->ret_tp = ret_tp;
    return (ASTNode*)decl;
  }

  /* must be definition */
  if (lpeek()->kind != T_OPENING_CUR)
    parse_error("expected function body or a semicolon");

  if (state&HAS_DLLIMPORT)
    parse_error("dllimport functions can't have a body");

  ASTNode_FuncDef *def = mk_ast_node(AST_FUNC_DEF);
  def->name = fname;
  def->args_ll = args_ll;
  def->ret_tp = ret_tp;
  parse_body(&def->body_ll);
  return (ASTNode*)def;
}

void print_ast_tree(ASTNode *tree, int depth) {
  for (int i = 0; i < depth*4; ++i)
    print(" ");
  if (!tree) {
    print("<NULL NODE>");
    return;
  }

  switch (tree->tp) {
  case AST_TYPE: {
    ASTNode_Type *node = (ASTNode_Type*)tree;
    print("AST_TYPE");
    switch (node->tp_kind) {
    case TK_BUILTIN:
      print(" builtin ");
      switch (node->builtin.tp) {
        case BIT_VOID: print("BIT_VOID"); break;
        case BIT___CXX_WCHAR_T: print("BIT___CXX_WCHAR_T"); break;
        case BIT___ASCII_CHAR: print("BIT___ASCII_CHAR"); break;
        case BIT_INT: print("BIT_INT"); break;
        case BIT_UINT: print("BIT_UINT"); break;
        case BIT_ULONG: print("BIT_ULONG"); break;
        case BIT_POINTER: print("BIT_POINTER"); break;
      }
      break;
    case TK_POINTER:
      if (node->ptr.is_const)
        print(" const ");
      else
        print(" mutable ");
      print(" pointer\n");
      print_ast_tree((ASTNode*)node->ptr.underlying, depth+1);
      print("\n");
      break;
    }
    break;
  }
  case AST_FUNC_ARG: {
    ASTNode_FuncArg *node = (ASTNode_FuncArg*)tree;
    print("AST_FUNC_ARG ");
    print(node->name);
    print("\n");
    for (int i = 0; i < depth; ++i)
      print(" ");
    print("type:\n");
    print_ast_tree((ASTNode*)node->var_tp, depth+1);
    print("\n");
    break;
  }
  case AST_MODULE: {
    ASTNode_Module *node = (ASTNode_Module*)tree;
    print("AST_MODULE ");
    print(node->file);
    print("\n");
    for (int i = 0; i < depth; ++i)
      print(" ");
    print("decls:\n");
    print_ast_tree(node->decls_ll, depth+1);
    print("\n");
    break;
  }
  case AST_FUNC_DECL: {
    ASTNode_FuncDecl *node = (ASTNode_FuncDecl*)tree;
    print("AST_FUNC_DECL ");
    if (node->is_dllimport)
      print("dllimport ");
    print(node->name);
    print("\n");
    for (int i = 0; i < depth; ++i)
      print(" ");
    print("ret_tp:\n");
    print_ast_tree((ASTNode*)node->ret_tp, depth+1);
    print("\n");
    for (int i = 0; i < depth; ++i)
      print(" ");
    if (node->args_ll) {
      print("args:\n");
      print_ast_tree((ASTNode*)node->args_ll, depth+1);
      print("\n");
    }
    break;
  }
  case AST_FUNC_DEF: {
    ASTNode_FuncDef *node = (ASTNode_FuncDef*)tree;
    print("AST_FUNC_DEF ");
    print(node->name);
    print("\n");
    for (int i = 0; i < depth; ++i)
      print(" ");
    print("ret_tp:\n");
    print_ast_tree((ASTNode*)node->ret_tp, depth+1);
    print("\n");
    for (int i = 0; i < depth; ++i)
      print(" ");
    print("args:\n");
    print_ast_tree((ASTNode*)node->args_ll, depth+1);
    print("\n");
    for (int i = 0; i < depth; ++i)
      print(" ");
    print("body:\n");
    print_ast_tree(node->body_ll, depth+1);
    print("\n");
    break;
  }
  case AST_VAR_DECL: {
    ASTNode_VarDecl *node = (ASTNode_VarDecl*)tree;
    print("AST_VAR_DECL ");
    print(node->name);
    print("\n");
    if (node->var_tp) {
      print_ast_tree((ASTNode*)node->var_tp, depth+1);
      print("\n");
    }
    if (node->init_expr) {
      print_ast_tree(node->init_expr, depth+1);
      print("\n");
    }
    break;
  }
  case AST_WHILE: {
    ASTNode_While *node = (ASTNode_While*)tree;
    print("AST_WHILE\n");
    print_ast_tree(node->cond_expr, depth+1);
    print("\n");
    print_ast_tree(node->body_ll, depth+1);
    print("\n");
    break;
  }
  case AST_IF: {
    ASTNode_If *node = (ASTNode_If*)tree;
    print("AST_IF\n");
    print_ast_tree(node->cond_expr, depth+1);
    print("\n");
    print_ast_tree(node->body_ll, depth+1);
    print("\n");
    if (node->else_body_ll) {
      print_ast_tree(node->else_body_ll, depth+1);
      print("\n");
    }
    break;
  }
  case AST_RETURN: {
    ASTNode_Return *node = (ASTNode_Return*)tree;
    print("AST_RETURN\n");
    print_ast_tree(node->retval_expr, depth+1);
    print("\n");
    break;
  }
  case AST_VAR: {
    ASTNode_Var *node = (ASTNode_Var*)tree;
    print("AST_VAR ");
    print(node->name);
    print("\n");
    break;
  }
  case AST_INT_LIT: {
    ASTNode_IntLit *node = (ASTNode_IntLit*)tree;
    print("AST_INT_LIT ");
    print_int(node->n);
    break;
  }
  case AST_STR_LIT: {
    ASTNode_StrLit *node = (ASTNode_StrLit*)tree;
    print("AST_STR_LIT");
    break;
  }
  case AST_UNARY_OP: {
    ASTNode_UnaryOp *node = (ASTNode_UnaryOp*)tree;
    print("AST_UNARY_OP ");
    if (node->is_pre)
      print("pre ");
    else
      print("post ");
    switch (node->op) {
    case UOP_DEREF: print("UOP_DEREF "); break;
    case UOP_ADDROF: print("UOP_ADDROF "); break;
    case UOP_PLUS: print("UOP_PLUS "); break;
    case UOP_MINUS: print("UOP_MINUS "); break;
    case UOP_NOT: print("UOP_NOT "); break;
    case UOP_INC: print("UOP_INC "); break;
    case UOP_DEC: print("UOP_DEC "); break;
    }
    print("\n");
    print_ast_tree(node->expr, depth+1);
    print("\n");
    break;
  }
  case AST_BINARY_OP: {
    ASTNode_BinaryOp *node = (ASTNode_BinaryOp*)tree;
    print("AST_BINARY_OP ");
    switch (node->op) {
    case BOP_EQ: print("BOP_EQ "); break;
    case BOP_NEQ: print("BOP_NEQ "); break;
    case BOP_LT: print("BOP_LT "); break;
    case BOP_GT: print("BOP_GT "); break;
    case BOP_LE: print("BOP_LE "); break;
    case BOP_GE: print("BOP_GE "); break;
    case BOP_ADD: print("BOP_ADD "); break;
    case BOP_SUB: print("BOP_SUB "); break;
    case BOP_MUL: print("BOP_MUL "); break;
    case BOP_ASSIGN: print("BOP_ASSIGN "); break;
    case BOP_ARROW: print("BOP_ARROW "); break;
    }
    print("\n");
    print_ast_tree(node->lhs, depth+1);
    print("\n");
    print_ast_tree(node->rhs, depth+1);
    print("\n");
    break;
  }
  case AST_CALL: {
    ASTNode_Call *node = (ASTNode_Call*)tree;
    print("AST_CALL ");
    print(node->name);
    print("\n");
    print_ast_tree(node->args_ll, depth+1);
    break;
  }
  case AST_NULLPTR: {
    ASTNode_Nullptr *node = (ASTNode_Nullptr*)tree;
    print("AST_NULLPTR");
    break;
  }
  }
  print("\n");
  if (tree->next)
    print_ast_tree(tree->next, depth);
}

/* parser entry point */
void parse() {
  int modifiers = 0;
  ASTNode_Module *module;

  module = mk_ast_node(AST_MODULE);
  ast_root = (ASTNode*)module;

  while (1) {
    switch (lpeek()->kind) {
    case KW_DLLIMPORT:
      if (modifiers&HAS_DLLIMPORT)
        parse_error("multiple 'dllimport' modifiers");
      ladvance();
      modifiers |= HAS_DLLIMPORT;
      break;

    case KW_FUNCTION:
      ladvance();
      ast_ll_insert(&module->decls_ll, parse_func(modifiers));
      modifiers = 0;
      break;

    case T_ERR:
      return;
    case T_EOF:
      goto end_parse_loop;

    default:
      parse_error("unexpected token");
    }
  } end_parse_loop:

  print_ast_tree((ASTNode*)module, 0);

  return;
}

int main(int argc, char **argv) {
  int code = 0;
  char *inputPath, *outputPath;
  HANDLE input = NULL, output = NULL;
  LARGE_INTEGER inputSize;
  BOOL success;
  Token *tk = NULL;

  if (argc != 3) {
    print("# invalid command line input\n");
    return 1;
  }

  inputPath = argv[1];
  outputPath = argv[2];

  input = CreateFileA(inputPath, GENERIC_READ, FILE_SHARE_READ, NULL,
                      OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

  if (input == INVALID_HANDLE_VALUE) {
    print("# failed to open the input file\n");
    code = 1;
    goto cleanup;
  }

  success = GetFileSizeEx(input, &inputSize);

  if (!success) {
    print("# failed to get size of the input file\n");
    code = 1;
    goto cleanup;
  }

  source_begin = my_malloc(inputSize.LowPart + 1);
  ReadFile(input, source_begin, inputSize.LowPart, NULL, NULL);
  source_begin[inputSize.LowPart] = 0;

  CloseHandle(input);
  input = NULL;

  src = source_begin;
  parse();

cleanup:
  my_free(source_begin);
  CloseHandle(input);

  return code;
}