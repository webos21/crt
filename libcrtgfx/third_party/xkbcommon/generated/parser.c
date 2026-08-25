/* A Bison parser, made by GNU Bison 3.8.2.  */

/* Bison implementation for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015, 2018-2021 Free Software Foundation,
   Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* C LALR(1) parser skeleton written by Richard Stallman, by
   simplifying the original so-called "semantic" parser.  */

/* DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
   especially those whose name start with YY_ or yy_.  They are
   private implementation details that can be changed or removed.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output, and Bison version.  */
#define YYBISON 30802

/* Bison version string.  */
#define YYBISON_VERSION "3.8.2"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 1

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1


/* Substitute the variable and function names.  */
#define yyparse         _xkbcommon_parse
#define yylex           _xkbcommon_lex
#define yyerror         _xkbcommon_error
#define yydebug         _xkbcommon_debug
#define yynerrs         _xkbcommon_nerrs

/* First part of user prologue.  */
#line 20 "parser.y"

#include "config.h"

#include <assert.h>

#include "keysym.h"
#include "xkbcomp/xkbcomp-priv.h"
#include "xkbcomp/parser-priv.h"
#include "xkbcomp/ast-build.h"

struct parser_param {
    struct xkb_context *ctx;
    struct scanner *scanner;
    XkbFile *rtrn;
    bool more_maps;
};

#define parser_err(param, error_id, fmt, ...) \
    scanner_err((param)->scanner, error_id, fmt, ##__VA_ARGS__)

#define parser_warn(param, warning_id, fmt, ...) \
    scanner_warn((param)->scanner, warning_id, fmt, ##__VA_ARGS__)

static void
_xkbcommon_error(struct parser_param *param, const char *msg)
{
    parser_err(param, XKB_ERROR_INVALID_XKB_SYNTAX, "%s", msg);
}

static bool
resolve_keysym(struct parser_param *param, struct sval name, xkb_keysym_t *sym_rtrn)
{
    xkb_keysym_t sym;

    if (isvaleq(name, SVAL_LIT("any")) || isvaleq(name, SVAL_LIT("nosymbol"))) {
        *sym_rtrn = XKB_KEY_NoSymbol;
        return true;
    }

    if (isvaleq(name, SVAL_LIT("none")) || isvaleq(name, SVAL_LIT("voidsymbol"))) {
        *sym_rtrn = XKB_KEY_VoidSymbol;
        return true;
    }

    /* xkb_keysym_from_name needs a C string. */
    char buf[XKB_KEYSYM_NAME_MAX_SIZE];
    if (name.len >= sizeof(buf)) {
        return false;
    }
    memcpy(buf, name.start, name.len);
    buf[name.len] = '\0';

    sym = xkb_keysym_from_name(buf, XKB_KEYSYM_NO_FLAGS);
    if (sym != XKB_KEY_NoSymbol) {
        *sym_rtrn = sym;
        check_deprecated_keysyms(parser_warn, param, param->ctx,
                                 sym, buf, buf, "%s", "");
        return true;
    }

    return false;
}

#define param_scanner param->scanner

#line 142 "parser.c"

# ifndef YY_CAST
#  ifdef __cplusplus
#   define YY_CAST(Type, Val) static_cast<Type> (Val)
#   define YY_REINTERPRET_CAST(Type, Val) reinterpret_cast<Type> (Val)
#  else
#   define YY_CAST(Type, Val) ((Type) (Val))
#   define YY_REINTERPRET_CAST(Type, Val) ((Type) (Val))
#  endif
# endif
# ifndef YY_NULLPTR
#  if defined __cplusplus
#   if 201103L <= __cplusplus
#    define YY_NULLPTR nullptr
#   else
#    define YY_NULLPTR 0
#   endif
#  else
#   define YY_NULLPTR ((void*)0)
#  endif
# endif

#include "parser.h"
/* Symbol kind.  */
enum yysymbol_kind_t
{
  YYSYMBOL_YYEMPTY = -2,
  YYSYMBOL_YYEOF = 0,                      /* END_OF_FILE  */
  YYSYMBOL_YYerror = 1,                    /* error  */
  YYSYMBOL_YYUNDEF = 2,                    /* "invalid token"  */
  YYSYMBOL_ERROR_TOK = 3,                  /* ERROR_TOK  */
  YYSYMBOL_XKB_KEYMAP = 4,                 /* XKB_KEYMAP  */
  YYSYMBOL_XKB_KEYCODES = 5,               /* XKB_KEYCODES  */
  YYSYMBOL_XKB_TYPES = 6,                  /* XKB_TYPES  */
  YYSYMBOL_XKB_SYMBOLS = 7,                /* XKB_SYMBOLS  */
  YYSYMBOL_XKB_COMPATMAP = 8,              /* XKB_COMPATMAP  */
  YYSYMBOL_XKB_GEOMETRY = 9,               /* XKB_GEOMETRY  */
  YYSYMBOL_XKB_SEMANTICS = 10,             /* XKB_SEMANTICS  */
  YYSYMBOL_XKB_LAYOUT = 11,                /* XKB_LAYOUT  */
  YYSYMBOL_INCLUDE = 12,                   /* INCLUDE  */
  YYSYMBOL_OVERRIDE = 13,                  /* OVERRIDE  */
  YYSYMBOL_AUGMENT = 14,                   /* AUGMENT  */
  YYSYMBOL_REPLACE = 15,                   /* REPLACE  */
  YYSYMBOL_ALTERNATE = 16,                 /* ALTERNATE  */
  YYSYMBOL_VIRTUAL_MODS = 17,              /* VIRTUAL_MODS  */
  YYSYMBOL_TYPE = 18,                      /* TYPE  */
  YYSYMBOL_INTERPRET = 19,                 /* INTERPRET  */
  YYSYMBOL_ACTION_TOK = 20,                /* ACTION_TOK  */
  YYSYMBOL_KEY = 21,                       /* KEY  */
  YYSYMBOL_ALIAS = 22,                     /* ALIAS  */
  YYSYMBOL_GROUP = 23,                     /* GROUP  */
  YYSYMBOL_MODIFIER_MAP = 24,              /* MODIFIER_MAP  */
  YYSYMBOL_INDICATOR = 25,                 /* INDICATOR  */
  YYSYMBOL_SHAPE = 26,                     /* SHAPE  */
  YYSYMBOL_KEYS = 27,                      /* KEYS  */
  YYSYMBOL_ROW = 28,                       /* ROW  */
  YYSYMBOL_SECTION = 29,                   /* SECTION  */
  YYSYMBOL_OVERLAY = 30,                   /* OVERLAY  */
  YYSYMBOL_TEXT = 31,                      /* TEXT  */
  YYSYMBOL_OUTLINE = 32,                   /* OUTLINE  */
  YYSYMBOL_SOLID = 33,                     /* SOLID  */
  YYSYMBOL_LOGO = 34,                      /* LOGO  */
  YYSYMBOL_VIRTUAL = 35,                   /* VIRTUAL  */
  YYSYMBOL_EQUALS = 36,                    /* EQUALS  */
  YYSYMBOL_PLUS = 37,                      /* PLUS  */
  YYSYMBOL_MINUS = 38,                     /* MINUS  */
  YYSYMBOL_DIVIDE = 39,                    /* DIVIDE  */
  YYSYMBOL_TIMES = 40,                     /* TIMES  */
  YYSYMBOL_OBRACE = 41,                    /* OBRACE  */
  YYSYMBOL_CBRACE = 42,                    /* CBRACE  */
  YYSYMBOL_OPAREN = 43,                    /* OPAREN  */
  YYSYMBOL_CPAREN = 44,                    /* CPAREN  */
  YYSYMBOL_OBRACKET = 45,                  /* OBRACKET  */
  YYSYMBOL_CBRACKET = 46,                  /* CBRACKET  */
  YYSYMBOL_DOT = 47,                       /* DOT  */
  YYSYMBOL_COMMA = 48,                     /* COMMA  */
  YYSYMBOL_SEMI = 49,                      /* SEMI  */
  YYSYMBOL_EXCLAM = 50,                    /* EXCLAM  */
  YYSYMBOL_INVERT = 51,                    /* INVERT  */
  YYSYMBOL_STRING = 52,                    /* STRING  */
  YYSYMBOL_INTEGER = 53,                   /* INTEGER  */
  YYSYMBOL_FLOAT = 54,                     /* FLOAT  */
  YYSYMBOL_IDENT = 55,                     /* IDENT  */
  YYSYMBOL_KEYNAME = 56,                   /* KEYNAME  */
  YYSYMBOL_PARTIAL = 57,                   /* PARTIAL  */
  YYSYMBOL_DEFAULT = 58,                   /* DEFAULT  */
  YYSYMBOL_HIDDEN = 59,                    /* HIDDEN  */
  YYSYMBOL_ALPHANUMERIC_KEYS = 60,         /* ALPHANUMERIC_KEYS  */
  YYSYMBOL_MODIFIER_KEYS = 61,             /* MODIFIER_KEYS  */
  YYSYMBOL_KEYPAD_KEYS = 62,               /* KEYPAD_KEYS  */
  YYSYMBOL_FUNCTION_KEYS = 63,             /* FUNCTION_KEYS  */
  YYSYMBOL_ALTERNATE_GROUP = 64,           /* ALTERNATE_GROUP  */
  YYSYMBOL_YYACCEPT = 65,                  /* $accept  */
  YYSYMBOL_XkbFile = 66,                   /* XkbFile  */
  YYSYMBOL_XkbCompositeMap = 67,           /* XkbCompositeMap  */
  YYSYMBOL_XkbCompositeType = 68,          /* XkbCompositeType  */
  YYSYMBOL_XkbMapConfigList = 69,          /* XkbMapConfigList  */
  YYSYMBOL_XkbMapConfig = 70,              /* XkbMapConfig  */
  YYSYMBOL_FileType = 71,                  /* FileType  */
  YYSYMBOL_OptFlags = 72,                  /* OptFlags  */
  YYSYMBOL_Flags = 73,                     /* Flags  */
  YYSYMBOL_Flag = 74,                      /* Flag  */
  YYSYMBOL_DeclList = 75,                  /* DeclList  */
  YYSYMBOL_Decl = 76,                      /* Decl  */
  YYSYMBOL_VarDecl = 77,                   /* VarDecl  */
  YYSYMBOL_KeyNameDecl = 78,               /* KeyNameDecl  */
  YYSYMBOL_KeyAliasDecl = 79,              /* KeyAliasDecl  */
  YYSYMBOL_VModDecl = 80,                  /* VModDecl  */
  YYSYMBOL_VModDefList = 81,               /* VModDefList  */
  YYSYMBOL_VModDef = 82,                   /* VModDef  */
  YYSYMBOL_InterpretDecl = 83,             /* InterpretDecl  */
  YYSYMBOL_InterpretMatch = 84,            /* InterpretMatch  */
  YYSYMBOL_VarDeclList = 85,               /* VarDeclList  */
  YYSYMBOL_KeyTypeDecl = 86,               /* KeyTypeDecl  */
  YYSYMBOL_SymbolsDecl = 87,               /* SymbolsDecl  */
  YYSYMBOL_OptSymbolsBody = 88,            /* OptSymbolsBody  */
  YYSYMBOL_SymbolsBody = 89,               /* SymbolsBody  */
  YYSYMBOL_SymbolsVarDecl = 90,            /* SymbolsVarDecl  */
  YYSYMBOL_MultiKeySymOrActionList = 91,   /* MultiKeySymOrActionList  */
  YYSYMBOL_NoSymbolOrActionList = 92,      /* NoSymbolOrActionList  */
  YYSYMBOL_GroupCompatDecl = 93,           /* GroupCompatDecl  */
  YYSYMBOL_ModMapDecl = 94,                /* ModMapDecl  */
  YYSYMBOL_KeyOrKeySymList = 95,           /* KeyOrKeySymList  */
  YYSYMBOL_KeyOrKeySym = 96,               /* KeyOrKeySym  */
  YYSYMBOL_LedMapDecl = 97,                /* LedMapDecl  */
  YYSYMBOL_LedNameDecl = 98,               /* LedNameDecl  */
  YYSYMBOL_ShapeDecl = 99,                 /* ShapeDecl  */
  YYSYMBOL_SectionDecl = 100,              /* SectionDecl  */
  YYSYMBOL_SectionBody = 101,              /* SectionBody  */
  YYSYMBOL_SectionBodyItem = 102,          /* SectionBodyItem  */
  YYSYMBOL_RowBody = 103,                  /* RowBody  */
  YYSYMBOL_RowBodyItem = 104,              /* RowBodyItem  */
  YYSYMBOL_Keys = 105,                     /* Keys  */
  YYSYMBOL_Key = 106,                      /* Key  */
  YYSYMBOL_OverlayDecl = 107,              /* OverlayDecl  */
  YYSYMBOL_OverlayKeyList = 108,           /* OverlayKeyList  */
  YYSYMBOL_OverlayKey = 109,               /* OverlayKey  */
  YYSYMBOL_OutlineList = 110,              /* OutlineList  */
  YYSYMBOL_OutlineInList = 111,            /* OutlineInList  */
  YYSYMBOL_CoordList = 112,                /* CoordList  */
  YYSYMBOL_Coord = 113,                    /* Coord  */
  YYSYMBOL_DoodadDecl = 114,               /* DoodadDecl  */
  YYSYMBOL_DoodadType = 115,               /* DoodadType  */
  YYSYMBOL_FieldSpec = 116,                /* FieldSpec  */
  YYSYMBOL_Element = 117,                  /* Element  */
  YYSYMBOL_OptMergeMode = 118,             /* OptMergeMode  */
  YYSYMBOL_MergeMode = 119,                /* MergeMode  */
  YYSYMBOL_ExprList = 120,                 /* ExprList  */
  YYSYMBOL_Expr = 121,                     /* Expr  */
  YYSYMBOL_Term = 122,                     /* Term  */
  YYSYMBOL_MultiActionList = 123,          /* MultiActionList  */
  YYSYMBOL_ActionList = 124,               /* ActionList  */
  YYSYMBOL_NonEmptyActions = 125,          /* NonEmptyActions  */
  YYSYMBOL_Actions = 126,                  /* Actions  */
  YYSYMBOL_Action = 127,                   /* Action  */
  YYSYMBOL_Lhs = 128,                      /* Lhs  */
  YYSYMBOL_Terminal = 129,                 /* Terminal  */
  YYSYMBOL_MultiKeySymList = 130,          /* MultiKeySymList  */
  YYSYMBOL_KeySymList = 131,               /* KeySymList  */
  YYSYMBOL_NonEmptyKeySyms = 132,          /* NonEmptyKeySyms  */
  YYSYMBOL_KeySyms = 133,                  /* KeySyms  */
  YYSYMBOL_KeySym = 134,                   /* KeySym  */
  YYSYMBOL_KeySymLit = 135,                /* KeySymLit  */
  YYSYMBOL_SignedNumber = 136,             /* SignedNumber  */
  YYSYMBOL_Number = 137,                   /* Number  */
  YYSYMBOL_Float = 138,                    /* Float  */
  YYSYMBOL_Integer = 139,                  /* Integer  */
  YYSYMBOL_KeyCode = 140,                  /* KeyCode  */
  YYSYMBOL_Ident = 141,                    /* Ident  */
  YYSYMBOL_String = 142,                   /* String  */
  YYSYMBOL_OptMapName = 143,               /* OptMapName  */
  YYSYMBOL_MapName = 144                   /* MapName  */
};
typedef enum yysymbol_kind_t yysymbol_kind_t;




#ifdef short
# undef short
#endif

/* On compilers that do not define __PTRDIFF_MAX__ etc., make sure
   <limits.h> and (if available) <stdint.h> are included
   so that the code can choose integer types of a good width.  */

#ifndef __PTRDIFF_MAX__
# include <limits.h> /* INFRINGES ON USER NAME SPACE */
# if defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stdint.h> /* INFRINGES ON USER NAME SPACE */
#  define YY_STDINT_H
# endif
#endif

/* Narrow types that promote to a signed type and that can represent a
   signed or unsigned integer of at least N bits.  In tables they can
   save space and decrease cache pressure.  Promoting to a signed type
   helps avoid bugs in integer arithmetic.  */

#ifdef __INT_LEAST8_MAX__
typedef __INT_LEAST8_TYPE__ yytype_int8;
#elif defined YY_STDINT_H
typedef int_least8_t yytype_int8;
#else
typedef signed char yytype_int8;
#endif

#ifdef __INT_LEAST16_MAX__
typedef __INT_LEAST16_TYPE__ yytype_int16;
#elif defined YY_STDINT_H
typedef int_least16_t yytype_int16;
#else
typedef short yytype_int16;
#endif

/* Work around bug in HP-UX 11.23, which defines these macros
   incorrectly for preprocessor constants.  This workaround can likely
   be removed in 2023, as HPE has promised support for HP-UX 11.23
   (aka HP-UX 11i v2) only through the end of 2022; see Table 2 of
   <https://h20195.www2.hpe.com/V2/getpdf.aspx/4AA4-7673ENW.pdf>.  */
#ifdef __hpux
# undef UINT_LEAST8_MAX
# undef UINT_LEAST16_MAX
# define UINT_LEAST8_MAX 255
# define UINT_LEAST16_MAX 65535
#endif

#if defined __UINT_LEAST8_MAX__ && __UINT_LEAST8_MAX__ <= __INT_MAX__
typedef __UINT_LEAST8_TYPE__ yytype_uint8;
#elif (!defined __UINT_LEAST8_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST8_MAX <= INT_MAX)
typedef uint_least8_t yytype_uint8;
#elif !defined __UINT_LEAST8_MAX__ && UCHAR_MAX <= INT_MAX
typedef unsigned char yytype_uint8;
#else
typedef short yytype_uint8;
#endif

#if defined __UINT_LEAST16_MAX__ && __UINT_LEAST16_MAX__ <= __INT_MAX__
typedef __UINT_LEAST16_TYPE__ yytype_uint16;
#elif (!defined __UINT_LEAST16_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST16_MAX <= INT_MAX)
typedef uint_least16_t yytype_uint16;
#elif !defined __UINT_LEAST16_MAX__ && USHRT_MAX <= INT_MAX
typedef unsigned short yytype_uint16;
#else
typedef int yytype_uint16;
#endif

#ifndef YYPTRDIFF_T
# if defined __PTRDIFF_TYPE__ && defined __PTRDIFF_MAX__
#  define YYPTRDIFF_T __PTRDIFF_TYPE__
#  define YYPTRDIFF_MAXIMUM __PTRDIFF_MAX__
# elif defined PTRDIFF_MAX
#  ifndef ptrdiff_t
#   include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  endif
#  define YYPTRDIFF_T ptrdiff_t
#  define YYPTRDIFF_MAXIMUM PTRDIFF_MAX
# else
#  define YYPTRDIFF_T long
#  define YYPTRDIFF_MAXIMUM LONG_MAX
# endif
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned
# endif
#endif

#define YYSIZE_MAXIMUM                                  \
  YY_CAST (YYPTRDIFF_T,                                 \
           (YYPTRDIFF_MAXIMUM < YY_CAST (YYSIZE_T, -1)  \
            ? YYPTRDIFF_MAXIMUM                         \
            : YY_CAST (YYSIZE_T, -1)))

#define YYSIZEOF(X) YY_CAST (YYPTRDIFF_T, sizeof (X))


/* Stored state numbers (used for stacks). */
typedef yytype_int16 yy_state_t;

/* State numbers in computations.  */
typedef int yy_state_fast_t;

#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(Msgid) dgettext ("bison-runtime", Msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(Msgid) Msgid
# endif
#endif


#ifndef YY_ATTRIBUTE_PURE
# if defined __GNUC__ && 2 < __GNUC__ + (96 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_PURE __attribute__ ((__pure__))
# else
#  define YY_ATTRIBUTE_PURE
# endif
#endif

#ifndef YY_ATTRIBUTE_UNUSED
# if defined __GNUC__ && 2 < __GNUC__ + (7 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_UNUSED __attribute__ ((__unused__))
# else
#  define YY_ATTRIBUTE_UNUSED
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YY_USE(E) ((void) (E))
#else
# define YY_USE(E) /* empty */
#endif

/* Suppress an incorrect diagnostic about yylval being uninitialized.  */
#if defined __GNUC__ && ! defined __ICC && 406 <= __GNUC__ * 100 + __GNUC_MINOR__
# if __GNUC__ * 100 + __GNUC_MINOR__ < 407
#  define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                           \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")
# else
#  define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                           \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")              \
    _Pragma ("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
# endif
# define YY_IGNORE_MAYBE_UNINITIALIZED_END      \
    _Pragma ("GCC diagnostic pop")
#else
# define YY_INITIAL_VALUE(Value) Value
#endif
#ifndef YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_END
#endif
#ifndef YY_INITIAL_VALUE
# define YY_INITIAL_VALUE(Value) /* Nothing. */
#endif

#if defined __cplusplus && defined __GNUC__ && ! defined __ICC && 6 <= __GNUC__
# define YY_IGNORE_USELESS_CAST_BEGIN                          \
    _Pragma ("GCC diagnostic push")                            \
    _Pragma ("GCC diagnostic ignored \"-Wuseless-cast\"")
# define YY_IGNORE_USELESS_CAST_END            \
    _Pragma ("GCC diagnostic pop")
#endif
#ifndef YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_END
#endif


#define YY_ASSERT(E) ((void) (0 && (E)))

#if !defined yyoverflow

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   elif defined __BUILTIN_VA_ARG_INCR
#    include <alloca.h> /* INFRINGES ON USER NAME SPACE */
#   elif defined _AIX
#    define YYSTACK_ALLOC __alloca
#   elif defined _MSC_VER
#    include <malloc.h> /* INFRINGES ON USER NAME SPACE */
#    define alloca _alloca
#   else
#    define YYSTACK_ALLOC alloca
#    if ! defined _ALLOCA_H && ! defined EXIT_SUCCESS
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
      /* Use EXIT_SUCCESS as a witness for stdlib.h.  */
#     ifndef EXIT_SUCCESS
#      define EXIT_SUCCESS 0
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's 'empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (0)
#  ifndef YYSTACK_ALLOC_MAXIMUM
    /* The OS might guarantee only one guard page at the bottom of the stack,
       and a page size can be as small as 4096 bytes.  So we cannot safely
       invoke alloca (N) if N exceeds 4096.  Use a slightly smaller number
       to allow for a few compiler-allocated temporary stack slots.  */
#   define YYSTACK_ALLOC_MAXIMUM 4032 /* reasonable circa 2006 */
#  endif
# else
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
#  ifndef YYSTACK_ALLOC_MAXIMUM
#   define YYSTACK_ALLOC_MAXIMUM YYSIZE_MAXIMUM
#  endif
#  if (defined __cplusplus && ! defined EXIT_SUCCESS \
       && ! ((defined YYMALLOC || defined malloc) \
             && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef EXIT_SUCCESS
#    define EXIT_SUCCESS 0
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined EXIT_SUCCESS
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined EXIT_SUCCESS
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
# endif
#endif /* !defined yyoverflow */

#if (! defined yyoverflow \
     && (! defined __cplusplus \
         || (defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yy_state_t yyss_alloc;
  YYSTYPE yyvs_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (YYSIZEOF (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (YYSIZEOF (yy_state_t) + YYSIZEOF (YYSTYPE)) \
      + YYSTACK_GAP_MAXIMUM)

# define YYCOPY_NEEDED 1

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack_alloc, Stack)                           \
    do                                                                  \
      {                                                                 \
        YYPTRDIFF_T yynewbytes;                                         \
        YYCOPY (&yyptr->Stack_alloc, Stack, yysize);                    \
        Stack = &yyptr->Stack_alloc;                                    \
        yynewbytes = yystacksize * YYSIZEOF (*Stack) + YYSTACK_GAP_MAXIMUM; \
        yyptr += yynewbytes / YYSIZEOF (*yyptr);                        \
      }                                                                 \
    while (0)

#endif

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
/* Copy COUNT objects from SRC to DST.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(Dst, Src, Count) \
      __builtin_memcpy (Dst, Src, YY_CAST (YYSIZE_T, (Count)) * sizeof (*(Src)))
#  else
#   define YYCOPY(Dst, Src, Count)              \
      do                                        \
        {                                       \
          YYPTRDIFF_T yyi;                      \
          for (yyi = 0; yyi < (Count); yyi++)   \
            (Dst)[yyi] = (Src)[yyi];            \
        }                                       \
      while (0)
#  endif
# endif
#endif /* !YYCOPY_NEEDED */

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  16
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   876

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  65
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  80
/* YYNRULES -- Number of rules.  */
#define YYNRULES  209
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  368

/* YYMAXUTOK -- Last valid token kind.  */
#define YYMAXUTOK   257


/* YYTRANSLATE(TOKEN-NUM) -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex, with out-of-bounds checking.  */
#define YYTRANSLATE(YYX)                                \
  (0 <= (YYX) && (YYX) <= YYMAXUTOK                     \
   ? YY_CAST (yysymbol_kind_t, yytranslate[YYX])        \
   : YYSYMBOL_YYUNDEF)

/* YYTRANSLATE[TOKEN-NUM] -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex.  */
static const yytype_int8 yytranslate[] =
{
       0,     4,     5,     6,     7,     8,     9,    10,    11,     2,
      12,    13,    14,    15,    16,     2,     2,     2,     2,     2,
      17,    18,    19,    20,    21,    22,    23,    24,    25,    26,
      27,    28,    29,    30,    31,    32,    33,    34,    35,     2,
      36,    37,    38,    39,    40,    41,    42,    43,    44,    45,
      46,    47,    48,    49,    50,    51,     2,     2,     2,     2,
      52,    53,    54,    55,    56,     2,     2,     2,     2,     2,
      57,    58,    59,    60,    61,    62,    63,    64,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     3,     1,     2
};

#if YYDEBUG
/* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_int16 yyrline[] =
{
       0,   259,   259,   261,   263,   267,   273,   274,   275,   281,
     293,   296,   304,   305,   306,   307,   308,   311,   312,   315,
     316,   319,   320,   321,   322,   323,   324,   325,   326,   329,
     344,   354,   357,   363,   368,   373,   378,   383,   388,   393,
     398,   403,   408,   409,   410,   411,   418,   420,   422,   426,
     430,   434,   438,   440,   444,   446,   450,   456,   458,   462,
     474,   477,   483,   489,   490,   493,   495,   499,   500,   501,
     502,   503,   518,   520,   538,   540,   562,   568,   570,   572,
     575,   579,   583,   585,   589,   591,   595,   599,   601,   605,
     607,   611,   615,   616,   619,   621,   623,   625,   627,   631,
     632,   635,   636,   640,   641,   644,   646,   650,   654,   655,
     658,   661,   663,   667,   669,   671,   675,   677,   681,   685,
     689,   690,   691,   692,   695,   696,   699,   701,   703,   705,
     707,   709,   711,   713,   715,   717,   719,   723,   724,   727,
     728,   729,   730,   731,   743,   755,   757,   760,   762,   764,
     766,   768,   770,   774,   776,   778,   780,   782,   784,   786,
     788,   790,   794,   800,   802,   804,   808,   810,   814,   818,
     820,   824,   828,   830,   832,   834,   838,   840,   842,   844,
     848,   854,   856,   858,   862,   864,   871,   877,   889,   891,
     903,   905,   909,   911,   920,   933,   934,   984,   985,   988,
     989,   992,   995,   998,  1001,  1002,  1005,  1008,  1009,  1012
};
#endif

/** Accessing symbol of state STATE.  */
#define YY_ACCESSING_SYMBOL(State) YY_CAST (yysymbol_kind_t, yystos[State])

#if YYDEBUG || 0
/* The user-facing name of the symbol whose (internal) number is
   YYSYMBOL.  No bounds checking.  */
static const char *yysymbol_name (yysymbol_kind_t yysymbol) YY_ATTRIBUTE_UNUSED;

/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "END_OF_FILE", "error", "\"invalid token\"", "ERROR_TOK", "XKB_KEYMAP",
  "XKB_KEYCODES", "XKB_TYPES", "XKB_SYMBOLS", "XKB_COMPATMAP",
  "XKB_GEOMETRY", "XKB_SEMANTICS", "XKB_LAYOUT", "INCLUDE", "OVERRIDE",
  "AUGMENT", "REPLACE", "ALTERNATE", "VIRTUAL_MODS", "TYPE", "INTERPRET",
  "ACTION_TOK", "KEY", "ALIAS", "GROUP", "MODIFIER_MAP", "INDICATOR",
  "SHAPE", "KEYS", "ROW", "SECTION", "OVERLAY", "TEXT", "OUTLINE", "SOLID",
  "LOGO", "VIRTUAL", "EQUALS", "PLUS", "MINUS", "DIVIDE", "TIMES",
  "OBRACE", "CBRACE", "OPAREN", "CPAREN", "OBRACKET", "CBRACKET", "DOT",
  "COMMA", "SEMI", "EXCLAM", "INVERT", "STRING", "INTEGER", "FLOAT",
  "IDENT", "KEYNAME", "PARTIAL", "DEFAULT", "HIDDEN", "ALPHANUMERIC_KEYS",
  "MODIFIER_KEYS", "KEYPAD_KEYS", "FUNCTION_KEYS", "ALTERNATE_GROUP",
  "$accept", "XkbFile", "XkbCompositeMap", "XkbCompositeType",
  "XkbMapConfigList", "XkbMapConfig", "FileType", "OptFlags", "Flags",
  "Flag", "DeclList", "Decl", "VarDecl", "KeyNameDecl", "KeyAliasDecl",
  "VModDecl", "VModDefList", "VModDef", "InterpretDecl", "InterpretMatch",
  "VarDeclList", "KeyTypeDecl", "SymbolsDecl", "OptSymbolsBody",
  "SymbolsBody", "SymbolsVarDecl", "MultiKeySymOrActionList",
  "NoSymbolOrActionList", "GroupCompatDecl", "ModMapDecl",
  "KeyOrKeySymList", "KeyOrKeySym", "LedMapDecl", "LedNameDecl",
  "ShapeDecl", "SectionDecl", "SectionBody", "SectionBodyItem", "RowBody",
  "RowBodyItem", "Keys", "Key", "OverlayDecl", "OverlayKeyList",
  "OverlayKey", "OutlineList", "OutlineInList", "CoordList", "Coord",
  "DoodadDecl", "DoodadType", "FieldSpec", "Element", "OptMergeMode",
  "MergeMode", "ExprList", "Expr", "Term", "MultiActionList", "ActionList",
  "NonEmptyActions", "Actions", "Action", "Lhs", "Terminal",
  "MultiKeySymList", "KeySymList", "NonEmptyKeySyms", "KeySyms", "KeySym",
  "KeySymLit", "SignedNumber", "Number", "Float", "Integer", "KeyCode",
  "Ident", "String", "OptMapName", "MapName", YY_NULLPTR
};

static const char *
yysymbol_name (yysymbol_kind_t yysymbol)
{
  return yytname[yysymbol];
}
#endif

#define YYPACT_NINF (-267)

#define yypact_value_is_default(Yyn) \
  ((Yyn) == YYPACT_NINF)

#define YYTABLE_NINF (-205)

#define yytable_value_is_error(Yyn) \
  0

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
static const yytype_int16 yypact[] =
{
      48,  -267,  -267,  -267,  -267,  -267,  -267,  -267,  -267,  -267,
       7,  -267,  -267,   542,   765,  -267,  -267,  -267,  -267,  -267,
    -267,  -267,  -267,  -267,  -267,    10,    10,  -267,  -267,    52,
    -267,    59,  -267,  -267,   189,    44,    40,  -267,   287,  -267,
    -267,  -267,  -267,  -267,    66,  -267,   280,    72,  -267,  -267,
     -14,    90,    94,  -267,    89,   103,   139,   -14,     1,    90,
    -267,    90,   132,  -267,  -267,  -267,   194,   -14,  -267,   193,
    -267,  -267,  -267,  -267,  -267,  -267,  -267,  -267,  -267,  -267,
    -267,  -267,  -267,  -267,  -267,    90,    16,  -267,   204,   165,
    -267,   131,  -267,   226,  -267,   192,  -267,  -267,  -267,  -267,
     213,   231,  -267,  -267,   228,   234,   236,   232,   238,   240,
     242,   243,   139,   230,   254,   248,   363,   818,   363,  -267,
     -14,  -267,   363,  -267,  -267,   363,   606,   268,   363,    -6,
     363,  -267,    93,   486,   289,  -267,  -267,   277,  -267,  -267,
    -267,  -267,  -267,  -267,  -267,  -267,  -267,  -267,   363,   363,
     775,   363,   363,   363,  -267,  -267,   -25,   197,  -267,  -267,
    -267,   301,  -267,  -267,  -267,  -267,  -267,   298,   166,  -267,
     294,   634,   649,   294,    14,   -14,   306,   303,  -267,  -267,
     325,   -12,   308,   172,  -267,    31,  -267,  -267,   218,   677,
     318,    45,   102,  -267,   112,  -267,   329,    90,   326,    90,
    -267,  -267,   445,  -267,  -267,  -267,   363,  -267,   692,  -267,
    -267,  -267,  -267,   327,   113,  -267,   316,  -267,  -267,   363,
     363,   363,   363,   363,  -267,   363,   363,  -267,   319,  -267,
     320,   335,   503,  -267,   337,    55,    68,  -267,  -267,    83,
    -267,  -267,  -267,   341,   606,   321,  -267,  -267,   344,    -6,
    -267,   346,   122,   185,  -267,  -267,   348,  -267,   349,   -29,
     350,   318,   404,   733,   356,   353,  -267,   279,   354,   363,
    -267,   818,  -267,   -40,   294,   160,   160,  -267,  -267,   294,
     283,  -267,  -267,  -267,  -267,   125,  -267,  -267,   544,  -267,
     790,  -267,    23,  -267,  -267,  -267,   294,  -267,  -267,  -267,
    -267,  -267,    45,  -267,  -267,  -267,  -267,   747,   294,   364,
    -267,   563,  -267,   351,  -267,  -267,  -267,    26,  -267,  -267,
     363,  -267,  -267,   123,   591,   176,   182,  -267,  -267,    39,
    -267,  -267,  -267,   362,   146,   -28,   360,  -267,   374,   149,
    -267,  -267,   294,  -267,  -267,  -267,  -267,  -267,  -267,  -267,
    -267,   363,  -267,   159,  -267,  -267,   355,   371,   351,   178,
     377,   -28,  -267,  -267,  -267,  -267,  -267,  -267
};

/* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
   Performed when YYTABLE does not specify something else to do.  Zero
   means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
      18,     4,    21,    22,    23,    24,    25,    26,    27,    28,
       0,     2,     3,     0,    17,    20,     1,     6,    12,    13,
      15,    14,    16,     7,     8,   208,   208,    19,   209,     0,
     207,     0,    10,    31,    18,   138,     0,     9,     0,   139,
     141,   140,   142,   143,     0,    29,     0,   137,     5,    11,
       0,   128,   127,   126,   129,     0,   130,   131,   132,   133,
     134,   135,   136,   121,   122,   123,     0,     0,   204,     0,
     205,    32,    34,    35,    30,    33,    36,    37,    39,    38,
      40,    41,    42,    43,    44,     0,   172,   125,     0,   124,
      45,     0,    53,    54,   206,     0,   195,   193,   202,   194,
       0,    58,   192,   196,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    47,
       0,    51,     0,    60,    60,     0,    64,     0,     0,     0,
       0,    60,     0,     0,     0,    48,   203,     0,    60,   128,
     127,   129,   130,   131,   132,   133,   135,   136,     0,     0,
       0,     0,     0,     0,   201,   179,   172,     0,   152,   169,
     159,   157,   160,   178,   177,   124,   176,   173,     0,    52,
      55,     0,     0,    57,    79,     0,     0,    63,    66,    71,
       0,   124,     0,     0,    84,     0,    83,    85,     0,     0,
       0,     0,     0,   112,     0,   117,     0,   132,   134,     0,
      95,    97,     0,    93,    98,    96,     0,    49,     0,   154,
     157,   153,   170,     0,     0,   167,     0,   155,   156,   146,
       0,     0,     0,     0,   174,     0,     0,    46,     0,    59,
       0,   195,     0,   189,   194,     0,     0,   165,   164,     0,
     183,   182,    70,     0,     0,     0,    50,    80,     0,     0,
      87,     0,     0,     0,   200,   199,     0,   198,     0,     0,
       0,     0,     0,     0,     0,     0,    92,     0,     0,   146,
     168,     0,   161,     0,   145,   148,   149,   147,   150,   151,
       0,    61,    56,    78,   187,     0,   186,    76,     0,    74,
       0,    72,     0,    62,    65,    68,    67,    81,    82,    86,
     113,   197,     0,    89,   111,    90,   116,     0,   115,     0,
     102,     0,   100,     0,    91,    88,   119,     0,   166,   158,
       0,   175,   188,     0,     0,     0,     0,   163,   162,     0,
     190,   181,   180,     0,     0,     0,     0,    99,     0,     0,
     109,   171,   144,   185,   184,    77,    75,    73,   191,   118,
     114,   146,   105,     0,   104,    94,     0,     0,     0,     0,
       0,     0,   110,   107,   108,   106,   101,   103
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -267,  -267,  -267,  -267,  -267,   378,  -267,   397,  -267,   420,
    -267,  -267,   -45,  -267,  -267,  -267,  -267,   317,  -267,  -267,
      58,  -267,  -267,  -267,  -267,   195,   191,  -267,  -267,  -267,
    -267,   199,   392,  -267,  -267,  -267,  -267,   241,  -267,   129,
    -267,    85,  -267,  -267,    86,  -267,   190,  -188,   200,   405,
    -267,   -46,  -267,  -267,  -267,  -266,    65,   111,   162,  -267,
    -168,   163,  -158,   -31,  -267,   164,  -267,   175,  -267,   428,
    -155,   170,   229,  -267,   -47,  -267,   -36,   -34,   455,  -267
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
       0,    10,    11,    25,    34,    12,    26,    13,    14,    15,
      35,    45,   229,    72,    73,    74,    91,    92,    75,   100,
     171,    76,    77,   176,   177,   178,   179,   235,    78,    79,
     185,   186,   201,    81,    82,    83,   202,   203,   311,   312,
     353,   354,   204,   339,   340,   192,   193,   194,   195,   205,
      85,   156,    87,    46,    47,   273,   274,   158,   236,   214,
     159,   160,   215,   161,   162,   239,   285,   240,   331,   187,
     102,   256,   257,   163,   164,   137,   165,   166,    29,    30
};

/* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule whose
   number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_int16 yytable[] =
{
      86,    71,   252,   317,   319,   103,   237,    16,   320,   106,
      89,   108,   190,   351,    93,    88,   238,    95,   219,   241,
     116,   107,   117,    96,   109,   110,    68,   111,   352,    70,
     -69,   113,   139,   140,    53,   141,   -69,   142,   143,   144,
     145,    68,    60,   231,    70,   147,    97,    98,     1,    99,
     184,   115,    96,    94,    98,   232,    39,    40,    41,    42,
      43,   116,    28,   117,   329,   134,   233,    98,    96,   234,
     341,   167,    70,   248,   320,   233,    98,   286,    99,   249,
      86,   348,   103,   253,    93,   359,    44,    86,   200,    48,
     181,   284,    98,    32,    99,   180,   196,    89,   254,   255,
      33,   287,    88,   288,   213,     2,     3,     4,     5,     6,
       7,     8,     9,   318,   289,    49,   290,   210,   210,   334,
     237,   210,   210,    96,    90,    86,    86,   103,   213,   291,
     238,   292,   328,   241,   190,    89,    89,   332,   191,   242,
      88,    88,    94,    86,   258,   104,    97,    98,    68,    99,
     259,    70,    96,    89,   260,   270,    86,   200,    88,   105,
     261,   271,    86,   109,   300,   264,    89,   322,   344,   286,
     261,    88,    89,   323,   286,   343,    98,    88,    99,   120,
     121,   157,   172,   168,  -120,   103,   213,   170,   350,   189,
     173,   357,    98,   183,   261,   188,   208,   358,    86,   222,
     223,   360,   103,   220,   221,   222,   223,   361,   181,   220,
     221,   222,   223,   180,   119,   227,   216,    86,   310,   112,
     365,   247,   346,   196,   290,   213,   320,    89,   347,   114,
     292,    36,    88,   123,   220,   221,   222,   223,   254,   255,
     118,   103,   213,   224,   213,   103,     2,     3,     4,     5,
       6,     7,     8,     9,   124,   220,   221,   222,   223,   209,
     211,   213,   122,   217,   218,    86,   310,   250,   125,   126,
     127,   267,   128,   129,   130,    89,   103,   103,   213,   135,
      88,   131,   103,   132,   133,   275,   276,   277,   278,   138,
     279,   280,    18,    19,    20,    21,    22,    50,    51,    52,
      53,    54,    55,    56,    57,    58,    59,   136,    60,    61,
     296,    62,    63,    64,    65,    66,   220,   221,   222,   223,
     220,   221,   222,   223,   182,   206,   207,   308,   315,   321,
      67,   220,   221,   222,   223,    68,    69,   225,    70,   139,
     140,    53,   141,   226,   142,   143,   144,   145,   243,    60,
     146,   244,   147,   220,   221,   222,   223,   246,   148,   149,
     272,   245,   150,   191,   151,   262,   174,   263,   281,   282,
     269,   152,   153,    94,    98,   154,    68,   155,  -135,    70,
    -204,   139,   140,    53,   141,   342,   142,   143,   144,   145,
     293,    60,   146,   297,   147,   299,   302,   313,   303,   305,
     148,   149,   314,   316,   150,   335,   151,   338,   349,   355,
     356,   362,    37,   152,   153,    94,    98,   154,    68,   155,
     363,    70,   139,   140,    53,   141,   366,   142,   143,   144,
     145,    38,    60,   146,    27,   147,   295,   169,    80,   294,
     337,   148,   149,   266,   364,   307,   367,   151,   298,   304,
     325,    84,   326,   327,   152,   153,    94,    98,   154,    68,
     155,   306,    70,   139,   140,    53,   141,   330,   142,   143,
     197,   145,   333,   198,   146,   199,    62,    63,    64,    65,
     101,    31,   301,     0,     0,     0,     0,   265,     0,     0,
       0,     0,     0,     0,     0,    67,     0,     0,     0,     0,
      68,     0,     0,    70,   139,   140,    53,   141,     0,   142,
     143,   197,   145,     0,   198,   146,   199,    62,    63,    64,
      65,   139,   140,    53,   141,     0,   142,   143,   144,   145,
       0,    60,   231,     0,   147,     0,    67,     0,     0,     0,
       0,    68,     0,     0,    70,   283,    17,    18,    19,    20,
      21,    22,    23,    24,     0,   284,    98,     0,   234,     0,
       0,    70,   139,   140,    53,   141,     0,   142,   143,   144,
     145,     0,    60,   231,     0,   147,     0,     0,     0,     0,
       0,   139,   140,    53,   141,   324,   142,   143,   144,   145,
     309,    60,   146,     0,   147,     0,   233,    98,     0,   234,
       0,     0,    70,     0,     0,   336,     0,     0,     0,   139,
     140,    53,   141,    67,   142,   143,   144,   145,    68,    60,
     231,    70,   147,     0,   139,   140,    53,   141,     0,   142,
     143,   144,   145,   345,    60,   146,     0,   147,     0,     0,
       0,     0,     0,   284,    98,     0,   234,     0,     0,    70,
       0,   174,   139,   140,    53,   141,   175,   142,   143,   144,
     145,    68,    60,   146,    70,   147,     0,   139,   140,    53,
     141,     0,   142,   143,   144,   145,   228,    60,   146,     0,
     147,     0,     0,     0,    67,     0,     0,     0,     0,    68,
       0,   230,    70,     0,     0,   139,   140,    53,   141,    67,
     142,   143,   144,   145,    68,    60,   146,    70,   147,     0,
     139,   140,    53,   141,     0,   142,   143,   144,   145,   251,
      60,   146,     0,   147,     0,     0,     0,    67,     0,     0,
       0,     0,    68,     0,   268,    70,     0,     0,     0,     0,
       0,     0,    67,     0,     0,     0,     0,    68,     0,     0,
      70,   139,   140,    53,   141,     0,   142,   143,   144,   145,
     309,    60,   146,     0,   147,   139,   140,    53,   141,     0,
     142,   143,   144,   145,     0,    60,   146,     0,   147,     0,
       0,     0,     0,    67,     0,     0,     0,     0,    68,   212,
       0,    70,   191,   139,   140,    53,   141,     0,   142,   143,
     144,   145,    68,    60,   146,    70,   147,     0,   139,   140,
      53,   141,     0,   142,   143,   144,   145,   212,    60,   146,
       0,   147,     2,     3,     4,     5,     6,     7,     8,     9,
      68,   150,     0,    70,     0,     0,   139,   140,    53,   141,
       0,   142,   143,   144,   145,    68,    60,   146,    70,   147,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    68,     0,     0,    70
};

static const yytype_int16 yycheck[] =
{
      46,    46,   190,   269,    44,    52,   174,     0,    48,    56,
      46,    58,    41,    41,    50,    46,   174,    51,    43,   174,
      45,    57,    47,    29,    58,    59,    55,    61,    56,    58,
      42,    67,    18,    19,    20,    21,    48,    23,    24,    25,
      26,    55,    28,    29,    58,    31,    52,    53,     0,    55,
      56,    85,    29,    52,    53,    41,    12,    13,    14,    15,
      16,    45,    52,    47,    41,   112,    52,    53,    29,    55,
      44,   117,    58,    42,    48,    52,    53,   232,    55,    48,
     126,    42,   129,    38,   120,   351,    42,   133,   133,    49,
     126,    52,    53,    41,    55,   126,   132,   133,    53,    54,
      41,    46,   133,    48,   150,    57,    58,    59,    60,    61,
      62,    63,    64,   271,    46,    49,    48,   148,   149,   307,
     288,   152,   153,    29,    52,   171,   172,   174,   174,    46,
     288,    48,   290,   288,    41,   171,   172,   292,    45,   175,
     171,   172,    52,   189,    42,    56,    52,    53,    55,    55,
      48,    58,    29,   189,    42,    42,   202,   202,   189,    56,
      48,    48,   208,   197,    42,   199,   202,    42,   323,   324,
      48,   202,   208,    48,   329,    52,    53,   208,    55,    48,
      49,   116,   124,   118,    52,   232,   232,   122,    42,   131,
     125,    42,    53,   128,    48,   130,   138,    48,   244,    39,
      40,    42,   249,    37,    38,    39,    40,    48,   244,    37,
      38,    39,    40,   244,    49,    49,   151,   263,   263,    25,
      42,    49,    46,   259,    48,   271,    48,   263,    46,    36,
      48,    42,   263,    41,    37,    38,    39,    40,    53,    54,
      36,   288,   288,    46,   290,   292,    57,    58,    59,    60,
      61,    62,    63,    64,    41,    37,    38,    39,    40,   148,
     149,   307,    36,   152,   153,   311,   311,    49,    37,    41,
      36,   206,    36,    41,    36,   311,   323,   324,   324,    49,
     311,    41,   329,    41,    41,   220,   221,   222,   223,    41,
     225,   226,     5,     6,     7,     8,     9,    17,    18,    19,
      20,    21,    22,    23,    24,    25,    26,    53,    28,    29,
     245,    31,    32,    33,    34,    35,    37,    38,    39,    40,
      37,    38,    39,    40,    56,    36,    49,   262,    49,    46,
      50,    37,    38,    39,    40,    55,    56,    36,    58,    18,
      19,    20,    21,    45,    23,    24,    25,    26,    42,    28,
      29,    48,    31,    37,    38,    39,    40,    49,    37,    38,
      44,    36,    41,    45,    43,    36,    45,    41,    49,    49,
      43,    50,    51,    52,    53,    54,    55,    56,    43,    58,
      43,    18,    19,    20,    21,   320,    23,    24,    25,    26,
      49,    28,    29,    49,    31,    49,    48,    41,    49,    49,
      37,    38,    49,    49,    41,    41,    43,    56,    46,    49,
      36,    56,    34,    50,    51,    52,    53,    54,    55,    56,
      49,    58,    18,    19,    20,    21,    49,    23,    24,    25,
      26,    34,    28,    29,    14,    31,   245,   120,    46,   244,
     311,    37,    38,   202,   358,    41,   361,    43,   249,   259,
     288,    46,   288,   290,    50,    51,    52,    53,    54,    55,
      56,   261,    58,    18,    19,    20,    21,   292,    23,    24,
      25,    26,   302,    28,    29,    30,    31,    32,    33,    34,
      52,    26,   253,    -1,    -1,    -1,    -1,    42,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    50,    -1,    -1,    -1,    -1,
      55,    -1,    -1,    58,    18,    19,    20,    21,    -1,    23,
      24,    25,    26,    -1,    28,    29,    30,    31,    32,    33,
      34,    18,    19,    20,    21,    -1,    23,    24,    25,    26,
      -1,    28,    29,    -1,    31,    -1,    50,    -1,    -1,    -1,
      -1,    55,    -1,    -1,    58,    42,     4,     5,     6,     7,
       8,     9,    10,    11,    -1,    52,    53,    -1,    55,    -1,
      -1,    58,    18,    19,    20,    21,    -1,    23,    24,    25,
      26,    -1,    28,    29,    -1,    31,    -1,    -1,    -1,    -1,
      -1,    18,    19,    20,    21,    41,    23,    24,    25,    26,
      27,    28,    29,    -1,    31,    -1,    52,    53,    -1,    55,
      -1,    -1,    58,    -1,    -1,    42,    -1,    -1,    -1,    18,
      19,    20,    21,    50,    23,    24,    25,    26,    55,    28,
      29,    58,    31,    -1,    18,    19,    20,    21,    -1,    23,
      24,    25,    26,    42,    28,    29,    -1,    31,    -1,    -1,
      -1,    -1,    -1,    52,    53,    -1,    55,    -1,    -1,    58,
      -1,    45,    18,    19,    20,    21,    50,    23,    24,    25,
      26,    55,    28,    29,    58,    31,    -1,    18,    19,    20,
      21,    -1,    23,    24,    25,    26,    42,    28,    29,    -1,
      31,    -1,    -1,    -1,    50,    -1,    -1,    -1,    -1,    55,
      -1,    42,    58,    -1,    -1,    18,    19,    20,    21,    50,
      23,    24,    25,    26,    55,    28,    29,    58,    31,    -1,
      18,    19,    20,    21,    -1,    23,    24,    25,    26,    42,
      28,    29,    -1,    31,    -1,    -1,    -1,    50,    -1,    -1,
      -1,    -1,    55,    -1,    42,    58,    -1,    -1,    -1,    -1,
      -1,    -1,    50,    -1,    -1,    -1,    -1,    55,    -1,    -1,
      58,    18,    19,    20,    21,    -1,    23,    24,    25,    26,
      27,    28,    29,    -1,    31,    18,    19,    20,    21,    -1,
      23,    24,    25,    26,    -1,    28,    29,    -1,    31,    -1,
      -1,    -1,    -1,    50,    -1,    -1,    -1,    -1,    55,    42,
      -1,    58,    45,    18,    19,    20,    21,    -1,    23,    24,
      25,    26,    55,    28,    29,    58,    31,    -1,    18,    19,
      20,    21,    -1,    23,    24,    25,    26,    42,    28,    29,
      -1,    31,    57,    58,    59,    60,    61,    62,    63,    64,
      55,    41,    -1,    58,    -1,    -1,    18,    19,    20,    21,
      -1,    23,    24,    25,    26,    55,    28,    29,    58,    31,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    55,    -1,    -1,    58
};

/* YYSTOS[STATE-NUM] -- The symbol kind of the accessing symbol of
   state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,     0,    57,    58,    59,    60,    61,    62,    63,    64,
      66,    67,    70,    72,    73,    74,     0,     4,     5,     6,
       7,     8,     9,    10,    11,    68,    71,    74,    52,   143,
     144,   143,    41,    41,    69,    75,    42,    70,    72,    12,
      13,    14,    15,    16,    42,    76,   118,   119,    49,    49,
      17,    18,    19,    20,    21,    22,    23,    24,    25,    26,
      28,    29,    31,    32,    33,    34,    35,    50,    55,    56,
      58,    77,    78,    79,    80,    83,    86,    87,    93,    94,
      97,    98,    99,   100,   114,   115,   116,   117,   128,   141,
      52,    81,    82,   141,    52,   142,    29,    52,    53,    55,
      84,   134,   135,   139,    56,    56,   139,   141,   139,   142,
     142,   142,    25,   141,    36,   142,    45,    47,    36,    49,
      48,    49,    36,    41,    41,    37,    41,    36,    36,    41,
      36,    41,    41,    41,   139,    49,    53,   140,    41,    18,
      19,    21,    23,    24,    25,    26,    29,    31,    37,    38,
      41,    43,    50,    51,    54,    56,   116,   121,   122,   125,
     126,   128,   129,   138,   139,   141,   142,   116,   121,    82,
     121,    85,    85,   121,    45,    50,    88,    89,    90,    91,
     128,   141,    56,   121,    56,    95,    96,   134,   121,    85,
      41,    45,   110,   111,   112,   113,   141,    25,    28,    30,
      77,    97,   101,   102,   107,   114,    36,    49,    85,   122,
     128,   122,    42,   116,   124,   127,   121,   122,   122,    43,
      37,    38,    39,    40,    46,    36,    45,    49,    42,    77,
      42,    29,    41,    52,    55,    92,   123,   125,   127,   130,
     132,   135,   141,    42,    48,    36,    49,    49,    42,    48,
      49,    42,   112,    38,    53,    54,   136,   137,    42,    48,
      42,    48,    36,    41,   142,    42,   102,   121,    42,    43,
      42,    48,    44,   120,   121,   121,   121,   121,   121,   121,
     121,    49,    49,    42,    52,   131,   135,    46,    48,    46,
      48,    46,    48,    49,    90,    91,   121,    49,    96,    49,
      42,   137,    48,    49,   111,    49,   113,    41,   121,    27,
      77,   103,   104,    41,    49,    49,    49,   120,   127,    44,
      48,    46,    42,    48,    41,   123,   130,   126,   127,    41,
     132,   133,   135,   136,   112,    41,    42,   104,    56,   108,
     109,    44,   121,    52,   135,    42,    46,    46,    42,    46,
      42,    41,    56,   105,   106,    49,    36,    42,    48,   120,
      42,    48,    56,    49,   109,    42,    49,   106
};

/* YYR1[RULE-NUM] -- Symbol kind of the left-hand side of rule RULE-NUM.  */
static const yytype_uint8 yyr1[] =
{
       0,    65,    66,    66,    66,    67,    68,    68,    68,    69,
      69,    70,    71,    71,    71,    71,    71,    72,    72,    73,
      73,    74,    74,    74,    74,    74,    74,    74,    74,    75,
      75,    75,    76,    76,    76,    76,    76,    76,    76,    76,
      76,    76,    76,    76,    76,    76,    77,    77,    77,    78,
      79,    80,    81,    81,    82,    82,    83,    84,    84,    85,
      85,    86,    87,    88,    88,    89,    89,    90,    90,    90,
      90,    90,    91,    91,    91,    91,    91,    92,    92,    92,
      93,    94,    95,    95,    96,    96,    97,    98,    98,    99,
      99,   100,   101,   101,   102,   102,   102,   102,   102,   103,
     103,   104,   104,   105,   105,   106,   106,   107,   108,   108,
     109,   110,   110,   111,   111,   111,   112,   112,   113,   114,
     115,   115,   115,   115,   116,   116,   117,   117,   117,   117,
     117,   117,   117,   117,   117,   117,   117,   118,   118,   119,
     119,   119,   119,   119,   120,   120,   120,   121,   121,   121,
     121,   121,   121,   122,   122,   122,   122,   122,   122,   122,
     122,   122,   123,   123,   123,   123,   124,   124,   125,   126,
     126,   127,   128,   128,   128,   128,   129,   129,   129,   129,
     130,   130,   130,   130,   131,   131,   131,   131,   132,   132,
     133,   133,   134,   134,   135,   135,   135,   136,   136,   137,
     137,   138,   139,   140,   141,   141,   142,   143,   143,   144
};

/* YYR2[RULE-NUM] -- Number of symbols on the right-hand side of rule RULE-NUM.  */
static const yytype_int8 yyr2[] =
{
       0,     2,     1,     1,     1,     7,     1,     1,     1,     2,
       0,     7,     1,     1,     1,     1,     1,     1,     0,     2,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     2,
       3,     0,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     4,     2,     3,     4,
       5,     3,     3,     1,     1,     3,     6,     3,     1,     2,
       0,     6,     6,     1,     0,     3,     1,     3,     3,     1,
       2,     1,     3,     5,     3,     5,     3,     4,     2,     0,
       5,     6,     3,     1,     1,     1,     6,     5,     6,     6,
       6,     6,     2,     1,     5,     1,     1,     1,     1,     2,
       1,     5,     1,     3,     1,     1,     3,     6,     3,     1,
       3,     3,     1,     3,     5,     3,     3,     1,     5,     6,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     0,     1,
       1,     1,     1,     1,     3,     1,     0,     3,     3,     3,
       3,     3,     1,     2,     2,     2,     2,     1,     4,     1,
       1,     3,     3,     3,     1,     1,     3,     1,     3,     1,
       2,     4,     1,     3,     4,     6,     1,     1,     1,     1,
       3,     3,     1,     1,     3,     3,     1,     1,     3,     1,
       1,     2,     1,     1,     1,     1,     1,     2,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     0,     1
};


enum { YYENOMEM = -2 };

#define yyerrok         (yyerrstatus = 0)
#define yyclearin       (yychar = YYEMPTY)

#define YYACCEPT        goto yyacceptlab
#define YYABORT         goto yyabortlab
#define YYERROR         goto yyerrorlab
#define YYNOMEM         goto yyexhaustedlab


#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)                                    \
  do                                                              \
    if (yychar == YYEMPTY)                                        \
      {                                                           \
        yychar = (Token);                                         \
        yylval = (Value);                                         \
        YYPOPSTACK (yylen);                                       \
        yystate = *yyssp;                                         \
        goto yybackup;                                            \
      }                                                           \
    else                                                          \
      {                                                           \
        yyerror (param, YY_("syntax error: cannot back up")); \
        YYERROR;                                                  \
      }                                                           \
  while (0)

/* Backward compatibility with an undocumented macro.
   Use YYerror or YYUNDEF. */
#define YYERRCODE YYUNDEF


/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)                        \
do {                                            \
  if (yydebug)                                  \
    YYFPRINTF Args;                             \
} while (0)




# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)                    \
do {                                                                      \
  if (yydebug)                                                            \
    {                                                                     \
      YYFPRINTF (stderr, "%s ", Title);                                   \
      yy_symbol_print (stderr,                                            \
                  Kind, Value, param); \
      YYFPRINTF (stderr, "\n");                                           \
    }                                                                     \
} while (0)


/*-----------------------------------.
| Print this symbol's value on YYO.  |
`-----------------------------------*/

static void
yy_symbol_value_print (FILE *yyo,
                       yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep, struct parser_param *param)
{
  FILE *yyoutput = yyo;
  YY_USE (yyoutput);
  YY_USE (param);
  if (!yyvaluep)
    return;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YY_USE (yykind);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}


/*---------------------------.
| Print this symbol on YYO.  |
`---------------------------*/

static void
yy_symbol_print (FILE *yyo,
                 yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep, struct parser_param *param)
{
  YYFPRINTF (yyo, "%s %s (",
             yykind < YYNTOKENS ? "token" : "nterm", yysymbol_name (yykind));

  yy_symbol_value_print (yyo, yykind, yyvaluep, param);
  YYFPRINTF (yyo, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

static void
yy_stack_print (yy_state_t *yybottom, yy_state_t *yytop)
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)                            \
do {                                                            \
  if (yydebug)                                                  \
    yy_stack_print ((Bottom), (Top));                           \
} while (0)


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

static void
yy_reduce_print (yy_state_t *yyssp, YYSTYPE *yyvsp,
                 int yyrule, struct parser_param *param)
{
  int yylno = yyrline[yyrule];
  int yynrhs = yyr2[yyrule];
  int yyi;
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %d):\n",
             yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr,
                       YY_ACCESSING_SYMBOL (+yyssp[yyi + 1 - yynrhs]),
                       &yyvsp[(yyi + 1) - (yynrhs)], param);
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)          \
do {                                    \
  if (yydebug)                          \
    yy_reduce_print (yyssp, yyvsp, Rule, param); \
} while (0)

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args) ((void) 0)
# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !YYDEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   YYSTACK_ALLOC_MAXIMUM < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif






/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

static void
yydestruct (const char *yymsg,
            yysymbol_kind_t yykind, YYSTYPE *yyvaluep, struct parser_param *param)
{
  YY_USE (yyvaluep);
  YY_USE (param);
  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yykind, yyvaluep, yylocationp);

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  switch (yykind)
    {
    case YYSYMBOL_STRING: /* STRING  */
#line 243 "parser.y"
            { free(((*yyvaluep).str)); }
#line 1383 "parser.c"
        break;

    case YYSYMBOL_XkbFile: /* XkbFile  */
#line 241 "parser.y"
            { if (!param->rtrn) FreeXkbFile(((*yyvaluep).file)); }
#line 1389 "parser.c"
        break;

    case YYSYMBOL_XkbCompositeMap: /* XkbCompositeMap  */
#line 241 "parser.y"
            { if (!param->rtrn) FreeXkbFile(((*yyvaluep).file)); }
#line 1395 "parser.c"
        break;

    case YYSYMBOL_XkbMapConfigList: /* XkbMapConfigList  */
#line 242 "parser.y"
            { FreeXkbFile(((*yyvaluep).fileList).head); }
#line 1401 "parser.c"
        break;

    case YYSYMBOL_XkbMapConfig: /* XkbMapConfig  */
#line 241 "parser.y"
            { if (!param->rtrn) FreeXkbFile(((*yyvaluep).file)); }
#line 1407 "parser.c"
        break;

    case YYSYMBOL_DeclList: /* DeclList  */
#line 237 "parser.y"
            { FreeStmt((ParseCommon *) ((*yyvaluep).anyList).head); }
#line 1413 "parser.c"
        break;

    case YYSYMBOL_Decl: /* Decl  */
#line 234 "parser.y"
            { FreeStmt((ParseCommon *) ((*yyvaluep).any)); }
#line 1419 "parser.c"
        break;

    case YYSYMBOL_VarDecl: /* VarDecl  */
#line 234 "parser.y"
            { FreeStmt((ParseCommon *) ((*yyvaluep).var)); }
#line 1425 "parser.c"
        break;

    case YYSYMBOL_KeyNameDecl: /* KeyNameDecl  */
#line 234 "parser.y"
            { FreeStmt((ParseCommon *) ((*yyvaluep).keyCode)); }
#line 1431 "parser.c"
        break;

    case YYSYMBOL_KeyAliasDecl: /* KeyAliasDecl  */
#line 234 "parser.y"
            { FreeStmt((ParseCommon *) ((*yyvaluep).keyAlias)); }
#line 1437 "parser.c"
        break;

    case YYSYMBOL_VModDecl: /* VModDecl  */
#line 237 "parser.y"
            { FreeStmt((ParseCommon *) ((*yyvaluep).vmodList).head); }
#line 1443 "parser.c"
        break;

    case YYSYMBOL_VModDefList: /* VModDefList  */
#line 237 "parser.y"
            { FreeStmt((ParseCommon *) ((*yyvaluep).vmodList).head); }
#line 1449 "parser.c"
        break;

    case YYSYMBOL_VModDef: /* VModDef  */
#line 234 "parser.y"
            { FreeStmt((ParseCommon *) ((*yyvaluep).vmod)); }
#line 1455 "parser.c"
        break;

    case YYSYMBOL_InterpretDecl: /* InterpretDecl  */
#line 234 "parser.y"
            { FreeStmt((ParseCommon *) ((*yyvaluep).interp)); }
#line 1461 "parser.c"
        break;

    case YYSYMBOL_InterpretMatch: /* InterpretMatch  */
#line 234 "parser.y"
            { FreeStmt((ParseCommon *) ((*yyvaluep).interp)); }
#line 1467 "parser.c"
        break;

    case YYSYMBOL_VarDeclList: /* VarDeclList  */
#line 237 "parser.y"
            { FreeStmt((ParseCommon *) ((*yyvaluep).varList).head); }
#line 1473 "parser.c"
        break;

    case YYSYMBOL_KeyTypeDecl: /* KeyTypeDecl  */
#line 234 "parser.y"
            { FreeStmt((ParseCommon *) ((*yyvaluep).keyType)); }
#line 1479 "parser.c"
        break;

    case YYSYMBOL_SymbolsDecl: /* SymbolsDecl  */
#line 234 "parser.y"
            { FreeStmt((ParseCommon *) ((*yyvaluep).syms)); }
#line 1485 "parser.c"
        break;

    case YYSYMBOL_OptSymbolsBody: /* OptSymbolsBody  */
#line 237 "parser.y"
            { FreeStmt((ParseCommon *) ((*yyvaluep).varList).head); }
#line 1491 "parser.c"
        break;

    case YYSYMBOL_SymbolsBody: /* SymbolsBody  */
#line 237 "parser.y"
            { FreeStmt((ParseCommon *) ((*yyvaluep).varList).head); }
#line 1497 "parser.c"
        break;

    case YYSYMBOL_SymbolsVarDecl: /* SymbolsVarDecl  */
#line 234 "parser.y"
            { FreeStmt((ParseCommon *) ((*yyvaluep).var)); }
#line 1503 "parser.c"
        break;

    case YYSYMBOL_MultiKeySymOrActionList: /* MultiKeySymOrActionList  */
#line 234 "parser.y"
            { FreeStmt((ParseCommon *) ((*yyvaluep).expr)); }
#line 1509 "parser.c"
        break;

    case YYSYMBOL_GroupCompatDecl: /* GroupCompatDecl  */
#line 234 "parser.y"
            { FreeStmt((ParseCommon *) ((*yyvaluep).groupCompat)); }
#line 1515 "parser.c"
        break;

    case YYSYMBOL_ModMapDecl: /* ModMapDecl  */
#line 234 "parser.y"
            { FreeStmt((ParseCommon *) ((*yyvaluep).modMask)); }
#line 1521 "parser.c"
        break;

    case YYSYMBOL_KeyOrKeySymList: /* KeyOrKeySymList  */
#line 237 "parser.y"
            { FreeStmt((ParseCommon *) ((*yyvaluep).exprList).head); }
#line 1527 "parser.c"
        break;

    case YYSYMBOL_KeyOrKeySym: /* KeyOrKeySym  */
#line 234 "parser.y"
            { FreeStmt((ParseCommon *) ((*yyvaluep).expr)); }
#line 1533 "parser.c"
        break;

    case YYSYMBOL_LedMapDecl: /* LedMapDecl  */
#line 234 "parser.y"
            { FreeStmt((ParseCommon *) ((*yyvaluep).ledMap)); }
#line 1539 "parser.c"
        break;

    case YYSYMBOL_LedNameDecl: /* LedNameDecl  */
#line 234 "parser.y"
            { FreeStmt((ParseCommon *) ((*yyvaluep).ledName)); }
#line 1545 "parser.c"
        break;

    case YYSYMBOL_CoordList: /* CoordList  */
#line 234 "parser.y"
            { FreeStmt((ParseCommon *) ((*yyvaluep).expr)); }
#line 1551 "parser.c"
        break;

    case YYSYMBOL_Coord: /* Coord  */
#line 234 "parser.y"
            { FreeStmt((ParseCommon *) ((*yyvaluep).expr)); }
#line 1557 "parser.c"
        break;

    case YYSYMBOL_ExprList: /* ExprList  */
#line 237 "parser.y"
            { FreeStmt((ParseCommon *) ((*yyvaluep).exprList).head); }
#line 1563 "parser.c"
        break;

    case YYSYMBOL_Expr: /* Expr  */
#line 234 "parser.y"
            { FreeStmt((ParseCommon *) ((*yyvaluep).expr)); }
#line 1569 "parser.c"
        break;

    case YYSYMBOL_Term: /* Term  */
#line 234 "parser.y"
            { FreeStmt((ParseCommon *) ((*yyvaluep).expr)); }
#line 1575 "parser.c"
        break;

    case YYSYMBOL_MultiActionList: /* MultiActionList  */
#line 237 "parser.y"
            { FreeStmt((ParseCommon *) ((*yyvaluep).exprList).head); }
#line 1581 "parser.c"
        break;

    case YYSYMBOL_ActionList: /* ActionList  */
#line 237 "parser.y"
            { FreeStmt((ParseCommon *) ((*yyvaluep).exprList).head); }
#line 1587 "parser.c"
        break;

    case YYSYMBOL_NonEmptyActions: /* NonEmptyActions  */
#line 234 "parser.y"
            { FreeStmt((ParseCommon *) ((*yyvaluep).expr)); }
#line 1593 "parser.c"
        break;

    case YYSYMBOL_Actions: /* Actions  */
#line 234 "parser.y"
            { FreeStmt((ParseCommon *) ((*yyvaluep).expr)); }
#line 1599 "parser.c"
        break;

    case YYSYMBOL_Action: /* Action  */
#line 234 "parser.y"
            { FreeStmt((ParseCommon *) ((*yyvaluep).expr)); }
#line 1605 "parser.c"
        break;

    case YYSYMBOL_Lhs: /* Lhs  */
#line 234 "parser.y"
            { FreeStmt((ParseCommon *) ((*yyvaluep).expr)); }
#line 1611 "parser.c"
        break;

    case YYSYMBOL_Terminal: /* Terminal  */
#line 234 "parser.y"
            { FreeStmt((ParseCommon *) ((*yyvaluep).expr)); }
#line 1617 "parser.c"
        break;

    case YYSYMBOL_MultiKeySymList: /* MultiKeySymList  */
#line 237 "parser.y"
            { FreeStmt((ParseCommon *) ((*yyvaluep).exprList).head); }
#line 1623 "parser.c"
        break;

    case YYSYMBOL_KeySymList: /* KeySymList  */
#line 234 "parser.y"
            { FreeStmt((ParseCommon *) ((*yyvaluep).expr)); }
#line 1629 "parser.c"
        break;

    case YYSYMBOL_NonEmptyKeySyms: /* NonEmptyKeySyms  */
#line 234 "parser.y"
            { FreeStmt((ParseCommon *) ((*yyvaluep).expr)); }
#line 1635 "parser.c"
        break;

    case YYSYMBOL_KeySyms: /* KeySyms  */
#line 234 "parser.y"
            { FreeStmt((ParseCommon *) ((*yyvaluep).expr)); }
#line 1641 "parser.c"
        break;

    case YYSYMBOL_OptMapName: /* OptMapName  */
#line 243 "parser.y"
            { free(((*yyvaluep).str)); }
#line 1647 "parser.c"
        break;

    case YYSYMBOL_MapName: /* MapName  */
#line 243 "parser.y"
            { free(((*yyvaluep).str)); }
#line 1653 "parser.c"
        break;

      default:
        break;
    }
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}






/*----------.
| yyparse.  |
`----------*/

int
yyparse (struct parser_param *param)
{
/* Lookahead token kind.  */
int yychar;


/* The semantic value of the lookahead symbol.  */
/* Default value used for initialization, for pacifying older GCCs
   or non-GCC compilers.  */
YY_INITIAL_VALUE (static YYSTYPE yyval_default;)
YYSTYPE yylval YY_INITIAL_VALUE (= yyval_default);

    /* Number of syntax errors so far.  */
    int yynerrs = 0;

    yy_state_fast_t yystate = 0;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus = 0;

    /* Refer to the stacks through separate pointers, to allow yyoverflow
       to reallocate them elsewhere.  */

    /* Their size.  */
    YYPTRDIFF_T yystacksize = YYINITDEPTH;

    /* The state stack: array, bottom, top.  */
    yy_state_t yyssa[YYINITDEPTH];
    yy_state_t *yyss = yyssa;
    yy_state_t *yyssp = yyss;

    /* The semantic value stack: array, bottom, top.  */
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs = yyvsa;
    YYSTYPE *yyvsp = yyvs;

  int yyn;
  /* The return value of yyparse.  */
  int yyresult;
  /* Lookahead symbol kind.  */
  yysymbol_kind_t yytoken = YYSYMBOL_YYEMPTY;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;



#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yychar = YYEMPTY; /* Cause a token to be read.  */

  goto yysetstate;


/*------------------------------------------------------------.
| yynewstate -- push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;


/*--------------------------------------------------------------------.
| yysetstate -- set current state (the top of the stack) to yystate.  |
`--------------------------------------------------------------------*/
yysetstate:
  YYDPRINTF ((stderr, "Entering state %d\n", yystate));
  YY_ASSERT (0 <= yystate && yystate < YYNSTATES);
  YY_IGNORE_USELESS_CAST_BEGIN
  *yyssp = YY_CAST (yy_state_t, yystate);
  YY_IGNORE_USELESS_CAST_END
  YY_STACK_PRINT (yyss, yyssp);

  if (yyss + yystacksize - 1 <= yyssp)
#if !defined yyoverflow && !defined YYSTACK_RELOCATE
    YYNOMEM;
#else
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYPTRDIFF_T yysize = yyssp - yyss + 1;

# if defined yyoverflow
      {
        /* Give user a chance to reallocate the stack.  Use copies of
           these so that the &'s don't force the real ones into
           memory.  */
        yy_state_t *yyss1 = yyss;
        YYSTYPE *yyvs1 = yyvs;

        /* Each stack pointer address is followed by the size of the
           data in use in that stack, in bytes.  This used to be a
           conditional around just the two extra args, but that might
           be undefined if yyoverflow is a macro.  */
        yyoverflow (YY_("memory exhausted"),
                    &yyss1, yysize * YYSIZEOF (*yyssp),
                    &yyvs1, yysize * YYSIZEOF (*yyvsp),
                    &yystacksize);
        yyss = yyss1;
        yyvs = yyvs1;
      }
# else /* defined YYSTACK_RELOCATE */
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
        YYNOMEM;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
        yystacksize = YYMAXDEPTH;

      {
        yy_state_t *yyss1 = yyss;
        union yyalloc *yyptr =
          YY_CAST (union yyalloc *,
                   YYSTACK_ALLOC (YY_CAST (YYSIZE_T, YYSTACK_BYTES (yystacksize))));
        if (! yyptr)
          YYNOMEM;
        YYSTACK_RELOCATE (yyss_alloc, yyss);
        YYSTACK_RELOCATE (yyvs_alloc, yyvs);
#  undef YYSTACK_RELOCATE
        if (yyss1 != yyssa)
          YYSTACK_FREE (yyss1);
      }
# endif

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;

      YY_IGNORE_USELESS_CAST_BEGIN
      YYDPRINTF ((stderr, "Stack size increased to %ld\n",
                  YY_CAST (long, yystacksize)));
      YY_IGNORE_USELESS_CAST_END

      if (yyss + yystacksize - 1 <= yyssp)
        YYABORT;
    }
#endif /* !defined yyoverflow && !defined YYSTACK_RELOCATE */


  if (yystate == YYFINAL)
    YYACCEPT;

  goto yybackup;


/*-----------.
| yybackup.  |
`-----------*/
yybackup:
  /* Do appropriate processing given the current state.  Read a
     lookahead token if we need one and don't already have one.  */

  /* First try to decide what to do without reference to lookahead token.  */
  yyn = yypact[yystate];
  if (yypact_value_is_default (yyn))
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* YYCHAR is either empty, or end-of-input, or a valid lookahead.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token\n"));
      yychar = yylex (&yylval, param_scanner);
    }

  if (yychar <= END_OF_FILE)
    {
      yychar = END_OF_FILE;
      yytoken = YYSYMBOL_YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else if (yychar == YYerror)
    {
      /* The scanner already issued an error message, process directly
         to error recovery.  But do not keep the error token as
         lookahead, it is too special and may lead us to an endless
         loop in error recovery. */
      yychar = YYUNDEF;
      yytoken = YYSYMBOL_YYerror;
      goto yyerrlab1;
    }
  else
    {
      yytoken = YYTRANSLATE (yychar);
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
    }

  /* If the proper action on seeing token YYTOKEN is to reduce or to
     detect an error, take that action.  */
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
    goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0)
    {
      if (yytable_value_is_error (yyn))
        goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the lookahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);
  yystate = yyn;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END

  /* Discard the shifted token.  */
  yychar = YYEMPTY;
  goto yynewstate;


/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


/*-----------------------------.
| yyreduce -- do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     '$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];


  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
  case 2: /* XkbFile: XkbCompositeMap  */
#line 260 "parser.y"
                        { (yyval.file) = param->rtrn = (yyvsp[0].file); param->more_maps = !!param->rtrn; (void) yynerrs; }
#line 1929 "parser.c"
    break;

  case 3: /* XkbFile: XkbMapConfig  */
#line 262 "parser.y"
                        { (yyval.file) = param->rtrn = (yyvsp[0].file); param->more_maps = !!param->rtrn; YYACCEPT; }
#line 1935 "parser.c"
    break;

  case 4: /* XkbFile: END_OF_FILE  */
#line 264 "parser.y"
                        { (yyval.file) = param->rtrn = NULL; param->more_maps = false; }
#line 1941 "parser.c"
    break;

  case 5: /* XkbCompositeMap: OptFlags XkbCompositeType OptMapName OBRACE XkbMapConfigList CBRACE SEMI  */
#line 270 "parser.y"
                        { (yyval.file) = XkbFileCreate((yyvsp[-5].file_type), (yyvsp[-4].str), (ParseCommon *) (yyvsp[-2].fileList).head, (yyvsp[-6].mapFlags)); }
#line 1947 "parser.c"
    break;

  case 6: /* XkbCompositeType: XKB_KEYMAP  */
#line 273 "parser.y"
                                        { (yyval.file_type) = FILE_TYPE_KEYMAP; }
#line 1953 "parser.c"
    break;

  case 7: /* XkbCompositeType: XKB_SEMANTICS  */
#line 274 "parser.y"
                                        { (yyval.file_type) = FILE_TYPE_KEYMAP; }
#line 1959 "parser.c"
    break;

  case 8: /* XkbCompositeType: XKB_LAYOUT  */
#line 275 "parser.y"
                                        { (yyval.file_type) = FILE_TYPE_KEYMAP; }
#line 1965 "parser.c"
    break;

  case 9: /* XkbMapConfigList: XkbMapConfigList XkbMapConfig  */
#line 282 "parser.y"
                        {
                            if ((yyvsp[0].file)) {
                                if ((yyvsp[-1].fileList).head) {
                                    (yyval.fileList).head = (yyvsp[-1].fileList).head;
                                    (yyval.fileList).last->common.next = &(yyvsp[0].file)->common;
                                    (yyval.fileList).last = (yyvsp[0].file);
                                } else {
                                    (yyval.fileList).head = (yyval.fileList).last = (yyvsp[0].file);
                                }
                            }
                        }
#line 1981 "parser.c"
    break;

  case 10: /* XkbMapConfigList: %empty  */
#line 293 "parser.y"
                        { (yyval.fileList).head = (yyval.fileList).last = NULL; }
#line 1987 "parser.c"
    break;

  case 11: /* XkbMapConfig: OptFlags FileType OptMapName OBRACE DeclList CBRACE SEMI  */
#line 299 "parser.y"
                        {
                            (yyval.file) = XkbFileCreate((yyvsp[-5].file_type), (yyvsp[-4].str), (yyvsp[-2].anyList).head, (yyvsp[-6].mapFlags));
                        }
#line 1995 "parser.c"
    break;

  case 12: /* FileType: XKB_KEYCODES  */
#line 304 "parser.y"
                                                { (yyval.file_type) = FILE_TYPE_KEYCODES; }
#line 2001 "parser.c"
    break;

  case 13: /* FileType: XKB_TYPES  */
#line 305 "parser.y"
                                                { (yyval.file_type) = FILE_TYPE_TYPES; }
#line 2007 "parser.c"
    break;

  case 14: /* FileType: XKB_COMPATMAP  */
#line 306 "parser.y"
                                                { (yyval.file_type) = FILE_TYPE_COMPAT; }
#line 2013 "parser.c"
    break;

  case 15: /* FileType: XKB_SYMBOLS  */
#line 307 "parser.y"
                                                { (yyval.file_type) = FILE_TYPE_SYMBOLS; }
#line 2019 "parser.c"
    break;

  case 16: /* FileType: XKB_GEOMETRY  */
#line 308 "parser.y"
                                                { (yyval.file_type) = FILE_TYPE_GEOMETRY; }
#line 2025 "parser.c"
    break;

  case 17: /* OptFlags: Flags  */
#line 311 "parser.y"
                                                { (yyval.mapFlags) = (yyvsp[0].mapFlags); }
#line 2031 "parser.c"
    break;

  case 18: /* OptFlags: %empty  */
#line 312 "parser.y"
                                                { (yyval.mapFlags) = 0; }
#line 2037 "parser.c"
    break;

  case 19: /* Flags: Flags Flag  */
#line 315 "parser.y"
                                                { (yyval.mapFlags) = ((yyvsp[-1].mapFlags) | (yyvsp[0].mapFlags)); }
#line 2043 "parser.c"
    break;

  case 20: /* Flags: Flag  */
#line 316 "parser.y"
                                                { (yyval.mapFlags) = (yyvsp[0].mapFlags); }
#line 2049 "parser.c"
    break;

  case 21: /* Flag: PARTIAL  */
#line 319 "parser.y"
                                                { (yyval.mapFlags) = MAP_IS_PARTIAL; }
#line 2055 "parser.c"
    break;

  case 22: /* Flag: DEFAULT  */
#line 320 "parser.y"
                                                { (yyval.mapFlags) = MAP_IS_DEFAULT; }
#line 2061 "parser.c"
    break;

  case 23: /* Flag: HIDDEN  */
#line 321 "parser.y"
                                                { (yyval.mapFlags) = MAP_IS_HIDDEN; }
#line 2067 "parser.c"
    break;

  case 24: /* Flag: ALPHANUMERIC_KEYS  */
#line 322 "parser.y"
                                                { (yyval.mapFlags) = MAP_HAS_ALPHANUMERIC; }
#line 2073 "parser.c"
    break;

  case 25: /* Flag: MODIFIER_KEYS  */
#line 323 "parser.y"
                                                { (yyval.mapFlags) = MAP_HAS_MODIFIER; }
#line 2079 "parser.c"
    break;

  case 26: /* Flag: KEYPAD_KEYS  */
#line 324 "parser.y"
                                                { (yyval.mapFlags) = MAP_HAS_KEYPAD; }
#line 2085 "parser.c"
    break;

  case 27: /* Flag: FUNCTION_KEYS  */
#line 325 "parser.y"
                                                { (yyval.mapFlags) = MAP_HAS_FN; }
#line 2091 "parser.c"
    break;

  case 28: /* Flag: ALTERNATE_GROUP  */
#line 326 "parser.y"
                                                { (yyval.mapFlags) = MAP_IS_ALTGR; }
#line 2097 "parser.c"
    break;

  case 29: /* DeclList: DeclList Decl  */
#line 330 "parser.y"
                        {
                            if ((yyvsp[0].any)) {
                                if ((yyvsp[-1].anyList).head) {
                                    (yyval.anyList).head = (yyvsp[-1].anyList).head; (yyvsp[-1].anyList).last->next = (yyvsp[0].any); (yyval.anyList).last = (yyvsp[0].any);
                                } else {
                                    (yyval.anyList).head = (yyval.anyList).last = (yyvsp[0].any);
                                }
                            }
                        }
#line 2111 "parser.c"
    break;

  case 30: /* DeclList: DeclList OptMergeMode VModDecl  */
#line 345 "parser.y"
                        {
                            for (VModDef *vmod = (yyvsp[0].vmodList).head; vmod; vmod = (VModDef *) vmod->common.next)
                                vmod->merge = (yyvsp[-1].merge);
                            if ((yyvsp[-2].anyList).head) {
                                (yyval.anyList).head = (yyvsp[-2].anyList).head; (yyvsp[-2].anyList).last->next = &(yyvsp[0].vmodList).head->common; (yyval.anyList).last = &(yyvsp[0].vmodList).last->common;
                            } else {
                                (yyval.anyList).head = &(yyvsp[0].vmodList).head->common; (yyval.anyList).last = &(yyvsp[0].vmodList).last->common;
                            }
                        }
#line 2125 "parser.c"
    break;

  case 31: /* DeclList: %empty  */
#line 354 "parser.y"
                        { (yyval.anyList).head = (yyval.anyList).last = NULL; }
#line 2131 "parser.c"
    break;

  case 32: /* Decl: OptMergeMode VarDecl  */
#line 358 "parser.y"
                        {
                            (yyvsp[0].var)->merge = (yyvsp[-1].merge);
                            (yyval.any) = (ParseCommon *) (yyvsp[0].var);
                        }
#line 2140 "parser.c"
    break;

  case 33: /* Decl: OptMergeMode InterpretDecl  */
#line 364 "parser.y"
                        {
                            (yyvsp[0].interp)->merge = (yyvsp[-1].merge);
                            (yyval.any) = (ParseCommon *) (yyvsp[0].interp);
                        }
#line 2149 "parser.c"
    break;

  case 34: /* Decl: OptMergeMode KeyNameDecl  */
#line 369 "parser.y"
                        {
                            (yyvsp[0].keyCode)->merge = (yyvsp[-1].merge);
                            (yyval.any) = (ParseCommon *) (yyvsp[0].keyCode);
                        }
#line 2158 "parser.c"
    break;

  case 35: /* Decl: OptMergeMode KeyAliasDecl  */
#line 374 "parser.y"
                        {
                            (yyvsp[0].keyAlias)->merge = (yyvsp[-1].merge);
                            (yyval.any) = (ParseCommon *) (yyvsp[0].keyAlias);
                        }
#line 2167 "parser.c"
    break;

  case 36: /* Decl: OptMergeMode KeyTypeDecl  */
#line 379 "parser.y"
                        {
                            (yyvsp[0].keyType)->merge = (yyvsp[-1].merge);
                            (yyval.any) = (ParseCommon *) (yyvsp[0].keyType);
                        }
#line 2176 "parser.c"
    break;

  case 37: /* Decl: OptMergeMode SymbolsDecl  */
#line 384 "parser.y"
                        {
                            (yyvsp[0].syms)->merge = (yyvsp[-1].merge);
                            (yyval.any) = (ParseCommon *) (yyvsp[0].syms);
                        }
#line 2185 "parser.c"
    break;

  case 38: /* Decl: OptMergeMode ModMapDecl  */
#line 389 "parser.y"
                        {
                            (yyvsp[0].modMask)->merge = (yyvsp[-1].merge);
                            (yyval.any) = (ParseCommon *) (yyvsp[0].modMask);
                        }
#line 2194 "parser.c"
    break;

  case 39: /* Decl: OptMergeMode GroupCompatDecl  */
#line 394 "parser.y"
                        {
                            (yyvsp[0].groupCompat)->merge = (yyvsp[-1].merge);
                            (yyval.any) = (ParseCommon *) (yyvsp[0].groupCompat);
                        }
#line 2203 "parser.c"
    break;

  case 40: /* Decl: OptMergeMode LedMapDecl  */
#line 399 "parser.y"
                        {
                            (yyvsp[0].ledMap)->merge = (yyvsp[-1].merge);
                            (yyval.any) = (ParseCommon *) (yyvsp[0].ledMap);
                        }
#line 2212 "parser.c"
    break;

  case 41: /* Decl: OptMergeMode LedNameDecl  */
#line 404 "parser.y"
                        {
                            (yyvsp[0].ledName)->merge = (yyvsp[-1].merge);
                            (yyval.any) = (ParseCommon *) (yyvsp[0].ledName);
                        }
#line 2221 "parser.c"
    break;

  case 42: /* Decl: OptMergeMode ShapeDecl  */
#line 408 "parser.y"
                                                        { (yyval.any) = NULL; }
#line 2227 "parser.c"
    break;

  case 43: /* Decl: OptMergeMode SectionDecl  */
#line 409 "parser.y"
                                                        { (yyval.any) = NULL; }
#line 2233 "parser.c"
    break;

  case 44: /* Decl: OptMergeMode DoodadDecl  */
#line 410 "parser.y"
                                                        { (yyval.any) = NULL; }
#line 2239 "parser.c"
    break;

  case 45: /* Decl: MergeMode STRING  */
#line 412 "parser.y"
                        {
                            (yyval.any) = (ParseCommon *) IncludeCreate(param->ctx, (yyvsp[0].str), (yyvsp[-1].merge));
                            free((yyvsp[0].str));
                        }
#line 2248 "parser.c"
    break;

  case 46: /* VarDecl: Lhs EQUALS Expr SEMI  */
#line 419 "parser.y"
                        { (yyval.var) = VarCreate((yyvsp[-3].expr), (yyvsp[-1].expr)); }
#line 2254 "parser.c"
    break;

  case 47: /* VarDecl: Ident SEMI  */
#line 421 "parser.y"
                        { (yyval.var) = BoolVarCreate((yyvsp[-1].atom), true); }
#line 2260 "parser.c"
    break;

  case 48: /* VarDecl: EXCLAM Ident SEMI  */
#line 423 "parser.y"
                        { (yyval.var) = BoolVarCreate((yyvsp[-1].atom), false); }
#line 2266 "parser.c"
    break;

  case 49: /* KeyNameDecl: KEYNAME EQUALS KeyCode SEMI  */
#line 427 "parser.y"
                        { (yyval.keyCode) = KeycodeCreate((yyvsp[-3].atom), (yyvsp[-1].num)); }
#line 2272 "parser.c"
    break;

  case 50: /* KeyAliasDecl: ALIAS KEYNAME EQUALS KEYNAME SEMI  */
#line 431 "parser.y"
                        { (yyval.keyAlias) = KeyAliasCreate((yyvsp[-3].atom), (yyvsp[-1].atom)); }
#line 2278 "parser.c"
    break;

  case 51: /* VModDecl: VIRTUAL_MODS VModDefList SEMI  */
#line 435 "parser.y"
                        { (yyval.vmodList) = (yyvsp[-1].vmodList); }
#line 2284 "parser.c"
    break;

  case 52: /* VModDefList: VModDefList COMMA VModDef  */
#line 439 "parser.y"
                        { (yyval.vmodList).head = (yyvsp[-2].vmodList).head; (yyval.vmodList).last->common.next = &(yyvsp[0].vmod)->common; (yyval.vmodList).last = (yyvsp[0].vmod); }
#line 2290 "parser.c"
    break;

  case 53: /* VModDefList: VModDef  */
#line 441 "parser.y"
                        { (yyval.vmodList).head = (yyval.vmodList).last = (yyvsp[0].vmod); }
#line 2296 "parser.c"
    break;

  case 54: /* VModDef: Ident  */
#line 445 "parser.y"
                        { (yyval.vmod) = VModCreate((yyvsp[0].atom), NULL); }
#line 2302 "parser.c"
    break;

  case 55: /* VModDef: Ident EQUALS Expr  */
#line 447 "parser.y"
                        { (yyval.vmod) = VModCreate((yyvsp[-2].atom), (yyvsp[0].expr)); }
#line 2308 "parser.c"
    break;

  case 56: /* InterpretDecl: INTERPRET InterpretMatch OBRACE VarDeclList CBRACE SEMI  */
#line 453 "parser.y"
                        { (yyvsp[-4].interp)->def = (yyvsp[-2].varList).head; (yyval.interp) = (yyvsp[-4].interp); }
#line 2314 "parser.c"
    break;

  case 57: /* InterpretMatch: KeySym PLUS Expr  */
#line 457 "parser.y"
                        { (yyval.interp) = InterpCreate((yyvsp[-2].keysym), (yyvsp[0].expr)); }
#line 2320 "parser.c"
    break;

  case 58: /* InterpretMatch: KeySym  */
#line 459 "parser.y"
                        { (yyval.interp) = InterpCreate((yyvsp[0].keysym), NULL); }
#line 2326 "parser.c"
    break;

  case 59: /* VarDeclList: VarDeclList VarDecl  */
#line 463 "parser.y"
                        {
                            if ((yyvsp[0].var)) {
                                if ((yyvsp[-1].varList).head) {
                                    (yyval.varList).head = (yyvsp[-1].varList).head;
                                    (yyval.varList).last->common.next = &(yyvsp[0].var)->common;
                                    (yyval.varList).last = (yyvsp[0].var);
                                } else {
                                    (yyval.varList).head = (yyval.varList).last = (yyvsp[0].var);
                                }
                            }
                        }
#line 2342 "parser.c"
    break;

  case 60: /* VarDeclList: %empty  */
#line 474 "parser.y"
                        { (yyval.varList).head = (yyval.varList).last = NULL; }
#line 2348 "parser.c"
    break;

  case 61: /* KeyTypeDecl: TYPE String OBRACE VarDeclList CBRACE SEMI  */
#line 480 "parser.y"
                        { (yyval.keyType) = KeyTypeCreate((yyvsp[-4].atom), (yyvsp[-2].varList).head); }
#line 2354 "parser.c"
    break;

  case 62: /* SymbolsDecl: KEY KEYNAME OBRACE OptSymbolsBody CBRACE SEMI  */
#line 486 "parser.y"
                        { (yyval.syms) = SymbolsCreate((yyvsp[-4].atom), (yyvsp[-2].varList).head); }
#line 2360 "parser.c"
    break;

  case 63: /* OptSymbolsBody: SymbolsBody  */
#line 489 "parser.y"
                                    { (yyval.varList) = (yyvsp[0].varList); }
#line 2366 "parser.c"
    break;

  case 64: /* OptSymbolsBody: %empty  */
#line 490 "parser.y"
                                    { (yyval.varList).head = (yyval.varList).last = NULL; }
#line 2372 "parser.c"
    break;

  case 65: /* SymbolsBody: SymbolsBody COMMA SymbolsVarDecl  */
#line 494 "parser.y"
                        { (yyval.varList).head = (yyvsp[-2].varList).head; (yyval.varList).last->common.next = &(yyvsp[0].var)->common; (yyval.varList).last = (yyvsp[0].var); }
#line 2378 "parser.c"
    break;

  case 66: /* SymbolsBody: SymbolsVarDecl  */
#line 496 "parser.y"
                        { (yyval.varList).head = (yyval.varList).last = (yyvsp[0].var); }
#line 2384 "parser.c"
    break;

  case 67: /* SymbolsVarDecl: Lhs EQUALS Expr  */
#line 499 "parser.y"
                                                { (yyval.var) = VarCreate((yyvsp[-2].expr), (yyvsp[0].expr)); }
#line 2390 "parser.c"
    break;

  case 68: /* SymbolsVarDecl: Lhs EQUALS MultiKeySymOrActionList  */
#line 500 "parser.y"
                                                           { (yyval.var) = VarCreate((yyvsp[-2].expr), (yyvsp[0].expr)); }
#line 2396 "parser.c"
    break;

  case 69: /* SymbolsVarDecl: Ident  */
#line 501 "parser.y"
                                                { (yyval.var) = BoolVarCreate((yyvsp[0].atom), true); }
#line 2402 "parser.c"
    break;

  case 70: /* SymbolsVarDecl: EXCLAM Ident  */
#line 502 "parser.y"
                                                { (yyval.var) = BoolVarCreate((yyvsp[0].atom), false); }
#line 2408 "parser.c"
    break;

  case 71: /* SymbolsVarDecl: MultiKeySymOrActionList  */
#line 503 "parser.y"
                                                { (yyval.var) = VarCreate(NULL, (yyvsp[0].expr)); }
#line 2414 "parser.c"
    break;

  case 72: /* MultiKeySymOrActionList: OBRACKET MultiKeySymList CBRACKET  */
#line 519 "parser.y"
                        { (yyval.expr) = (yyvsp[-1].exprList).head; }
#line 2420 "parser.c"
    break;

  case 73: /* MultiKeySymOrActionList: OBRACKET NoSymbolOrActionList COMMA MultiKeySymList CBRACKET  */
#line 521 "parser.y"
                        {
                            /* Prepend n times NoSymbol */
                            struct {ExprDef *head; ExprDef *last;} list = {
                                .head = (yyvsp[-1].exprList).head, .last = (yyvsp[-1].exprList).last
                            };
                            for (uint32_t k = 0; k < (yyvsp[-3].noSymbolOrActionList); k++) {
                                ExprDef* const syms =
                                    ExprCreateKeySymList(XKB_KEY_NoSymbol);
                                if (!syms) {
                                    /* TODO: Use Bison’s more appropriate YYNOMEM */
                                    YYABORT;
                                }
                                syms->common.next = &list.head->common;
                                list.head = syms;
                            }
                            (yyval.expr) = list.head;
                        }
#line 2442 "parser.c"
    break;

  case 74: /* MultiKeySymOrActionList: OBRACKET MultiActionList CBRACKET  */
#line 539 "parser.y"
                        { (yyval.expr) = (yyvsp[-1].exprList).head; }
#line 2448 "parser.c"
    break;

  case 75: /* MultiKeySymOrActionList: OBRACKET NoSymbolOrActionList COMMA MultiActionList CBRACKET  */
#line 541 "parser.y"
                        {
                            /* Prepend n times NoAction() */
                            struct {ExprDef *head; ExprDef *last;} list = {
                                .head = (yyvsp[-1].exprList).head, .last = (yyvsp[-1].exprList).last
                            };
                            for (uint32_t k = 0; k < (yyvsp[-3].noSymbolOrActionList); k++) {
                                ExprDef* const acts = ExprCreateActionList(NULL);
                                if (!acts) {
                                    /* TODO: Use Bison’s more appropriate YYNOMEM */
                                    YYABORT;
                                }
                                acts->common.next = &list.head->common;
                                list.head = acts;
                            }
                            (yyval.expr) = list.head;
                        }
#line 2469 "parser.c"
    break;

  case 76: /* MultiKeySymOrActionList: OBRACKET NoSymbolOrActionList CBRACKET  */
#line 563 "parser.y"
                        { (yyval.expr) = ExprEmptyList(); }
#line 2475 "parser.c"
    break;

  case 77: /* NoSymbolOrActionList: NoSymbolOrActionList COMMA OBRACE CBRACE  */
#line 569 "parser.y"
                        { (yyval.noSymbolOrActionList) = (yyvsp[-3].noSymbolOrActionList) + 1; }
#line 2481 "parser.c"
    break;

  case 78: /* NoSymbolOrActionList: OBRACE CBRACE  */
#line 571 "parser.y"
                        { (yyval.noSymbolOrActionList) = 1; }
#line 2487 "parser.c"
    break;

  case 79: /* NoSymbolOrActionList: %empty  */
#line 572 "parser.y"
                        { (yyval.noSymbolOrActionList) = 0; }
#line 2493 "parser.c"
    break;

  case 80: /* GroupCompatDecl: GROUP Integer EQUALS Expr SEMI  */
#line 576 "parser.y"
                        { (yyval.groupCompat) = GroupCompatCreate((yyvsp[-3].num), (yyvsp[-1].expr)); }
#line 2499 "parser.c"
    break;

  case 81: /* ModMapDecl: MODIFIER_MAP Ident OBRACE KeyOrKeySymList CBRACE SEMI  */
#line 580 "parser.y"
                        { (yyval.modMask) = ModMapCreate((yyvsp[-4].atom), (yyvsp[-2].exprList).head); }
#line 2505 "parser.c"
    break;

  case 82: /* KeyOrKeySymList: KeyOrKeySymList COMMA KeyOrKeySym  */
#line 584 "parser.y"
                        { (yyval.exprList).head = (yyvsp[-2].exprList).head; (yyval.exprList).last->common.next = &(yyvsp[0].expr)->common; (yyval.exprList).last = (yyvsp[0].expr); }
#line 2511 "parser.c"
    break;

  case 83: /* KeyOrKeySymList: KeyOrKeySym  */
#line 586 "parser.y"
                        { (yyval.exprList).head = (yyval.exprList).last = (yyvsp[0].expr); }
#line 2517 "parser.c"
    break;

  case 84: /* KeyOrKeySym: KEYNAME  */
#line 590 "parser.y"
                        { (yyval.expr) = ExprCreateKeyName((yyvsp[0].atom)); }
#line 2523 "parser.c"
    break;

  case 85: /* KeyOrKeySym: KeySym  */
#line 592 "parser.y"
                        { (yyval.expr) = ExprCreateKeySym((yyvsp[0].keysym)); }
#line 2529 "parser.c"
    break;

  case 86: /* LedMapDecl: INDICATOR String OBRACE VarDeclList CBRACE SEMI  */
#line 596 "parser.y"
                        { (yyval.ledMap) = LedMapCreate((yyvsp[-4].atom), (yyvsp[-2].varList).head); }
#line 2535 "parser.c"
    break;

  case 87: /* LedNameDecl: INDICATOR Integer EQUALS Expr SEMI  */
#line 600 "parser.y"
                        { (yyval.ledName) = LedNameCreate((yyvsp[-3].num), (yyvsp[-1].expr), false); }
#line 2541 "parser.c"
    break;

  case 88: /* LedNameDecl: VIRTUAL INDICATOR Integer EQUALS Expr SEMI  */
#line 602 "parser.y"
                        { (yyval.ledName) = LedNameCreate((yyvsp[-3].num), (yyvsp[-1].expr), true); }
#line 2547 "parser.c"
    break;

  case 89: /* ShapeDecl: SHAPE String OBRACE OutlineList CBRACE SEMI  */
#line 606 "parser.y"
                        { (yyval.geom) = NULL; }
#line 2553 "parser.c"
    break;

  case 90: /* ShapeDecl: SHAPE String OBRACE CoordList CBRACE SEMI  */
#line 608 "parser.y"
                        { (void) (yyvsp[-2].expr); (yyval.geom) = NULL; }
#line 2559 "parser.c"
    break;

  case 91: /* SectionDecl: SECTION String OBRACE SectionBody CBRACE SEMI  */
#line 612 "parser.y"
                        { (yyval.geom) = NULL; }
#line 2565 "parser.c"
    break;

  case 92: /* SectionBody: SectionBody SectionBodyItem  */
#line 615 "parser.y"
                                                        { (yyval.geom) = NULL;}
#line 2571 "parser.c"
    break;

  case 93: /* SectionBody: SectionBodyItem  */
#line 616 "parser.y"
                                                        { (yyval.geom) = NULL; }
#line 2577 "parser.c"
    break;

  case 94: /* SectionBodyItem: ROW OBRACE RowBody CBRACE SEMI  */
#line 620 "parser.y"
                        { (yyval.geom) = NULL; }
#line 2583 "parser.c"
    break;

  case 95: /* SectionBodyItem: VarDecl  */
#line 622 "parser.y"
                        { FreeStmt((ParseCommon *) (yyvsp[0].var)); (yyval.geom) = NULL; }
#line 2589 "parser.c"
    break;

  case 96: /* SectionBodyItem: DoodadDecl  */
#line 624 "parser.y"
                        { (yyval.geom) = NULL; }
#line 2595 "parser.c"
    break;

  case 97: /* SectionBodyItem: LedMapDecl  */
#line 626 "parser.y"
                        { FreeStmt((ParseCommon *) (yyvsp[0].ledMap)); (yyval.geom) = NULL; }
#line 2601 "parser.c"
    break;

  case 98: /* SectionBodyItem: OverlayDecl  */
#line 628 "parser.y"
                        { (yyval.geom) = NULL; }
#line 2607 "parser.c"
    break;

  case 99: /* RowBody: RowBody RowBodyItem  */
#line 631 "parser.y"
                                                { (yyval.geom) = NULL;}
#line 2613 "parser.c"
    break;

  case 100: /* RowBody: RowBodyItem  */
#line 632 "parser.y"
                                                { (yyval.geom) = NULL; }
#line 2619 "parser.c"
    break;

  case 101: /* RowBodyItem: KEYS OBRACE Keys CBRACE SEMI  */
#line 635 "parser.y"
                                                     { (yyval.geom) = NULL; }
#line 2625 "parser.c"
    break;

  case 102: /* RowBodyItem: VarDecl  */
#line 637 "parser.y"
                        { FreeStmt((ParseCommon *) (yyvsp[0].var)); (yyval.geom) = NULL; }
#line 2631 "parser.c"
    break;

  case 103: /* Keys: Keys COMMA Key  */
#line 640 "parser.y"
                                                { (yyval.geom) = NULL; }
#line 2637 "parser.c"
    break;

  case 104: /* Keys: Key  */
#line 641 "parser.y"
                                                { (yyval.geom) = NULL; }
#line 2643 "parser.c"
    break;

  case 105: /* Key: KEYNAME  */
#line 645 "parser.y"
                        { (yyval.geom) = NULL; }
#line 2649 "parser.c"
    break;

  case 106: /* Key: OBRACE ExprList CBRACE  */
#line 647 "parser.y"
                        { FreeStmt((ParseCommon *) (yyvsp[-1].exprList).head); (yyval.geom) = NULL; }
#line 2655 "parser.c"
    break;

  case 107: /* OverlayDecl: OVERLAY String OBRACE OverlayKeyList CBRACE SEMI  */
#line 651 "parser.y"
                        { (yyval.geom) = NULL; }
#line 2661 "parser.c"
    break;

  case 108: /* OverlayKeyList: OverlayKeyList COMMA OverlayKey  */
#line 654 "parser.y"
                                                        { (yyval.geom) = NULL; }
#line 2667 "parser.c"
    break;

  case 109: /* OverlayKeyList: OverlayKey  */
#line 655 "parser.y"
                                                        { (yyval.geom) = NULL; }
#line 2673 "parser.c"
    break;

  case 110: /* OverlayKey: KEYNAME EQUALS KEYNAME  */
#line 658 "parser.y"
                                                        { (yyval.geom) = NULL; }
#line 2679 "parser.c"
    break;

  case 111: /* OutlineList: OutlineList COMMA OutlineInList  */
#line 662 "parser.y"
                        { (yyval.geom) = NULL;}
#line 2685 "parser.c"
    break;

  case 112: /* OutlineList: OutlineInList  */
#line 664 "parser.y"
                        { (yyval.geom) = NULL; }
#line 2691 "parser.c"
    break;

  case 113: /* OutlineInList: OBRACE CoordList CBRACE  */
#line 668 "parser.y"
                        { (void) (yyvsp[-1].expr); (yyval.geom) = NULL; }
#line 2697 "parser.c"
    break;

  case 114: /* OutlineInList: Ident EQUALS OBRACE CoordList CBRACE  */
#line 670 "parser.y"
                        { (void) (yyvsp[-1].expr); (yyval.geom) = NULL; }
#line 2703 "parser.c"
    break;

  case 115: /* OutlineInList: Ident EQUALS Expr  */
#line 672 "parser.y"
                        { FreeStmt((ParseCommon *) (yyvsp[0].expr)); (yyval.geom) = NULL; }
#line 2709 "parser.c"
    break;

  case 116: /* CoordList: CoordList COMMA Coord  */
#line 676 "parser.y"
                        { (void) (yyvsp[-2].expr); (void) (yyvsp[0].expr); (yyval.expr) = NULL; }
#line 2715 "parser.c"
    break;

  case 117: /* CoordList: Coord  */
#line 678 "parser.y"
                        { (void) (yyvsp[0].expr); (yyval.expr) = NULL; }
#line 2721 "parser.c"
    break;

  case 118: /* Coord: OBRACKET SignedNumber COMMA SignedNumber CBRACKET  */
#line 682 "parser.y"
                        { (yyval.expr) = NULL; }
#line 2727 "parser.c"
    break;

  case 119: /* DoodadDecl: DoodadType String OBRACE VarDeclList CBRACE SEMI  */
#line 686 "parser.y"
                        { FreeStmt((ParseCommon *) (yyvsp[-2].varList).head); (yyval.geom) = NULL; }
#line 2733 "parser.c"
    break;

  case 120: /* DoodadType: TEXT  */
#line 689 "parser.y"
                                { (yyval.num) = 0; }
#line 2739 "parser.c"
    break;

  case 121: /* DoodadType: OUTLINE  */
#line 690 "parser.y"
                                { (yyval.num) = 0; }
#line 2745 "parser.c"
    break;

  case 122: /* DoodadType: SOLID  */
#line 691 "parser.y"
                                { (yyval.num) = 0; }
#line 2751 "parser.c"
    break;

  case 123: /* DoodadType: LOGO  */
#line 692 "parser.y"
                                { (yyval.num) = 0; }
#line 2757 "parser.c"
    break;

  case 124: /* FieldSpec: Ident  */
#line 695 "parser.y"
                                { (yyval.atom) = (yyvsp[0].atom); }
#line 2763 "parser.c"
    break;

  case 125: /* FieldSpec: Element  */
#line 696 "parser.y"
                                { (yyval.atom) = (yyvsp[0].atom); }
#line 2769 "parser.c"
    break;

  case 126: /* Element: ACTION_TOK  */
#line 700 "parser.y"
                        { (yyval.atom) = xkb_atom_intern_literal(param->ctx, "action"); }
#line 2775 "parser.c"
    break;

  case 127: /* Element: INTERPRET  */
#line 702 "parser.y"
                        { (yyval.atom) = xkb_atom_intern_literal(param->ctx, "interpret"); }
#line 2781 "parser.c"
    break;

  case 128: /* Element: TYPE  */
#line 704 "parser.y"
                        { (yyval.atom) = xkb_atom_intern_literal(param->ctx, "type"); }
#line 2787 "parser.c"
    break;

  case 129: /* Element: KEY  */
#line 706 "parser.y"
                        { (yyval.atom) = xkb_atom_intern_literal(param->ctx, "key"); }
#line 2793 "parser.c"
    break;

  case 130: /* Element: GROUP  */
#line 708 "parser.y"
                        { (yyval.atom) = xkb_atom_intern_literal(param->ctx, "group"); }
#line 2799 "parser.c"
    break;

  case 131: /* Element: MODIFIER_MAP  */
#line 710 "parser.y"
                        {(yyval.atom) = xkb_atom_intern_literal(param->ctx, "modifier_map");}
#line 2805 "parser.c"
    break;

  case 132: /* Element: INDICATOR  */
#line 712 "parser.y"
                        { (yyval.atom) = xkb_atom_intern_literal(param->ctx, "indicator"); }
#line 2811 "parser.c"
    break;

  case 133: /* Element: SHAPE  */
#line 714 "parser.y"
                        { (yyval.atom) = xkb_atom_intern_literal(param->ctx, "shape"); }
#line 2817 "parser.c"
    break;

  case 134: /* Element: ROW  */
#line 716 "parser.y"
                        { (yyval.atom) = xkb_atom_intern_literal(param->ctx, "row"); }
#line 2823 "parser.c"
    break;

  case 135: /* Element: SECTION  */
#line 718 "parser.y"
                        { (yyval.atom) = xkb_atom_intern_literal(param->ctx, "section"); }
#line 2829 "parser.c"
    break;

  case 136: /* Element: TEXT  */
#line 720 "parser.y"
                        { (yyval.atom) = xkb_atom_intern_literal(param->ctx, "text"); }
#line 2835 "parser.c"
    break;

  case 137: /* OptMergeMode: MergeMode  */
#line 723 "parser.y"
                                        { (yyval.merge) = (yyvsp[0].merge); }
#line 2841 "parser.c"
    break;

  case 138: /* OptMergeMode: %empty  */
#line 724 "parser.y"
                                        { (yyval.merge) = MERGE_DEFAULT; }
#line 2847 "parser.c"
    break;

  case 139: /* MergeMode: INCLUDE  */
#line 727 "parser.y"
                                        { (yyval.merge) = MERGE_DEFAULT; }
#line 2853 "parser.c"
    break;

  case 140: /* MergeMode: AUGMENT  */
#line 728 "parser.y"
                                        { (yyval.merge) = MERGE_AUGMENT; }
#line 2859 "parser.c"
    break;

  case 141: /* MergeMode: OVERRIDE  */
#line 729 "parser.y"
                                        { (yyval.merge) = MERGE_OVERRIDE; }
#line 2865 "parser.c"
    break;

  case 142: /* MergeMode: REPLACE  */
#line 730 "parser.y"
                                        { (yyval.merge) = MERGE_REPLACE; }
#line 2871 "parser.c"
    break;

  case 143: /* MergeMode: ALTERNATE  */
#line 732 "parser.y"
                {
                    /*
                     * This used to be MERGE_ALT_FORM. This functionality was
                     * unused and has been removed.
                     */
                    parser_warn(param, XKB_LOG_MESSAGE_NO_ID,
                                "ignored unsupported legacy merge mode \"alternate\"");
                    (yyval.merge) = MERGE_DEFAULT;
                }
#line 2885 "parser.c"
    break;

  case 144: /* ExprList: ExprList COMMA Expr  */
#line 744 "parser.y"
                        {
                            if ((yyvsp[0].expr)) {
                                if ((yyvsp[-2].exprList).head) {
                                    (yyval.exprList).head = (yyvsp[-2].exprList).head;
                                    (yyval.exprList).last->common.next = &(yyvsp[0].expr)->common;
                                    (yyval.exprList).last = (yyvsp[0].expr);
                                } else {
                                    (yyval.exprList).head = (yyval.exprList).last = (yyvsp[0].expr);
                                }
                            }
                        }
#line 2901 "parser.c"
    break;

  case 145: /* ExprList: Expr  */
#line 756 "parser.y"
                        { (yyval.exprList).head = (yyval.exprList).last = (yyvsp[0].expr); }
#line 2907 "parser.c"
    break;

  case 146: /* ExprList: %empty  */
#line 757 "parser.y"
                        { (yyval.exprList).head = (yyval.exprList).last = NULL; }
#line 2913 "parser.c"
    break;

  case 147: /* Expr: Expr DIVIDE Expr  */
#line 761 "parser.y"
                        { (yyval.expr) = ExprCreateBinary(STMT_EXPR_DIVIDE, (yyvsp[-2].expr), (yyvsp[0].expr)); }
#line 2919 "parser.c"
    break;

  case 148: /* Expr: Expr PLUS Expr  */
#line 763 "parser.y"
                        { (yyval.expr) = ExprCreateBinary(STMT_EXPR_ADD, (yyvsp[-2].expr), (yyvsp[0].expr)); }
#line 2925 "parser.c"
    break;

  case 149: /* Expr: Expr MINUS Expr  */
#line 765 "parser.y"
                        { (yyval.expr) = ExprCreateBinary(STMT_EXPR_SUBTRACT, (yyvsp[-2].expr), (yyvsp[0].expr)); }
#line 2931 "parser.c"
    break;

  case 150: /* Expr: Expr TIMES Expr  */
#line 767 "parser.y"
                        { (yyval.expr) = ExprCreateBinary(STMT_EXPR_MULTIPLY, (yyvsp[-2].expr), (yyvsp[0].expr)); }
#line 2937 "parser.c"
    break;

  case 151: /* Expr: Lhs EQUALS Expr  */
#line 769 "parser.y"
                        { (yyval.expr) = ExprCreateBinary(STMT_EXPR_ASSIGN, (yyvsp[-2].expr), (yyvsp[0].expr)); }
#line 2943 "parser.c"
    break;

  case 152: /* Expr: Term  */
#line 771 "parser.y"
                        { (yyval.expr) = (yyvsp[0].expr); }
#line 2949 "parser.c"
    break;

  case 153: /* Term: MINUS Term  */
#line 775 "parser.y"
                        { (yyval.expr) = ExprCreateUnary(STMT_EXPR_NEGATE, (yyvsp[0].expr)); }
#line 2955 "parser.c"
    break;

  case 154: /* Term: PLUS Term  */
#line 777 "parser.y"
                        { (yyval.expr) = ExprCreateUnary(STMT_EXPR_UNARY_PLUS, (yyvsp[0].expr)); }
#line 2961 "parser.c"
    break;

  case 155: /* Term: EXCLAM Term  */
#line 779 "parser.y"
                        { (yyval.expr) = ExprCreateUnary(STMT_EXPR_NOT, (yyvsp[0].expr)); }
#line 2967 "parser.c"
    break;

  case 156: /* Term: INVERT Term  */
#line 781 "parser.y"
                        { (yyval.expr) = ExprCreateUnary(STMT_EXPR_INVERT, (yyvsp[0].expr)); }
#line 2973 "parser.c"
    break;

  case 157: /* Term: Lhs  */
#line 783 "parser.y"
                        { (yyval.expr) = (yyvsp[0].expr); }
#line 2979 "parser.c"
    break;

  case 158: /* Term: FieldSpec OPAREN ExprList CPAREN  */
#line 785 "parser.y"
                        { (yyval.expr) = ExprCreateAction((yyvsp[-3].atom), (yyvsp[-1].exprList).head); }
#line 2985 "parser.c"
    break;

  case 159: /* Term: Actions  */
#line 787 "parser.y"
                        { (yyval.expr) = (yyvsp[0].expr); }
#line 2991 "parser.c"
    break;

  case 160: /* Term: Terminal  */
#line 789 "parser.y"
                        { (yyval.expr) = (yyvsp[0].expr); }
#line 2997 "parser.c"
    break;

  case 161: /* Term: OPAREN Expr CPAREN  */
#line 791 "parser.y"
                        { (yyval.expr) = (yyvsp[-1].expr); }
#line 3003 "parser.c"
    break;

  case 162: /* MultiActionList: MultiActionList COMMA Action  */
#line 795 "parser.y"
                        {
                            ExprDef *expr = ExprCreateActionList((yyvsp[0].expr));
                            (yyval.exprList) = (yyvsp[-2].exprList);
                            (yyval.exprList).last->common.next = &expr->common; (yyval.exprList).last = expr;
                        }
#line 3013 "parser.c"
    break;

  case 163: /* MultiActionList: MultiActionList COMMA Actions  */
#line 801 "parser.y"
                        { (yyval.exprList) = (yyvsp[-2].exprList); (yyval.exprList).last->common.next = &(yyvsp[0].expr)->common; (yyval.exprList).last = (yyvsp[0].expr); }
#line 3019 "parser.c"
    break;

  case 164: /* MultiActionList: Action  */
#line 803 "parser.y"
                        { (yyval.exprList).head = (yyval.exprList).last = ExprCreateActionList((yyvsp[0].expr)); }
#line 3025 "parser.c"
    break;

  case 165: /* MultiActionList: NonEmptyActions  */
#line 805 "parser.y"
                        { (yyval.exprList).head = (yyval.exprList).last = (yyvsp[0].expr); }
#line 3031 "parser.c"
    break;

  case 166: /* ActionList: ActionList COMMA Action  */
#line 809 "parser.y"
                        { (yyval.exprList) = (yyvsp[-2].exprList); (yyval.exprList).last->common.next = &(yyvsp[0].expr)->common; (yyval.exprList).last = (yyvsp[0].expr); }
#line 3037 "parser.c"
    break;

  case 167: /* ActionList: Action  */
#line 811 "parser.y"
                        { (yyval.exprList).head = (yyval.exprList).last = (yyvsp[0].expr); }
#line 3043 "parser.c"
    break;

  case 168: /* NonEmptyActions: OBRACE ActionList CBRACE  */
#line 815 "parser.y"
                        { (yyval.expr) = ExprCreateActionList((yyvsp[-1].exprList).head); }
#line 3049 "parser.c"
    break;

  case 169: /* Actions: NonEmptyActions  */
#line 819 "parser.y"
                        { (yyval.expr) = (yyvsp[0].expr); }
#line 3055 "parser.c"
    break;

  case 170: /* Actions: OBRACE CBRACE  */
#line 821 "parser.y"
                        { (yyval.expr) = ExprCreateActionList(NULL); }
#line 3061 "parser.c"
    break;

  case 171: /* Action: FieldSpec OPAREN ExprList CPAREN  */
#line 825 "parser.y"
                        { (yyval.expr) = ExprCreateAction((yyvsp[-3].atom), (yyvsp[-1].exprList).head); }
#line 3067 "parser.c"
    break;

  case 172: /* Lhs: FieldSpec  */
#line 829 "parser.y"
                        { (yyval.expr) = ExprCreateIdent((yyvsp[0].atom)); }
#line 3073 "parser.c"
    break;

  case 173: /* Lhs: FieldSpec DOT FieldSpec  */
#line 831 "parser.y"
                        { (yyval.expr) = ExprCreateFieldRef((yyvsp[-2].atom), (yyvsp[0].atom)); }
#line 3079 "parser.c"
    break;

  case 174: /* Lhs: FieldSpec OBRACKET Expr CBRACKET  */
#line 833 "parser.y"
                        { (yyval.expr) = ExprCreateArrayRef(XKB_ATOM_NONE, (yyvsp[-3].atom), (yyvsp[-1].expr)); }
#line 3085 "parser.c"
    break;

  case 175: /* Lhs: FieldSpec DOT FieldSpec OBRACKET Expr CBRACKET  */
#line 835 "parser.y"
                        { (yyval.expr) = ExprCreateArrayRef((yyvsp[-5].atom), (yyvsp[-3].atom), (yyvsp[-1].expr)); }
#line 3091 "parser.c"
    break;

  case 176: /* Terminal: String  */
#line 839 "parser.y"
                        { (yyval.expr) = ExprCreateString((yyvsp[0].atom)); }
#line 3097 "parser.c"
    break;

  case 177: /* Terminal: Integer  */
#line 841 "parser.y"
                        { (yyval.expr) = ExprCreateInteger((yyvsp[0].num)); }
#line 3103 "parser.c"
    break;

  case 178: /* Terminal: Float  */
#line 843 "parser.y"
                        { (yyval.expr) = ExprCreateFloat(/* Discard $1 */); }
#line 3109 "parser.c"
    break;

  case 179: /* Terminal: KEYNAME  */
#line 845 "parser.y"
                        { (yyval.expr) = ExprCreateKeyName((yyvsp[0].atom)); }
#line 3115 "parser.c"
    break;

  case 180: /* MultiKeySymList: MultiKeySymList COMMA KeySymLit  */
#line 849 "parser.y"
                        {
                            ExprDef *expr = ExprCreateKeySymList((yyvsp[0].keysym));
                            (yyval.exprList) = (yyvsp[-2].exprList);
                            (yyval.exprList).last->common.next = &expr->common; (yyval.exprList).last = expr;
                        }
#line 3125 "parser.c"
    break;

  case 181: /* MultiKeySymList: MultiKeySymList COMMA KeySyms  */
#line 855 "parser.y"
                        { (yyval.exprList) = (yyvsp[-2].exprList); (yyval.exprList).last->common.next = &(yyvsp[0].expr)->common; (yyval.exprList).last = (yyvsp[0].expr); }
#line 3131 "parser.c"
    break;

  case 182: /* MultiKeySymList: KeySymLit  */
#line 857 "parser.y"
                        { (yyval.exprList).head = (yyval.exprList).last = ExprCreateKeySymList((yyvsp[0].keysym)); }
#line 3137 "parser.c"
    break;

  case 183: /* MultiKeySymList: NonEmptyKeySyms  */
#line 859 "parser.y"
                        { (yyval.exprList).head = (yyval.exprList).last = (yyvsp[0].expr); }
#line 3143 "parser.c"
    break;

  case 184: /* KeySymList: KeySymList COMMA KeySymLit  */
#line 863 "parser.y"
                        { (yyval.expr) = ExprAppendKeySymList((yyvsp[-2].expr), (yyvsp[0].keysym)); }
#line 3149 "parser.c"
    break;

  case 185: /* KeySymList: KeySymList COMMA STRING  */
#line 865 "parser.y"
                        {
                            (yyval.expr) = ExprKeySymListAppendString(param->scanner, (yyvsp[-2].expr), (yyvsp[0].str));
                            free((yyvsp[0].str));
                            if (!(yyval.expr))
                                YYERROR;
                        }
#line 3160 "parser.c"
    break;

  case 186: /* KeySymList: KeySymLit  */
#line 872 "parser.y"
                        {
                            (yyval.expr) = ExprCreateKeySymList((yyvsp[0].keysym));
                            if (!(yyval.expr))
                                YYERROR;
                        }
#line 3170 "parser.c"
    break;

  case 187: /* KeySymList: STRING  */
#line 878 "parser.y"
                        {
                            (yyval.expr) = ExprCreateKeySymList(XKB_KEY_NoSymbol);
                            if (!(yyval.expr))
                                YYERROR;
                            (yyval.expr) = ExprKeySymListAppendString(param->scanner, (yyval.expr), (yyvsp[0].str));
                            free((yyvsp[0].str));
                            if (!(yyval.expr))
                                YYERROR;
                        }
#line 3184 "parser.c"
    break;

  case 188: /* NonEmptyKeySyms: OBRACE KeySymList CBRACE  */
#line 890 "parser.y"
                        { (yyval.expr) = (yyvsp[-1].expr); }
#line 3190 "parser.c"
    break;

  case 189: /* NonEmptyKeySyms: STRING  */
#line 892 "parser.y"
                        {
                            (yyval.expr) = ExprCreateKeySymList(XKB_KEY_NoSymbol);
                            if (!(yyval.expr))
                                YYERROR;
                            (yyval.expr) = ExprKeySymListAppendString(param->scanner, (yyval.expr), (yyvsp[0].str));
                            free((yyvsp[0].str));
                            if (!(yyval.expr))
                                YYERROR;
                        }
#line 3204 "parser.c"
    break;

  case 190: /* KeySyms: NonEmptyKeySyms  */
#line 904 "parser.y"
                        { (yyval.expr) = (yyvsp[0].expr); }
#line 3210 "parser.c"
    break;

  case 191: /* KeySyms: OBRACE CBRACE  */
#line 906 "parser.y"
                        { (yyval.expr) = ExprCreateKeySymList(XKB_KEY_NoSymbol); }
#line 3216 "parser.c"
    break;

  case 192: /* KeySym: KeySymLit  */
#line 910 "parser.y"
                        { (yyval.keysym) = (yyvsp[0].keysym); }
#line 3222 "parser.c"
    break;

  case 193: /* KeySym: STRING  */
#line 912 "parser.y"
                        {
                            (yyval.keysym) = KeysymParseString(param->scanner, (yyvsp[0].str));
                            free((yyvsp[0].str));
                            if ((yyval.keysym) == XKB_KEY_NoSymbol)
                                YYERROR;
                        }
#line 3233 "parser.c"
    break;

  case 194: /* KeySymLit: IDENT  */
#line 921 "parser.y"
                        {
                            if (!resolve_keysym(param, (yyvsp[0].sval), &(yyval.keysym))) {
                                parser_warn(
                                    param,
                                    XKB_WARNING_UNRECOGNIZED_KEYSYM,
                                    "unrecognized keysym \"%.*s\"",
                                    (yyvsp[0].sval).len, (yyvsp[0].sval).start
                                );
                                (yyval.keysym) = XKB_KEY_NoSymbol;
                            }
                        }
#line 3249 "parser.c"
    break;

  case 195: /* KeySymLit: SECTION  */
#line 933 "parser.y"
                                { (yyval.keysym) = XKB_KEY_section; }
#line 3255 "parser.c"
    break;

  case 196: /* KeySymLit: Integer  */
#line 935 "parser.y"
                        {
                            if ((yyvsp[0].num) < XKB_KEYSYM_MIN) {
                                /* Negative value */
                                static_assert(XKB_KEYSYM_MIN == 0,
                                              "Keysyms are positive");
                                parser_warn(
                                    param,
                                    XKB_ERROR_INVALID_NUMERIC_KEYSYM,
                                    "unrecognized keysym \"-%#06"PRIx64"\""
                                    " (%"PRId64")",
                                    -(yyvsp[0].num), (yyvsp[0].num)
                                );
                                (yyval.keysym) = XKB_KEY_NoSymbol;
                            }
                            else if ((yyvsp[0].num) < 10) {
                                /* Special case for digits 0..9:
                                 * map to XKB_KEY_0 .. XKB_KEY_9 */
                                (yyval.keysym) = XKB_KEY_0 + (xkb_keysym_t) (yyvsp[0].num);
                            }
                            else {
                                /* Any other numeric value */
                                if ((yyvsp[0].num) <= XKB_KEYSYM_MAX) {
                                    /*
                                     * Valid keysym
                                     * No normalization is performed and value
                                     * is used as is.
                                     */
                                    (yyval.keysym) = (xkb_keysym_t) (yyvsp[0].num);
                                    check_deprecated_keysyms(
                                        parser_warn, param, param->ctx,
                                        (yyval.keysym), NULL, (yyval.keysym), "%#06"PRIx32, "");
                                } else {
                                    /* Invalid keysym */
                                    parser_warn(
                                        param, XKB_ERROR_INVALID_NUMERIC_KEYSYM,
                                        "unrecognized keysym \"%#06"PRIx64"\" "
                                        "(%"PRId64")", (yyvsp[0].num), (yyvsp[0].num)
                                    );
                                    (yyval.keysym) = XKB_KEY_NoSymbol;
                                }
                                parser_warn(
                                    param, XKB_WARNING_NUMERIC_KEYSYM,
                                    "numeric keysym \"%#06"PRIx64"\" (%"PRId64")",
                                    (yyvsp[0].num), (yyvsp[0].num)
                                );
                            }
                        }
#line 3307 "parser.c"
    break;

  case 197: /* SignedNumber: MINUS Number  */
#line 984 "parser.y"
                                        { (yyval.num) = -(yyvsp[0].num); }
#line 3313 "parser.c"
    break;

  case 198: /* SignedNumber: Number  */
#line 985 "parser.y"
                                        { (yyval.num) = (yyvsp[0].num); }
#line 3319 "parser.c"
    break;

  case 199: /* Number: FLOAT  */
#line 988 "parser.y"
                                { (yyval.num) = (yyvsp[0].num); }
#line 3325 "parser.c"
    break;

  case 200: /* Number: INTEGER  */
#line 989 "parser.y"
                                { (yyval.num) = (yyvsp[0].num); }
#line 3331 "parser.c"
    break;

  case 201: /* Float: FLOAT  */
#line 992 "parser.y"
                                { (yyval.num) = 0; }
#line 3337 "parser.c"
    break;

  case 202: /* Integer: INTEGER  */
#line 995 "parser.y"
                                { (yyval.num) = (yyvsp[0].num); }
#line 3343 "parser.c"
    break;

  case 203: /* KeyCode: INTEGER  */
#line 998 "parser.y"
                                { (yyval.num) = (yyvsp[0].num); }
#line 3349 "parser.c"
    break;

  case 204: /* Ident: IDENT  */
#line 1001 "parser.y"
                                { (yyval.atom) = xkb_atom_intern(param->ctx, (yyvsp[0].sval).start, (yyvsp[0].sval).len); }
#line 3355 "parser.c"
    break;

  case 205: /* Ident: DEFAULT  */
#line 1002 "parser.y"
                                { (yyval.atom) = xkb_atom_intern_literal(param->ctx, "default"); }
#line 3361 "parser.c"
    break;

  case 206: /* String: STRING  */
#line 1005 "parser.y"
                                { (yyval.atom) = xkb_atom_intern(param->ctx, (yyvsp[0].str), strlen((yyvsp[0].str))); free((yyvsp[0].str)); }
#line 3367 "parser.c"
    break;

  case 207: /* OptMapName: MapName  */
#line 1008 "parser.y"
                                { (yyval.str) = (yyvsp[0].str); }
#line 3373 "parser.c"
    break;

  case 208: /* OptMapName: %empty  */
#line 1009 "parser.y"
                                { (yyval.str) = NULL; }
#line 3379 "parser.c"
    break;

  case 209: /* MapName: STRING  */
#line 1012 "parser.y"
                                { (yyval.str) = (yyvsp[0].str); }
#line 3385 "parser.c"
    break;


#line 3389 "parser.c"

      default: break;
    }
  /* User semantic actions sometimes alter yychar, and that requires
     that yytoken be updated with the new translation.  We take the
     approach of translating immediately before every use of yytoken.
     One alternative is translating here after every semantic action,
     but that translation would be missed if the semantic action invokes
     YYABORT, YYACCEPT, or YYERROR immediately after altering yychar or
     if it invokes YYBACKUP.  In the case of YYABORT or YYACCEPT, an
     incorrect destructor might then be invoked immediately.  In the
     case of YYERROR or YYBACKUP, subsequent parser actions might lead
     to an incorrect destructor call or verbose syntax error message
     before the lookahead is translated.  */
  YY_SYMBOL_PRINT ("-> $$ =", YY_CAST (yysymbol_kind_t, yyr1[yyn]), &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;

  *++yyvsp = yyval;

  /* Now 'shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */
  {
    const int yylhs = yyr1[yyn] - YYNTOKENS;
    const int yyi = yypgoto[yylhs] + *yyssp;
    yystate = (0 <= yyi && yyi <= YYLAST && yycheck[yyi] == *yyssp
               ? yytable[yyi]
               : yydefgoto[yylhs]);
  }

  goto yynewstate;


/*--------------------------------------.
| yyerrlab -- here on detecting error.  |
`--------------------------------------*/
yyerrlab:
  /* Make sure we have latest lookahead translation.  See comments at
     user semantic actions for why this is necessary.  */
  yytoken = yychar == YYEMPTY ? YYSYMBOL_YYEMPTY : YYTRANSLATE (yychar);
  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
      yyerror (param, YY_("syntax error"));
    }

  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
         error, discard it.  */

      if (yychar <= END_OF_FILE)
        {
          /* Return failure if at end of input.  */
          if (yychar == END_OF_FILE)
            YYABORT;
        }
      else
        {
          yydestruct ("Error: discarding",
                      yytoken, &yylval, param);
          yychar = YYEMPTY;
        }
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:
  /* Pacify compilers when the user code never invokes YYERROR and the
     label yyerrorlab therefore never appears in user code.  */
  if (0)
    YYERROR;
  ++yynerrs;

  /* Do not reclaim the symbols of the rule whose action triggered
     this YYERROR.  */
  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);
  yystate = *yyssp;
  goto yyerrlab1;


/*-------------------------------------------------------------.
| yyerrlab1 -- common code for both syntax error and YYERROR.  |
`-------------------------------------------------------------*/
yyerrlab1:
  yyerrstatus = 3;      /* Each real token shifted decrements this.  */

  /* Pop stack until we find a state that shifts the error token.  */
  for (;;)
    {
      yyn = yypact[yystate];
      if (!yypact_value_is_default (yyn))
        {
          yyn += YYSYMBOL_YYerror;
          if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYSYMBOL_YYerror)
            {
              yyn = yytable[yyn];
              if (0 < yyn)
                break;
            }
        }

      /* Pop the current state because it cannot handle the error token.  */
      if (yyssp == yyss)
        YYABORT;


      yydestruct ("Error: popping",
                  YY_ACCESSING_SYMBOL (yystate), yyvsp, param);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END


  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", YY_ACCESSING_SYMBOL (yyn), yyvsp, yylsp);

  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturnlab;


/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturnlab;


/*-----------------------------------------------------------.
| yyexhaustedlab -- YYNOMEM (memory exhaustion) comes here.  |
`-----------------------------------------------------------*/
yyexhaustedlab:
  yyerror (param, YY_("memory exhausted"));
  yyresult = 2;
  goto yyreturnlab;


/*----------------------------------------------------------.
| yyreturnlab -- parsing is finished, clean up and return.  |
`----------------------------------------------------------*/
yyreturnlab:
  if (yychar != YYEMPTY)
    {
      /* Make sure we have latest lookahead translation.  See comments at
         user semantic actions for why this is necessary.  */
      yytoken = YYTRANSLATE (yychar);
      yydestruct ("Cleanup: discarding lookahead",
                  yytoken, &yylval, param);
    }
  /* Do not reclaim the symbols of the rule whose action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
                  YY_ACCESSING_SYMBOL (+*yyssp), yyvsp, param);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif

  return yyresult;
}

#line 1015 "parser.y"


XkbFile *
parse(struct xkb_context *ctx, struct scanner *scanner, const char *map)
{
    int ret;
    XkbFile *first = NULL;
    struct parser_param param = {
        .scanner = scanner,
        .ctx = ctx,
        .rtrn = NULL,
        .more_maps = false,
    };

    /*
     * If we got a specific map, we look for it exclusively and return
     * immediately upon finding it. Otherwise, we need to get the
     * default map. If we find a map marked as default, we return it
     * immediately. If there are no maps marked as default, we return
     * the first map in the file.
     */

    while ((ret = yyparse(&param)) == 0 && param.more_maps) {
        if (map) {
            if (streq_not_null(map, param.rtrn->name))
                return param.rtrn;
            else
                FreeXkbFile(param.rtrn);
        }
        else {
            if (param.rtrn->flags & MAP_IS_DEFAULT) {
                FreeXkbFile(first);
                return param.rtrn;
            }
            else if (!first) {
                first = param.rtrn;
            }
            else {
                FreeXkbFile(param.rtrn);
            }
        }
        param.rtrn = NULL;
    }

    if (ret != 0) {
        /* Some error happend; clear the Xkbfiles parsed so far */
        FreeXkbFile(first);
        FreeXkbFile(param.rtrn);
        return NULL;
    }

    if (first)
        log_vrb(ctx, 5,
                XKB_WARNING_MISSING_DEFAULT_SECTION,
                "No map in include statement, but \"%s\" contains several; "
                "Using first defined map, \"%s\"\n",
                scanner->file_name, safe_map_name(first));

    return first;
}
