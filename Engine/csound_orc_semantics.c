 /*
    csound_orc_semantics.c:

    Copyright (C) 2006
    John ffitch, Steven Yi

    This file is part of Csound.

    The Csound Library is free software; you can redistribute it
    and/or modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    Csound is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with Csound; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
    02111-1307 USA
*/

#include <stdio.h>
#include <stdlib.h>
#include "csoundCore.h"
#include "csound_orc.h"
#include "namedins.h"

TREE* force_rate(TREE* a, char t)
{                               /* Ensure a is of type t */
    return a;
}


/** Verifies that opcodes and args are correct*/
int verify_tree(CSOUND *csound, TREE *root) {
    csound->Message(csound, "Verifying AST (NEED TO IMPLEMENT)\n");
    return 1;
}


/* BISON PARSER FUNCTION */
int csound_orcwrap()
{
    /* csound->Message(csound, "END OF INPUT\n"); */
    return (1);
}

/* BISON PARSER FUNCTION */
void csound_orcerror(CSOUND *csound, char *str)
{
    csound->Message(csound, "csound_orcerror: %s\n", str);
}

/**
 * Appends TREE * node to TREE * node using ->next field in struct; walks
 * down the linked list to append at end; checks for NULL's and returns
 * appropriate nodes
 */
TREE* appendToTree(CSOUND * csound, TREE *first, TREE *newlast) {

    if(first == NULL) {
        return newlast;
    }

    if(newlast == NULL) {
        return first;
    }

    /* HACK - Checks to see if first node is uninitialized (sort of)
     * This occurs for rules like in topstatement where the left hand
     * topstatement the first time around is not initialized to anything
     * useful; the number 400 is arbitrary, chosen as it seemed to be a value
     * higher than all the type numbers that were being printed out
     */
    if(first->type > 400) {
        return newlast;
    }

    TREE *current = first;

    while(current->next != NULL) {
        current = current->next;
    }

    current->next = newlast;

    return first;
}


/* USED BY PARSER TO ASSEMBLE TREE NODES */
TREE* make_node(CSOUND *csound, int type, TREE* left, TREE* right)
{
  TREE *ans;
  ans = (TREE*)mmalloc(csound, sizeof(TREE));
  if (ans==NULL) {
    /* fprintf(stderr, "Out of memory\n"); */
    exit(1);
  }
  ans->type = type;
  ans->left = left;
  ans->right = right;
  ans->next = NULL;
  ans->len = 2;
  ans->rate = -1;
  return ans;
}

TREE* make_leaf(CSOUND *csound, int type, ORCTOKEN *v)
{
  TREE *ans;
  ans = (TREE*)mmalloc(csound, sizeof(TREE));
  if (ans==NULL) {
    /* fprintf(stderr, "Out of memory\n"); */
    exit(1);
  }
  ans->type = type;
  ans->left = NULL;
  ans->right = NULL;
  ans->next = NULL;
  ans->len = 0;
  ans->rate = -1;
  ans->value = v;
  return ans;
}

/** Utility function to create assignment statements
 *  Replaces = with correct version for args
 */
char* get_assignment_type(CSOUND *csound, char * ans, char * arg1) {
    char c = argtyp2(csound, ans);
    char* str = (char*)csound->Calloc(csound, 65);

    switch (c) {
          case 'S':
                  strcpy(str, "strcpy");
                  break;
          case 'a':
                  c = argtyp2(csound, arg1);
                strcpy(str, (c == 'a' ? "=.a" : "upsamp"));
                /* strcpy(str, "=.a"); */
                break;
          case 'p':
                  c = 'i'; /* purposefully fall through */
          default:
                  sprintf(str, "=.%c", c);
    }

    csound->Message(csound, "Found Assignment type: %s\n", str);

    return str;
}



/* DEBUGGING FUNCTIONS */
void print_tree_i(CSOUND *csound, TREE *l, int n)
{
    int i;
    if (l==NULL) {
        return;
    }
    for (i=0; i<n; i++) {
        csound->Message(csound, " ");
    }

    csound->Message(csound, "TYPE: %d ", l->type);

    switch (l->type) {
    case S_COM:
      csound->Message(csound,"S_COM:\n"); break;
    case S_Q:
      csound->Message(csound,"S_Q:\n"); break;
    case S_COL:
      csound->Message(csound,"S_COL:\n"); break;
    case S_NOT:
      csound->Message(csound,"S_NOT:\n"); break;
    case S_PLUS:
      csound->Message(csound,"S_PLUS:\n"); break;
    case S_MINUS:
      csound->Message(csound,"S_MINUS:\n"); break;
    case S_TIMES:
      csound->Message(csound,"S_TIMES:\n"); break;
    case S_DIV:
      csound->Message(csound,"S_DIV:\n"); break;
    case S_NL:
      csound->Message(csound,"S_NL:\n"); break;
    case S_LB:
      csound->Message(csound,"S_LB:\n"); break;
    case S_RB:
      csound->Message(csound,"S_RB:\n"); break;
    case S_NEQ:
      csound->Message(csound,"S_NEQ:\n"); break;
    case S_AND:
      csound->Message(csound,"S_AND:\n"); break;
    case S_OR:
      csound->Message(csound,"S_OR:\n"); break;
    case S_LT:
      csound->Message(csound,"S_LT:\n"); break;
    case S_LE:
      csound->Message(csound,"S_LE:\n"); break;
    case S_EQ:
      csound->Message(csound,"S_EQ:\n"); break;
    case S_ASSIGN:
      csound->Message(csound,"S_ASSIGN:\n"); break;
    case S_GT:
      csound->Message(csound,"S_GT:\n"); break;
    case S_GE:
      csound->Message(csound,"S_GE:\n"); break;
    case T_LABEL:
      csound->Message(csound,"T_LABEL: %s\n", l->value->lexeme); break;
    case T_IF:
      csound->Message(csound,"T_IF:\n"); break;
    case T_GOTO:
      csound->Message(csound,"T_GOTO:\n"); break;
    case T_IGOTO:
      csound->Message(csound,"T_IGOTO:\n"); break;
    case T_KGOTO:
      csound->Message(csound,"T_KGOTO:\n"); break;
    case T_SRATE:
      csound->Message(csound,"T_SRATE:\n"); break;
    case T_KRATE:
      csound->Message(csound,"T_KRATE:\n"); break;
    case T_KSMPS:
      csound->Message(csound,"T_KSMPS:\n"); break;
    case T_NCHNLS:
      csound->Message(csound,"T_NCHNLS:\n"); break;
    case T_INSTR:
      csound->Message(csound,"T_INSTR:\n"); break;
    case T_STRCONST:
      csound->Message(csound,"T_STRCONST: %s\n", l->value->lexeme); break;
    case T_IDENT:
      csound->Message(csound,"T_IDENT: %s\n", l->value->lexeme); break;
    case T_IDENT_I:
      csound->Message(csound,"IDENT_I: %s\n", l->value->lexeme); break;
    case T_IDENT_GI:
      csound->Message(csound,"IDENT_GI: %s\n", l->value->lexeme); break;
    case T_IDENT_K:
      csound->Message(csound,"IDENT_K: %s\n", l->value->lexeme); break;
    case T_IDENT_GK:
      csound->Message(csound,"IDENT_GK: %s\n", l->value->lexeme); break;
    case T_IDENT_A:
      csound->Message(csound,"IDENT_A: %s\n", l->value->lexeme); break;
    case T_IDENT_GA:
      csound->Message(csound,"IDENT_GA: %s\n", l->value->lexeme); break;
    case T_IDENT_S:
      csound->Message(csound,"IDENT_D: %s\n", l->value->lexeme); break;
    case T_IDENT_GS:
      csound->Message(csound,"IDENT_GS: %s\n", l->value->lexeme); break;
    case T_IDENT_W:
      csound->Message(csound,"IDENT_W: %s\n", l->value->lexeme); break;
    case T_IDENT_GW:
      csound->Message(csound,"IDENT_GW: %s\n", l->value->lexeme); break;
    case T_IDENT_F:
      csound->Message(csound,"IDENT_F: %s\n", l->value->lexeme); break;
    case T_IDENT_GF:
      csound->Message(csound,"IDENT_GF: %s\n", l->value->lexeme); break;
    case T_IDENT_P:
      csound->Message(csound,"IDENT_P: %s\n", l->value->lexeme); break;
    case T_IDENT_B:
      csound->Message(csound,"IDENT_B: %s\n", l->value->lexeme); break;
    case T_IDENT_b:
      csound->Message(csound,"IDENT_b: %s\n", l->value->lexeme); break;
    case T_INTGR:
      csound->Message(csound,"T_INTGR: %d\n", l->value->value); break;
    case T_NUMBER:
      csound->Message(csound,"T_NUMBER: %f\n", l->value->fvalue); break;
    case S_ANDTHEN:
      csound->Message(csound,"S_ANDTHEN:\n"); break;
    case S_APPLY:
      csound->Message(csound,"S_APPLY:\n"); break;
    case T_OPCODE0:
      csound->Message(csound,"T_OPCODE0: %s\n", l->value->lexeme); break;
    case T_OPCODE:
      csound->Message(csound,"T_OPCODE: %s\n", l->value->lexeme); break;
    case T_FUNCTION:
      csound->Message(csound,"T_FUNCTION: %s\n", l->value->lexeme); break;
    case S_UMINUS:
        csound->Message(csound,"S_UMINUS:\n"); break;
    default:
      csound->Message(csound,"t:%d\n", l->type);
    }

    print_tree_i(csound, l->left,n+1);
    print_tree_i(csound, l->right,n+1);

    if(l->next != NULL) {
        print_tree_i(csound, l->next, n);
    }
}

void print_tree(CSOUND * csound, TREE *l)
{
    csound->Message(csound, "Printing Tree\n");
    print_tree_i(csound, l, 0);
}



void handle_optional_args(CSOUND *csound, TREE *l) {
    int opnum = find_opcode(csound, l->value->lexeme);
    OENTRY *ep = csound->opcodlst + opnum;
    int nreqd = strlen(ep->intypes);
    int incnt = tree_arg_list_count(l->right);

    TREE * temp;

    csound->Message(csound, "Handling Optional Args for opcode %s, %d, %d",
        ep->opname, incnt, nreqd);

    if (incnt < nreqd) {         /*  or set defaults: */
         do {
              switch (ep->intypes[incnt]) {
                  case 'O':             /* Will this work?  Doubtful code.... */
                  case 'o':
                      temp = make_leaf(csound, T_INTGR, make_int(csound, "0"));
                      appendToTree(csound, l->right, temp);
                      break;
                  case 'p':
                      temp = make_leaf(csound, T_INTGR, make_int(csound, "1"));
                      appendToTree(csound, l->right, temp);
                      break;
                  case 'q':
                      temp = make_leaf(csound, T_INTGR, make_int(csound, "10"));
                      appendToTree(csound, l->right, temp);
                  break;

                case 'V':
                  case 'v':
                      temp = make_leaf(csound, T_NUMBER, make_num(csound, ".5"));
                      appendToTree(csound, l->right, temp);
                      break;
                case 'h':
                      temp = make_leaf(csound, T_INTGR, make_int(csound, "127"));
                      appendToTree(csound, l->right, temp);
                      break;
                case 'j':
                      temp = make_leaf(csound, T_INTGR, make_int(csound, "-1"));
                      appendToTree(csound, l->right, temp);
                      break;
                  case 'M':
                  case 'N':
                  case 'm':
                      nreqd--;
                    break;
                  default:
                      synterr(csound, Str("insufficient required arguments"));
              }
              incnt++;
        } while (incnt < nreqd);
    }
}

char tree_argtyp(CSOUND *csound, TREE *tree) {
    if(tree->type == T_INTGR || tree->type == T_NUMBER) {
        return 'i';
    }

    return argtyp2(csound, tree->value->lexeme);
}

void handle_polymorphic_opcode(CSOUND* csound, TREE * tree) {

    if(tree->type == S_ASSIGN) {
        tree->value->lexeme = get_assignment_type(csound,
            tree->left->value->lexeme, tree->right->value->lexeme);
        return;
    }

    int opnum = find_opcode(csound, tree->value->lexeme);
    OENTRY *ep = csound->opcodlst + opnum;

    int incnt = tree_arg_list_count(tree->right);

    char * str = (char *)mcalloc(csound, strlen(ep->opname) + 4);
    char c, d;

    if (ep->dsblksiz >= 0xfffb) {

        c = tree_argtyp(csound, tree->right);

        switch (ep->dsblksiz) {

        case 0xffff:
          /* use outype to modify some opcodes flagged as translating */
          csound->Message(csound, "[%s]\n", tree->left->value->lexeme);

          c = tree_argtyp(csound, tree->left);
          if (c == 'p')   c = 'i';
          if (c == '?')   c = 'a';                   /* tmp */
          sprintf(str, "%s.%c", ep->opname, c);

          csound->Message(csound, "New Value: %s\n", str);

          /*if (find_opcode(csound, str) == 0) {*/
            /* synterr(csound, Str("failed to find %s, output arg '%s' illegal type"),
                    str, ST(group)[ST(nxtest)]);*/    /* report syntax error     */
            /*ST(nxtest) = 100; */                       /* step way over this line */
            /*goto tstnxt;*/                            /* & go to next            */
            /*break;*/
          /*}*/
          tree->value->lexeme = (char *)mrealloc(csound, tree->value->lexeme, strlen(str) + 1);
          strcpy(tree->value->lexeme, str);
          csound->DebugMsg(csound, Str("modified opcod: %s"), str);
        break;
      case 0xfffe:                              /* Two tags for OSCIL's    */
        csound->Message(csound, "POLYMORPHIC 0xfffe\n");
        if (c != 'a') c = 'k';
        if ((d = tree_argtyp(csound, tree->right->next)) != 'a') d = 'k';
        sprintf(str, "%s.%c%c", ep->opname, c, d);
        csound->Message(csound, "New Value: %s\n", str);
        tree->value->lexeme = (char *)mrealloc(csound, tree->value->lexeme, strlen(str) + 1);
        strcpy(tree->value->lexeme, str);
        break;
      case 0xfffd:                              /* For peak, etc.          */
        /*if (c != 'a') c = 'k';
        sprintf(str, "%s.%c", ST(linopcod), c);*/
        break;
      case 0xfffc:                              /* For divz types          */
        /*d = argtyp(csound, ST(group)[ST(opgrpno)+1]);
        if ((c=='i' || c=='c') && (d=='i' || d=='c'))
          c = 'i', d = 'i';
        else {
          if (c != 'a') c = 'k';
          if (d != 'a') d = 'k';
        }
        sprintf(str, "%s.%c%c", ST(linopcod), c, d);*/
        break;
      case 0xfffb:          /* determine opcode by type of first input arg */
            /* allows a, k, and i types (e.g. Inc, Dec), but not constants */
        /*if (ST(typemask_tabl)[(unsigned char) c] & (ARGTYP_i | ARGTYP_p))
          c = 'i';
        sprintf(str, "%s.%c", ST(linopcod), c);*/
        break;
      default:
        break;
        /*strcpy(str, ST(linopcod));*/  /* unknown code: use original opcode   */
      }

      /*if (!(isopcod(csound, str))) {*/
                        /* if opcode is not found: report syntax error     */
        /*synterr(csound, Str("failed to find %s, input arg illegal type"), str);*/
        /*ST(nxtest) = 100;*/                       /* step way over this line */
        /*goto tstnxt;*/                            /* & go to next            */
      /*}
      ST(linopnum) = ST(opnum);
      ST(linopcod) = ST(opcod);
      csound->DebugMsg(csound, Str("modified opcod: %s"), ST(opcod));*/
    }

    /* free(str); */
}