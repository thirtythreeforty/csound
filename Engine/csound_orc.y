 /*
    csound_orc.l:

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
%token S_COM
%token S_Q
%token S_COL
%token S_NOT
%token S_PLUS
%token S_MINUS
%token S_TIMES
%token S_DIV
%token S_NL
%token S_LB
%token S_RB
%token S_NEQ
%token S_AND
%token S_OR
%token S_LT
%token S_LE
%token S_EQ
%token S_ASSIGN
%token S_GT
%token S_GE
%token S_XOR
%token S_MOD

%token T_LABEL
%token T_IF

%token T_OPCODE0
%token T_OPCODE

%token T_UDO
%token T_UDOSTART
%token T_UDOEND
%token T_UDO_ANS
%token T_UDO_ARGS

%token T_ERROR

%token T_FUNCTION

%token T_INSTR
%token T_ENDIN
%token T_STRSET
%token T_PSET
%token T_CTRLINIT
%token T_MASSIGN
%token T_TURNON
%token T_PREALLOC
%token T_ZAKINIT
%token T_FTGEN
%token T_INIT
%token T_GOTO
%token T_KGOTO
%token T_IGOTO

%token T_SRATE
%token T_KRATE
%token T_KSMPS
%token T_NCHNLS
%token T_STRCONST
%token T_IDENT

%token T_IDENT_I
%token T_IDENT_GI
%token T_IDENT_K
%token T_IDENT_GK
%token T_IDENT_A
%token T_IDENT_GA
%token T_IDENT_W
%token T_IDENT_GW
%token T_IDENT_F
%token T_IDENT_GF
%token T_IDENT_S
%token T_IDENT_GS
%token T_IDENT_P
%token T_IDENT_B
%token T_IDENT_b
%token T_INTGR
%token T_NUMBER
%token T_THEN

%start orcfile
%left S_AND S_OR
%nonassoc S_LT S_GT S_LEQ S_GEQ S_EQ S_NEQ
%left S_PLUS S_MINUS
%left S_STAR S_SLASH
%right S_UNOT
%right S_UMINUS
%token S_GOTO
%token T_HIGHEST
%pure_parser
%error-verbose
%parse-param { CSOUND * csound }
%parse-param { TREE * astTree }
%lex-param { CSOUND * csound }

/* NOTE: Perhaps should use %union feature of bison */

%{
#define YYSTYPE ORCTOKEN*
#ifndef NULL
#define NULL 0L
#endif
#include "csoundCore.h"
#include <ctype.h>
#include "namedins.h"

#include "csound_orc.h"

int udoflag = -1; /* THIS NEEDS TO BE MADE NON-GLOBAL */

extern TREE* appendToTree(CSOUND * csound, TREE *first, TREE *newlast);

%}
%%

orcfile           : rootstatement
                        {
                            *astTree = *((TREE *)$1);
                        }
                  ;

rootstatement	  : rootstatement topstatement
                        {
                        $$ = appendToTree(csound, (TREE *)$1, (TREE *)$2);
                        }
                  | rootstatement instrdecl
                        {
                        $$ = appendToTree(csound, (TREE *)$1, (TREE *)$2);
                        }
                  | rootstatement udodecl
                        {
                        $$ = appendToTree(csound, (TREE *)$1, (TREE *)$2);
                        }
                  | S_NL { }
                  ;

/* FIXME: Does not allow "instr 2,3,4,5,6" syntax */
/* FIXME: Does not allow named instruments i.e. "instr trumpet" */
instrdecl	  : T_INSTR T_INTGR S_NL statementlist T_ENDIN S_NL
                    {
                        TREE *leaf = make_leaf(csound, T_INTGR, $2);
                        $$ = make_node(csound, T_INSTR, leaf, $4);
                    }
          | T_INSTR S_NL error
                        { csound->Message(csound, "No number following instr\n"); }
          ;

udodecl		: T_UDOSTART T_IDENT_S S_COM
                { udoflag = 0;}
              T_UDO_ANS
                  { udoflag = 1; }
              S_COM T_UDO_ARGS S_NL
                  { udoflag = 2; }
              statementlist T_UDOEND S_NL
                  {	udoflag = -1;



                    TREE * udoTop = make_leaf(csound, T_UDO, NULL);
                    TREE * udoAns = make_leaf(csound, T_UDO_ANS, $5);
                    TREE * udoArgs = make_leaf(csound, T_UDO_ARGS, $8);


                    udoTop->left = udoAns;
                    udoAns->left = udoArgs;

                    udoTop->right = (TREE *)$11;

                    $$ = udoTop;
                }

            ;

/* rtparam		  : T_SRATE S_ASSIGN T_NUMBER S_NL
                    {
                        TREE *ans = make_leaf(csound, S_ASSIGN, $2);
                        ans->left = make_leaf(csound, $1->type, $1);
                        ans->right = make_leaf(csound, $3->type, $3);
                        ans->value->lexeme = get_assignment_type(csound, $1->lexeme);

                        $$ = ans;
                        //csound->tran_sr = (MYFLT)(((ORCTOKEN*)$3)->fvalue);
                        //csound->Message(csound, "sr set to %f\n", csound->tran_sr);
                    }
              | T_SRATE S_ASSIGN T_INTGR S_NL
                    {
                        TREE *ans = make_leaf(csound, S_ASSIGN, $2);
                        ans->left = make_leaf(csound, $1->type, $1);
                        ans->right = make_leaf(csound, $3->type, $3);
                        ans->value->lexeme = get_assignment_type(csound, $1->lexeme);

                        $$ = ans;
                      //csound->tran_sr = (MYFLT)(((ORCTOKEN*)$3)->value);
                      //csound->Message(csound, "sr set to %f\n", csound->tran_sr);
                    }
                | T_KRATE S_ASSIGN T_NUMBER S_NL
                    {
                        TREE *ans = make_leaf(csound, S_ASSIGN, $2);
                        ans->left = make_leaf(csound, $1->type, $1);
                        ans->right = make_leaf(csound, $3->type, $3);
                        ans->value->lexeme = get_assignment_type(csound, $1->lexeme);

                        $$ = ans;
                        ((TREE *)$$)->value->lexeme = "=.r";
                      //csound->tran_kr = (MYFLT)(((ORCTOKEN*)$3)->fvalue);
                      //csound->Message(csound, "kr set to %f\n", csound->tran_kr);
                    }
                | T_KRATE S_ASSIGN T_INTGR S_NL
                    {
                        TREE *ans = make_leaf(csound, S_ASSIGN, $2);
                        ans->left = make_leaf(csound, $1->type, $1);
                        ans->right = make_leaf(csound, $3->type, $3);
                        ans->value->lexeme = get_assignment_type(csound, $1->lexeme);

                        $$ = ans;
                      //csound->tran_kr = (MYFLT)(((ORCTOKEN*)$3)->value);
                      //csound->Message(csound, "kr set to %f\n", csound->tran_kr);
                    }
                | T_KSMPS S_ASSIGN T_INTGR S_NL
                    {
                        TREE *ans = make_leaf(csound, S_ASSIGN, $2);
                        ans->left = make_leaf(csound, $1->type, $1);
                        ans->right = make_leaf(csound, $3->type, $3);
                        ans->value->lexeme = get_assignment_type(csound, $1->lexeme);

                        $$ = ans;
                      //csound->tran_ksmps = (MYFLT)(((ORCTOKEN*)$3)->value);
                      //csound->Message(csound, "ksmps set to %f\n", csound->tran_ksmps);
                    }
                | T_NCHNLS S_ASSIGN T_INTGR S_NL
                    {
                        TREE *ans = make_leaf(csound, S_ASSIGN, $2);
                        ans->left = make_leaf(csound, $1->type, $1);
                        ans->right = make_leaf(csound, $3->type, $3);
                        ans->value->lexeme = get_assignment_type(csound, $1->lexeme);

                        $$ = ans;
                      //csound->tran_nchnls = ((ORCTOKEN*)$3)->value;
                      //csound->Message(csound, "nchnls set to %i\n", csound->tran_nchnls);
                    }
              | gident S_ASSIGN exprlist S_NL
                    {
                        TREE *ans = make_leaf(csound, S_ASSIGN, $2);
                        ans->left = (TREE *)$1;
                        ans->right = (TREE *)$3;
                        ans->value->lexeme = get_assignment_type(csound,
                                ((TREE *)$1)->value->lexeme);

                        $$ = ans;
                        //instr0(csound, $2, $1, check_opcode($2, $1, $3));
                      }
              | gans initop exprlist S_NL
                    { //instr0(csound, $2, $1, check_opcode($2, $1, $3));
                      }
                | initop0 exprlist S_NL
                    {
                        //instr0(csound, $1, NULL, check_opcode0($1, $2));
                    }
                | S_NL {
                    $$ = NULL;
                }
                  ;

initop0           : T_STRSET		{ $$ = make_leaf(csound, T_STRSET, NULL); }
                  | T_PSET		{ $$ = make_leaf(csound, T_PSET, NULL); }
                  | T_CTRLINIT		{ $$ = make_leaf(csound, T_CTRLINIT, NULL); }
                  | T_MASSIGN		{ $$ = make_leaf(csound, T_MASSIGN, NULL); }
                  | T_TURNON		{ $$ = make_leaf(csound, T_TURNON, NULL); }
                  | T_PREALLOC		{ $$ = make_leaf(csound, T_PREALLOC, NULL); }
                  | T_ZAKINIT		{ $$ = make_leaf(csound, T_ZAKINIT, NULL); }
                  ;
initop            : T_FTGEN		{ $$ = make_leaf(csound, T_FTGEN, NULL); }
                  | T_INIT              { $$ = make_leaf(csound, T_INIT, NULL); }
                  ;
*/

gans              : gident              { $$ = $1; }
                  | gans S_COM gident   { $$ = appendToTree(csound, $1, $3); }
                  ;



statementlist     : statementlist statement
                        {
                          $$ = appendToTree(csound, (TREE *)$1, (TREE *)$2);
                        }
                  | /* null */          { $$ = NULL; }
                  ;

topstatement : rident S_ASSIGN expr S_NL
                {

                    TREE *ans = make_leaf(csound, S_ASSIGN, $2);
                    ans->left = (TREE *)$1;
                    ans->right = (TREE *)$3;
                    /* ans->value->lexeme = get_assignment_type(csound, ans->left->value->lexeme, ans->right->value->lexeme); */

                    $$ = ans;
                }
             | statement { $$ = $1; }

             ;

statement : ident S_ASSIGN expr S_NL
                {

                    TREE *ans = make_leaf(csound, S_ASSIGN, $2);
                    ans->left = (TREE *)$1;
                    ans->right = (TREE *)$3;
                    /* ans->value->lexeme = get_assignment_type(csound, ans->left->value->lexeme, ans->right->value->lexeme); */

                    $$ = ans;
                }
          | ans opcode exprlist S_NL
                {

                    ((TREE *)$2)->left = (TREE *)$1;
                    ((TREE *)$2)->right = (TREE *)$3;

                    $$ = $2;
                }
          | opcode0 exprlist S_NL
                  {
                      ((TREE *)$1)->left = NULL;
                      ((TREE *)$1)->right = (TREE *)$2;

                      $$ = $1;
                  }
          | /* NULL */  { $$ = NULL; }
          | T_LABEL S_NL
                  {

                      $$ = make_leaf(csound, T_LABEL, yylval);

                  }
          | goto T_IDENT S_NL
                  {
                      ((TREE *)$1)->left = NULL;
                      ((TREE*) $1)->right = make_leaf(csound, T_IDENT, (ORCTOKEN *)$2);
                      $$ = $1;
                }
          | T_IF S_LB expr S_RB goto T_IDENT S_NL
                {
                    ((TREE *)$5)->left = NULL;
                    ((TREE *)$5)->right = make_leaf(csound, T_IDENT, (ORCTOKEN *)$6);
                    $$ = make_node(csound, T_IF, $3, $5);
                }
          | T_IF S_LB expr S_RB error
          | T_IF S_LB expr error
          | T_IF error
          | S_NL { $$ = NULL; }
          ;


lvalue		  : ident               { $$ = $1; }
          ;

ans               : ident               { $$ = $1; }
                  | ans S_COM ident     { $$ = appendToTree(csound, $1, $3); }
                  ;

goto		      : T_GOTO              { $$ = make_leaf(csound, T_GOTO, yylval); }
                  | T_KGOTO             { $$ = make_leaf(csound, T_KGOTO, yylval); }
                  | T_IGOTO             { $$ = make_leaf(csound, T_IGOTO, yylval); }
                  ;


exprlist  : exprlist S_COM expr
                {
                    /* $$ = make_node(S_COM, $1, $3); */
                    $$ = appendToTree(csound, (TREE *)$1, (TREE *)$3);
                }
          | exprlist S_COM error
          | expr { $$ = $1;	}
          | /* null */          { $$ = NULL; }
          ;





expr              : expr S_Q expr S_COL expr %prec S_Q
                                        { $$ = make_node(csound, S_Q, $1,
                                                make_node(csound, S_COL, $3, $5)); }
          | expr S_Q expr S_COL error
          | expr S_Q expr error
          | expr S_Q error
          | expr S_LE expr      { $$ = make_node(csound, S_LE, $1, $3); }
          | expr S_LE error
          | expr S_GE expr      { $$ = make_node(csound, S_GE, $1, $3); }
          | expr S_GE error
          | expr S_NEQ expr     { $$ = make_node(csound, S_NEQ, $1, $3); }
          | expr S_NEQ error
          | expr S_EQ expr      { $$ = make_node(csound, S_EQ, $1, $3); }
          | expr S_EQ error
          | expr S_GT expr      { $$ = make_node(csound, S_GT, $1, $3); }
          | expr S_GT error
          | expr S_LT expr      { $$ = make_node(csound, S_LT, $1, $3); }
          | expr S_LT error
          | expr S_AND expr     { $$ = make_node(csound, S_AND, $1, $3); }
          | expr S_AND error
          | expr S_OR expr      { $$ = make_node(csound, S_OR, $1, $3); }
          | expr S_OR error
          | S_NOT expr %prec S_UNOT { $$ = make_node(csound, S_UNOT, $2, NULL); }
          | S_NOT error
                  | iexp                { $$ = $1; }
                  ;

iexp      : iexp S_PLUS iterm   { $$ = make_node(csound, S_PLUS, $1, $3); }
          | iexp S_PLUS error
          | iexp S_MINUS iterm  { $$ = make_node(csound, S_MINUS, $1, $3); }
          | expr S_MINUS error
          | iterm               { $$ = $1; }
                  ;

iterm     : iterm S_TIMES ifac   { $$ = make_node(csound, S_TIMES, $1, $3); }
          | iterm S_TIMES error
          | iterm S_DIV ifac     { $$ = make_node(csound, S_DIV, $1, $3); }
          | iterm S_DIV error
                  | ifac                { $$ = $1; }
                  ;

ifac              : ident               { $$ = $1; }
                  | constant               { $$ = $1; }
          | S_MINUS ifac %prec S_UMINUS
                {
                       $$ = make_node(csound, S_UMINUS, NULL, $2);
                   }
          | S_MINUS error
                  | S_LB expr S_RB      { $$ = $2; }
          | S_LB expr error
          | S_LB error
          | function S_LB exprlist S_RB
                  {
                    ((TREE *)$1)->left = NULL;
                      ((TREE *)$1)->right = (TREE *)$3;

                    $$ = $1;
                  }
          | function S_LB error
                  ;

function		  : T_FUNCTION	{ $$ = make_leaf(csound, T_FUNCTION, $1); }
                  ;

/* exprstrlist	  : exprstrlist S_COM expr
                                        { $$ = make_node(csound, S_COM, $1, $3); }
          | exprstrlist S_COM T_STRCONST
                                        { $$ = make_node(csound, S_COM, $1,
                                                make_leaf(csound, T_STRCONST, yylval)); }
          | exprstrlist S_COM error
          | expr                { $$ = $1; }
          ;
 */

rident	  : T_SRATE 			{ $$ = make_leaf(csound, T_SRATE, yylval); }
          | T_KRATE             { $$ = make_leaf(csound, T_KRATE, yylval); }
          | T_KSMPS             { $$ = make_leaf(csound, T_KSMPS, yylval); }
          | T_NCHNLS	        { $$ = make_leaf(csound, T_NCHNLS, yylval); }
          ;

ident	  : T_IDENT_I			{ $$ = make_leaf(csound, T_IDENT_I, yylval); }
          | T_IDENT_K           { $$ = make_leaf(csound, T_IDENT_K, yylval); }
          | T_IDENT_F           { $$ = make_leaf(csound, T_IDENT_F, yylval); }
          | T_IDENT_W           { $$ = make_leaf(csound, T_IDENT_W, yylval); }
          | T_IDENT_S           { $$ = make_leaf(csound, T_IDENT_S, yylval); }
          | T_IDENT_A           { $$ = make_leaf(csound, T_IDENT_A, yylval); }
          | T_IDENT_P           { $$ = make_leaf(csound, T_IDENT_P, yylval); }
          | gident              { $$ = $1; }
          ;

gident	  : T_IDENT_GI          { $$ = make_leaf(csound, T_IDENT_GI, yylval); }
          | T_IDENT_GK          { $$ = make_leaf(csound, T_IDENT_GK, yylval); }
          | T_IDENT_GF          { $$ = make_leaf(csound, T_IDENT_GF, yylval); }
          | T_IDENT_GW          { $$ = make_leaf(csound, T_IDENT_GW, yylval); }
          | T_IDENT_GS          { $$ = make_leaf(csound, T_IDENT_GS, yylval); }
          | T_IDENT_GA          { $$ = make_leaf(csound, T_IDENT_GA, yylval); }
                  ;

constant	  : T_INTGR 		{ $$ = make_leaf(csound, T_INTGR, yylval); }
          | T_NUMBER 		{ $$ = make_leaf(csound, T_NUMBER, yylval); }
          | T_STRCONST 		{ $$ = make_leaf(csound, T_STRCONST, yylval); }
                  | T_SRATE             { $$ = make_leaf(csound, T_NUMBER, yylval); }
                  | T_KRATE             { $$ = make_leaf(csound, T_NUMBER, yylval); }
                  | T_KSMPS             { $$ = make_leaf(csound, T_NUMBER, yylval); }
                  | T_NCHNLS            { $$ = make_leaf(csound, T_NUMBER, yylval); }
          ;

opcode0           : T_OPCODE0           { csound->Message(csound, "opcode0 yylval=%p\n", yylval);
                                          $$ = make_leaf(csound, T_OPCODE0, yylval); }
          ;

opcode            : T_OPCODE		{ $$ = make_leaf(csound, T_OPCODE, yylval); }
                  ;

%%