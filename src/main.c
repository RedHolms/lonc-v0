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

  T_INC = 256,          /* '++' */
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
  T_DLLIMPORT,
  T_FUNCTION,
  T_MUTABLE,
  T_CONST,
  T_LET,
  T_WHILE,
  T_RETURN,
  T_NULLPTR,
  T_IF,
  T_ELSE,

  /* built-in types */
  T_TP___CXX_WCHAR_T,
  T_TP___ASCII_CHAR,
  T_TP_INT,
  T_TP_UINT,
  T_TP_POINTER
} TokenKind;

typedef struct _S_Token {
  TokenKind kind;
  char *s;
  int64_t n;
  struct _S_Token* next;
} Token;

enum _E_EOLFormat {
  LF = 1,
  CRLF,
  CR
};

char *source_begin = NULL, *src = NULL;
int line = 1, col = 1;
int eolf = 0;
Token *first_tk = NULL, *last_tk = NULL;

inline const char* eolf_as_str() {
  switch (eolf) {
  default:
    return "undefined";
  case LF:
    return "LF";
  case CRLF:
    return "CRLF";
  case CR:
    return "CR";
  }
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

inline void *my_malloc(size_t n) {
  return HeapAlloc(GetProcessHeap(), 0, n);
}

inline void my_free(void *block) {
  HeapFree(GetProcessHeap(), 0, block);
}

void print(const char *msg) {
  HANDLE con;
  DWORD written;

  con = GetStdHandle(STD_OUTPUT_HANDLE);
  WriteConsoleA(con, msg, my_strlen(msg), &written, NULL);
}

void print_int(int val) {
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
  case T_DLLIMPORT: print("T_DLLIMPORT "); fl=1; break;
  case T_FUNCTION: print("T_FUNCTION "); fl=1; break;
  case T_MUTABLE: print("T_MUTABLE "); fl=1; break;
  case T_CONST: print("T_CONST "); fl=1; break;
  case T_LET: print("T_LET "); fl=1; break;
  case T_WHILE: print("T_WHILE "); fl=1; break;
  case T_RETURN: print("T_RETURN "); fl=1; break;
  case T_NULLPTR: print("T_NULLPTR "); fl=1; break;
  case T_IF: print("T_IF "); fl=1; break;
  case T_ELSE: print("T_ELSE "); fl=1; break;
  case T_TP___CXX_WCHAR_T: print("T_TP___CXX_WCHAR_T "); fl=1; break;
  case T_TP___ASCII_CHAR: print("T_TP___ASCII_CHAR "); fl=1; break;
  case T_TP_INT: print("T_TP_INT "); fl=1; break;
  case T_TP_UINT: print("T_TP_UINT "); fl=1; break;
  case T_TP_POINTER: print("T_TP_POINTER "); fl=1; break;
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

Token* tok(TokenKind k) {
  Token *tk = my_malloc(sizeof(Token));

  if (last_tk)
    last_tk->next = tk;
  if (!first_tk)
    first_tk = tk;

  tk->kind = k;
  tk->s = NULL;
  tk->n = 0;
  tk->next = NULL;
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
    tk->kind = T_DLLIMPORT;
  else if (my_strcmp(tk->s, "function") == 0)
    tk->kind = T_FUNCTION;
  else if (my_strcmp(tk->s, "mutable") == 0)
    tk->kind = T_MUTABLE;
  else if (my_strcmp(tk->s, "const") == 0)
    tk->kind = T_CONST;
  else if (my_strcmp(tk->s, "let") == 0)
    tk->kind = T_LET;
  else if (my_strcmp(tk->s, "while") == 0)
    tk->kind = T_WHILE;
  else if (my_strcmp(tk->s, "return") == 0)
    tk->kind = T_RETURN;
  else if (my_strcmp(tk->s, "nullptr") == 0)
    tk->kind = T_NULLPTR;
  else if (my_strcmp(tk->s, "if") == 0)
    tk->kind = T_IF;
  else if (my_strcmp(tk->s, "else") == 0)
    tk->kind = T_ELSE;
  else if (my_strcmp(tk->s, "__cxx_wchar_t") == 0)
    tk->kind = T_TP___CXX_WCHAR_T;
  else if (my_strcmp(tk->s, "__ascii_char") == 0)
    tk->kind = T_TP___ASCII_CHAR;
  else if (my_strcmp(tk->s, "int") == 0)
    tk->kind = T_TP_INT;
  else if (my_strcmp(tk->s, "uint") == 0)
    tk->kind = T_TP_UINT;
  else if (my_strcmp(tk->s, "pointer") == 0)
    tk->kind = T_TP_POINTER;
}

void lex_error(const char *err) {
  print("# lexer error at ");
  print_int(line);
  print(":");
  print_int(col);
  print("\n");
  print("# ");
  print(err);
  print("\n");
}

Token* lex() {
lex_begin:
  if (*src == 0)
    return NULL;

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
    return tok(*src++);

  case '+':
    if (*(src+1) == '+') {
      src += 2; col += 2;
      return tok(T_INC);
    }
    goto one_char_tok;
  case '-':
    if (*(src+1) == '-') {
      src += 2; col += 2;
      return tok(T_DEC);
    }
    if (*(src+1) == '>') {
      src += 2; col += 2;
      return tok(T_ARROW);
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
      return tok(T_EQ);
    }
    goto one_char_tok;
  case '!':
    if (*(src+1) == '=') {
      src += 2; col += 2;
      return tok(T_NE);
    }
    goto one_char_tok;
  case '>':
    if (*(src+1) == '=') {
      src += 2; col += 2;
      return tok(T_GE);
    }
    goto one_char_tok;
  case '<':
    if (*(src+1) == '=') {
      src += 2; col += 2;
      return tok(T_LE);
    }
    goto one_char_tok;

    /* ID (or keyword). Must begin with ASCII char or underscore. Other chars
     * can be digits (see is_id_char) */
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

    Token *tk = tok(T_ID);

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
    Token *tk = tok(T_INT);

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

    Token *tk = tok(T_STR);
    size_t len = 0;

    for (char *p = src;;) {
      switch (*p) {
      case 0:
        lex_error("unexpected end of file inside a string");
        return NULL;

      case '\n':
      case '\r':
        lex_error("unexpected end of line inside a string");
        return NULL;

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
          lex_error("invalid escape sequence");
          return NULL;
        }

        tk->s[i] = ch;
        ++i; ++src; ++col;
        continue;
      }

      /* generic char */
      tk->s[i++] = *src++; ++col;
    }

    /* skip the ending '"' */
    ++src; ++col;

    return tk;
  }

  default:
    lex_error("unexpected characters");
    return NULL;
  }
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
  while (1) {
    tk = lex();
    if (tk == NULL)
      break;
    print_tok(tk);
  }

cleanup:
  my_free(source_begin);
  CloseHandle(input);

  return code;
}
