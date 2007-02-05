 /*
    csound_orc_compile.c:
    (Based on otran.c)

    Copyright (C) 1991, 1997, 2003, 2006
    Barry Vercoe, John ffitch, Istvan Varga, Steven Yi

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

#include "csoundCore.h"
#include "csound_orc.h"
#include <math.h>
#include <ctype.h>

#include "oload.h"
#include "insert.h"
#include "pstream.h"
#include "namedins.h"
#include "typetabl.h"

typedef struct NAME_ {
    char          *namep;
    struct NAME_  *nxt;
    int           type, count;
} NAME;

typedef struct {
    NAME      *gblNames[256], *lclNames[256];   /* for 8 bit hash */
    ARGLST    *nullist;
    ARGOFFS   *nulloffs;
    int       lclkcnt, lclwcnt, lclfixed;
    int       lclpcnt, lclscnt, lclacnt, lclnxtpcnt;
    int       lclnxtkcnt, lclnxtwcnt, lclnxtacnt, lclnxtscnt;
    int       gblnxtkcnt, gblnxtpcnt, gblnxtacnt, gblnxtscnt;
    int       gblfixed, gblkcount, gblacount, gblscount;
    int       *nxtargoffp, *argofflim, lclpmax;
    char      **strpool;
    long      poolcount, strpool_cnt, argoffsize;
    int       nconsts;
    int       *constTbl;
} OTRAN_GLOBALS;

static  int     gexist(CSOUND *, char *), gbloffndx(CSOUND *, char *);
static  int     lcloffndx(CSOUND *, char *);
static  int     constndx(CSOUND *, const char *);
static  int     strconstndx(CSOUND *, const char *);
static  void    insprep(CSOUND *, INSTRTXT *);
static  void    lgbuild(CSOUND *, char *);
static  void    gblnamset(CSOUND *, char *);
static  int     plgndx(CSOUND *, char *);
static  NAME    *lclnamset(CSOUND *, char *);
        int     lgexist(CSOUND *, const char *);
static  void    delete_global_namepool(CSOUND *);
static  void    delete_local_namepool(CSOUND *);
static  int pnum(char *s) ;

char argtyp2(CSOUND *csound, char *s);

#define txtcpy(a,b) memcpy(a,b,sizeof(TEXT));
#define ST(x)   (((OTRAN_GLOBALS*) ((CSOUND*) csound)->otranGlobals)->x)

#define KTYPE   1
#define WTYPE   2
#define ATYPE   3
#define PTYPE   4
#define STYPE   5
/* NOTE: these assume that sizeof(MYFLT) is either 4 or 8 */
#define Wfloats (((int) sizeof(SPECDAT) + 7) / (int) sizeof(MYFLT))
#define Pfloats (((int) sizeof(PVSDAT) + 7) / (int) sizeof(MYFLT))

#ifdef FLOAT_COMPARE
#undef FLOAT_COMPARE
#endif
#ifdef USE_DOUBLE
#define FLOAT_COMPARE(x,y)  (fabs((double) (x) / (double) (y) - 1.0) > 1.0e-12)
#else
#define FLOAT_COMPARE(x,y)  (fabs((double) (x) / (double) (y) - 1.0) > 5.0e-7)
#endif

/** This function body copied from rdorch.c, not currently used */
static void lblclear(CSOUND *csound)
{
    /* ST(lblcnt) = 0; */
}

static void resetouts(CSOUND *csound)
{
    csound->acount = csound->kcount = csound->icount = 0;
    csound->Bcount = csound->bcount = 0;
}

TEXT *create_text(CSOUND *csound) {
    TEXT        *tp = (TEXT *)mcalloc(csound, (long)sizeof(TEXT));
    return tp;
}

int tree_arg_list_count(TREE * root) {
    int count = 0;

    TREE * current = root;

    while(current != NULL) {
        current = current->next;
        count++;
    }

    return count;
}

/**
 * Returns last OPTXT of OPTXT chain optxt
 */
static OPTXT * last_optxt(OPTXT *optxt) {
    OPTXT *current = optxt;

    while(current->nxtop != NULL) {
        current = current->nxtop;
    }

    return current;
}

/**
 * Append OPTXT op2 to end of OPTXT chain op1
 */
static void append_optxt(OPTXT *op1, OPTXT *op2) {
    last_optxt(op1)->nxtop = op2;
}


/**
 * Current not used; intended to do the job of counting lcl counts
 * but is flawed as it does not take into account counting variables
 * only once if used multiple times; to be removed or reused in context
 * of redoing namset functions (if even desirable)
 */
/* void update_lclcount(CSOUND *csound, INSTRTXT *ip, TREE *argslist) {
    TREE * current = argslist;

    while(current != NULL) {
        switch(current->type) {
            case T_IDENT_S:
                ip->lclscnt++;
                csound->Message(csound, "S COUNT INCREASED: %d\n", ip->lclscnt);
                break;
            case T_IDENT_W:
                ip->lclwcnt++;
                csound->Message(csound, "W COUNT INCREASED: %d\n", ip->lclwcnt);
                break;
            case T_IDENT_A:
                ip->lclacnt++;
                csound->Message(csound, "A COUNT INCREASED: %d\n", ip->lclacnt);
                break;
            case T_IDENT_K:
            case T_IDENT_F:
            case T_IDENT_I:
            case T_NUMBER:
            case T_INTGR:
            default:
                ip->lclkcnt++;
                csound->Message(csound, "K COUNT INCREASED: %d\n", ip->lclkcnt);
        }
        current = current->next;
    }
}*/

void set_xincod(CSOUND *csound, TEXT *tp, OENTRY *ep) {
     int n = tp->inlist->count;
     char *s;

     char *types = ep->intypes;

     int nreqd = -1;
     char      tfound = '\0', treqd;

     if (nreqd < 0)    /* for other opcodes */
        nreqd = strlen(types = ep->intypes);

      /*if (n > nreqd) {*/                  /* IV - Oct 24 2002: end of new code */
        /*if ((treqd = types[nreqd-1]) == 'n') {*/  /* indef args: */
        /*  if (!(incnt & 01))*/                    /* require odd */
            /*synterr(csound, Str("missing or extra arg"));*/
        /*}*/       /* IV - Sep 1 2002: added 'M' */
        /*else if (treqd != 'm' && treqd != 'z' && treqd != 'y' &&*/
                 /*treqd != 'Z' && treqd != 'M' && treqd != 'N')*/ /* else any no */
          /*synterr(csound, Str("too many input args"));*/
      /*}*/

     while (n--) {                     /* inargs:   */
        long    tfound_m, treqd_m = 0L;
        s = tp->inlist->arg[n];

        if (n >= nreqd) {               /* det type required */
          switch (types[nreqd-1]) {
            case 'M':
            case 'N':
            case 'Z':
            case 'y':
            case 'z':   treqd = types[nreqd-1]; break;
            default:    treqd = 'i';    /*   (indef in-type) */
          }
        }
        else treqd = types[n];          /*       or given)   */
        if (treqd == 'l') {             /* if arg takes lbl  */
          csound->DebugMsg(csound, "treqd = l");
          /*lblrequest(csound, s);*/        /*      req a search */
          continue;                     /*      chk it later */
        }
        tfound = argtyp2(csound, s);     /* else get arg type */
        /* IV - Oct 31 2002 */
        /*tfound_m = ST(typemask_tabl)[(unsigned char) tfound];
        if (!(tfound_m & (ARGTYP_c|ARGTYP_p)) && !ST(lgprevdef) && *s != '"') {
          synterr(csound, Str("input arg '%s' used before defined"), s);
        }*/
        csound->DebugMsg(csound, "treqd %c, tfound %c", treqd, tfound);
        if (tfound == 'a' && n < 31)    /* JMC added for FOG */
          /* 4 for FOF, 8 for FOG; expanded to 15  */
          tp->xincod |= (1 << n);
        if (tfound == 'S' && n < 31)
          tp->xincod_str |= (1 << n);
        /* IV - Oct 31 2002: simplified code */
       /* if (!(tfound_m & ST(typemask_tabl_in)[(unsigned char) treqd])) { */
          /* check for exceptional types */
          /*switch (treqd) {*/
          /*case 'Z':*/                             /* indef kakaka ... */
            /*if (!(tfound_m & (n & 1 ? ARGTYP_a : ARGTYP_ipcrk)))
              intyperr(csound, n, tfound, treqd);
            break;
          case 'x':
            treqd_m = ARGTYP_ipcr;*/              /* also allows i-rate */
          /*case 's':*/                             /* a- or k-rate */
            /*treqd_m |= ARGTYP_a | ARGTYP_k;
            if (tfound_m & treqd_m) {
              if (tfound == 'a' && tp->outlist != ST(nullist)) {*/
                /*long outyp_m =*/                  /* ??? */
                  /*ST(typemask_tabl)[(unsigned char) argtyp(csound,
                                                       tp->outlist->arg[0])];
                if (outyp_m & (ARGTYP_a | ARGTYP_w)) break;
              }
              else
                break;
            }
          default:
            intyperr(csound, n, tfound, treqd);
            break;
          }
        }*/
      }
      csound->DebugMsg(csound, "xincod = %d", tp->xincod);
}

void set_xoutcod(CSOUND *csound, TEXT *tp, OENTRY *ep) {
     int n = tp->outlist->count;
     char *s;

     char *types = ep->outypes;

     int nreqd = -1;
     char      tfound = '\0', treqd;

    if (nreqd < 0)    /* for other opcodes */
        nreqd = strlen(types = ep->outypes);
/*      if ((n != nreqd) &&       */        /* IV - Oct 24 2002: end of new code */
/*          !(n > 0 && n < nreqd &&
            (types[n] == (char) 'm' || types[n] == (char) 'z' ||
             types[n] == (char) 'X' || types[n] == (char) 'N'))) {
        synterr(csound, Str("illegal no of output args"));
        if (n > nreqd)
          n = nreqd;
      }*/


    while (n--) {                                     /* outargs:  */
        long    tfound_m;       /* IV - Oct 31 2002 */
        s = tp->outlist->arg[n];
        treqd = types[n];
        tfound = argtyp2(csound, s);                     /*  found    */
        /* IV - Oct 31 2002 */
        /*tfound_m = ST(typemask_tabl)[(unsigned char) tfound];*/
        /* IV - Sep 1 2002: xoutcod is the same as xincod for input */
        if (tfound == 'a' && n < 31)
          tp->xoutcod |= (1 << n);
        if (tfound == 'S' && n < 31)
          tp->xoutcod_str |= (1 << n);
        csound->DebugMsg(csound, "treqd %c, tfound %c", treqd, tfound);
        /*if (tfound_m & ARGTYP_w)
          if (ST(lgprevdef)) {
            synterr(csound, Str("output name previously used, "
                                "type '%c' must be uniquely defined"), tfound);
          }*/
        /* IV - Oct 31 2002: simplified code */
        /*if (!(tfound_m & ST(typemask_tabl_out)[(unsigned char) treqd])) {
          synterr(csound, Str("output arg '%s' illegal type"), s);
        }*/
      }
}


/**
 * Create an Opcode (OPTXT) from the AST node given. Called from
 * create_udo and create_instrument.
 */
OPTXT *create_opcode(CSOUND *csound, TREE *root, INSTRTXT *ip) {

    TEXT *tp;
    TREE *inargs, *outargs;
    OPTXT *optxt, *expOptxt, *retOptxt = NULL;
    char *arg;
    int opnum;

    int n;

    optxt = (OPTXT *) mcalloc(csound, (long)sizeof(OPTXT));
    tp = &(optxt->t);

    switch(root->type) {
        case T_LABEL:
            /* TODO - Need to verify here or elsewhere that this label isn't already defined */
            tp->opnum = LABEL;
            tp->opcod = strsav_string(csound, root->value->lexeme);

            tp->outlist = (ARGLST *) mmalloc(csound, sizeof(ARGLST));
            tp->outlist->count = 0;
            tp->inlist = (ARGLST *) mmalloc(csound, sizeof(ARGLST));
            tp->inlist->count = 0;

            ip->mdepends |= csound->opcodlst[LABEL].thread;
            ip->opdstot += csound->opcodlst[LABEL].dsblksiz;

            break;
        case T_GOTO:
        case T_IGOTO:
        case T_KGOTO:
        case T_OPCODE:
        case T_OPCODE0:
        case S_ASSIGN:
//            csound->Message(csound,
//                "create_opcode: Found node for opcode %s\n", root->value->lexeme);

            opnum = find_opcode(csound, root->value->lexeme);

            /* INITIAL SETUP */
            tp->opnum = opnum;
            tp->opcod = strsav_string(csound, csound->opcodlst[opnum].opname);
            ip->mdepends |= csound->opcodlst[opnum].thread;
            ip->opdstot += csound->opcodlst[opnum].dsblksiz;

            /* BUILD ARG LISTS */

            int incount = tree_arg_list_count(root->right);
            int outcount = tree_arg_list_count(root->left);

//            csound->Message(csound, "Tree: In Count: %d\n", incount);
//            csound->Message(csound, "Tree: Out Count: %d\n", outcount);

            size_t m = sizeof(ARGLST) + (incount - 1) * sizeof(char*);
            tp->inlist = (ARGLST*) mrealloc(csound, tp->inlist, m);
            tp->inlist->count = incount;

            m = sizeof(ARGLST) + (outcount - 1) * sizeof(char*);
            tp->outlist = (ARGLST*) mrealloc(csound, tp->outlist, m);
            tp->outlist->count = outcount;

            int argcount = 0;

            for (inargs = root->right; inargs != NULL; inargs = inargs->next) {
            /* INARGS */

//                csound->Message(csound, "IN ARG TYPE: %d\n", inargs->type);

                arg = inargs->value->lexeme;

                tp->inlist->arg[argcount++] = strsav_string(csound, arg);

                if ((n = pnum(arg)) >= 0) {
                    if (n > ip->pmax)  ip->pmax = n;
                } else {
                    lgbuild(csound, arg);
                }


            }

            /* update_lclcount(csound, ip, root->right); */

            argcount = 0;

            /* OUTARGS */
            for (outargs = root->left; outargs != NULL; outargs = outargs->next) {

                arg = outargs->value->lexeme;

                tp->outlist->arg[argcount++] =
                    strsav_string(csound, arg);

                if ((n = pnum(arg)) >= 0) {
                    if (n > ip->pmax)  ip->pmax = n;
                } else {
                    lgbuild(csound, arg);
                }

            }

            /* update_lclcount(csound, ip, root->left); */


            /* VERIFY ARG LISTS MATCH OPCODE EXPECTED TYPES */

            OENTRY *ep = csound->opcodlst + tp->opnum;

            csound->Message(csound, "Opcode InTypes: %s\n", ep->intypes);
            csound->Message(csound, "Opcode OutTypes: %s\n", ep->outypes);

            set_xincod(csound, tp, ep);
            set_xoutcod(csound, tp, ep);

            if (root->right != NULL) {
                if (ep->intypes[0] != 'l') {     /* intype defined by 1st inarg */
                    tp->intype = argtyp2(csound, tp->inlist->arg[0]);
                } else  {
                    tp->intype = 'l';          /*   (unless label)  */
                }
            }

            if (root->left != NULL) {                       /* pftype defined by outarg */
                tp->pftype = argtyp2(csound, root->left->value->lexeme);
            } else {                            /*    else by 1st inarg     */
                tp->pftype = tp->intype;
            }

            csound->Message(csound,
                "create_opcode[%s]: opnum for opcode: %d\n", root->value->lexeme, opnum);

            break;
        default:
            csound->Message(csound,
                "create_opcode: No rule to handle statemnent of type %d\n", root->type);
            print_tree(csound, root);
    }

    if(retOptxt == NULL) {
        retOptxt = optxt;
    } else {
        append_optxt(retOptxt, optxt);
    }

    return retOptxt;
}



/**
 * Create a UDO (INSTRTXT) from the AST node given. Called from
 * csound_orc_compile.
 */
INSTRTXT *create_udo(CSOUND *csound, TREE *root) {
    INSTRTXT *ip = (INSTRTXT *) mcalloc(csound, sizeof(INSTRTXT));
    return ip;
}

/**
 * Create an Instrument (INSTRTXT) from the AST node given for use as
 * Instrument0. Called from csound_orc_compile.
 */
INSTRTXT *create_instrument0(CSOUND *csound, TREE *root) {
    INSTRTXT *ip;
    OPTXT *op;

    TREE *current;

    ip = (INSTRTXT *) mcalloc(csound, sizeof(INSTRTXT));
    op = (OPTXT *)ip;

    current = root;

    /* initialize */
    ip->lclkcnt = 0;
    ip->lclwcnt = 0;
    ip->lclacnt = 0;
    ip->lclpcnt = 0;
    ip->lclscnt = 0;

    delete_local_namepool(csound);
    ST(lclnxtkcnt) = 0;                     /*   for rebuilding  */
    ST(lclnxtwcnt) = ST(lclnxtacnt) = 0;
    ST(lclnxtpcnt) = ST(lclnxtscnt) = 0;

    ip->mdepends = 0;
    ip->opdstot = 0;

    ip->pmax = 3L;

    /* start chain */
    ip->t.opnum = INSTR;
      ip->t.opcod = strsav_string(csound, "instr"); /*  to hold global assigns */

      /* The following differs from otran and needs review.  otran keeps a
       * nulllist to point to for empty lists, while this is creating a new list
       * regardless
       */
      ip->t.outlist = (ARGLST *) mmalloc(csound, sizeof(ARGLST));
    ip->t.outlist->count = 0;
    ip->t.inlist = (ARGLST *) mmalloc(csound, sizeof(ARGLST));
    ip->t.inlist->count = 1;

      ip->t.inlist->arg[0] = strsav_string(csound, "0");


    while(current != NULL) {

        if(current->type != T_INSTR && current->type != T_UDO) {

            csound->Message(csound, "In INSTR 0: %s\n", current->value->lexeme);

            if(current->type == S_ASSIGN
                && strcmp(current->value->lexeme, "=.r") == 0) {

                MYFLT val = csound->pool[constndx(csound,
                                               current->right->value->lexeme)];


                /* if(current->right->type == T_INTGR) {
                    val = FL(current->right->value->value);
                } else {
                    val = FL(current->right->value->fvalue);
                }*/



                /* modify otran defaults*/
                if (current->left->type == T_SRATE) {
                  csound->tran_sr = val;
                } else if (current->left->type == T_KRATE) {
                  csound->tran_kr = val;
                } else if (current->left->type == T_KSMPS) {
                  csound->tran_ksmps = val;
                } else if (current->left->type == T_NCHNLS) {
                  csound->tran_nchnls = current->right->value->value;
                  csound->Message(csound, "SETTING NCHNLS: %d\n", csound->tran_nchnls);
                }
                /* TODO - Implement 0dbfs constant */
                /*else if (strcmp(s, "0dbfs") == 0) {*/  /* we have set this as reserved in rdorch.c */
                  /*csound->tran_0dbfs = constval;
                }*/
            }

            OPTXT * optxt = create_opcode(csound, current, ip);

            op->nxtop = optxt;
            op = last_optxt(op);

        }
        current = current->next;
    }

    close_instrument(csound, ip);

    return ip;
}


/**
 * Create an Instrument (INSTRTXT) from the AST node given. Called from
 * csound_orc_compile.
 */
INSTRTXT *create_instrument(CSOUND *csound, TREE *root) {
    INSTRTXT *ip;
    OPTXT *op;
    char *c;

    TREE *statements, *current;

    ip = (INSTRTXT *) mcalloc(csound, sizeof(INSTRTXT));
    op = (OPTXT *)ip;
    statements = root->right;

    ip->lclkcnt = 0;
    ip->lclwcnt = 0;
    ip->lclacnt = 0;
    ip->lclpcnt = 0;
    ip->lclscnt = 0;

    delete_local_namepool(csound);
    ST(lclnxtkcnt) = 0;                     /*   for rebuilding  */
    ST(lclnxtwcnt) = ST(lclnxtacnt) = 0;
    ST(lclnxtpcnt) = ST(lclnxtscnt) = 0;


    ip->mdepends = 0;
    ip->opdstot = 0;

    ip->pmax = 3L;

    /* Initialize */
    ip->t.opnum = INSTR;
      ip->t.opcod = strsav_string(csound, "instr"); /*  to hold global assigns */

      /* The following differs from otran and needs review.  otran keeps a
       * nulllist to point to for empty lists, while this is creating a new list
       * regardless
       */
      ip->t.outlist = (ARGLST *) mmalloc(csound, sizeof(ARGLST));
    ip->t.outlist->count = 0;
    ip->t.inlist = (ARGLST *) mmalloc(csound, sizeof(ARGLST));
    ip->t.inlist->count = 1;

    /* Maybe should do this assignment at end when instr is setup?
     * Note: look into how "instr 4,5,6,8" is handled, i.e. if copies
     * are made or if they are all set to point to the same INSTRTXT
     *
     * Note2: For now am not checking if root->left is a list (i.e. checking
     * root->left->next is NULL or not to indicate list)
     */
    if(root->left->type == T_INTGR) { /* numbered instrument */
        long instrNum = (long)root->left->value->value;

        sscanf(c, "%ld", &instrNum);

        csound->Message(csound, "create_instrument: instr num %ld\n", instrNum);

        ip->t.inlist->arg[0] = strsav_string(csound, c);
    }


    current = statements;

    while(current != NULL) {
        OPTXT * optxt = create_opcode(csound, current, ip);

        op->nxtop = optxt;
        op = last_optxt(op);

        current = current->next;
    }

    close_instrument(csound, ip);

    return ip;
}

void close_instrument(CSOUND *csound, INSTRTXT * ip) {
    OPTXT * bp, *current;
    int n;

    bp = (OPTXT *) mcalloc(csound, (long)sizeof(OPTXT));

    bp->t.opnum = ENDIN;                          /*  send an endin to */
    bp->t.opcod = strsav_string(csound, "endin"); /*  term instr 0 blk */
    bp->t.outlist = bp->t.inlist = NULL;

    bp->nxtop = NULL;   /* terminate the optxt chain */

    current = (OPTXT *)ip;

    while(current->nxtop != NULL) {
        current = current->nxtop;
    }

    current->nxtop = bp;


    ip->lclkcnt = ST(lclnxtkcnt);
    /* align to 8 bytes for "spectral" types */
    if ((int) sizeof(MYFLT) < 8 &&
        (ST(lclnxtwcnt) + ST(lclnxtpcnt)) > 0)
      ip->lclkcnt = (ip->lclkcnt + 1) & (~1);
    ip->lclwcnt = ST(lclnxtwcnt);
    ip->lclacnt = ST(lclnxtacnt);
    ip->lclpcnt = ST(lclnxtpcnt);
    ip->lclscnt = ST(lclnxtscnt);
    ip->lclfixed = ST(lclnxtkcnt) + ST(lclnxtwcnt) * Wfloats
                                  + ST(lclnxtpcnt) * Pfloats;

    /* align to 8 bytes for "spectral" types */
/*    if ((int) sizeof(MYFLT) < 8 && (ip->lclwcnt + ip->lclpcnt) > 0) {
          ip->lclkcnt = (ip->lclkcnt + 1) & (~1);
    }

    ip->lclfixed = ip->lclkcnt + ip->lclwcnt * Wfloats * ip->lclpcnt * Pfloats;*/

    ip->mdepends = ip->mdepends >> 4;

    ip->pextrab = ((n = ip->pmax - 3L) > 0 ? (int) n * sizeof(MYFLT) : 0);
    ip->pextrab = ((int) ip->pextrab + 7) & (~7);

    ip->muted = 1;

    /*ip->pmax = (int)pmax;
    ip->pextrab = ((n = pmax-3L) > 0 ? (int) n * sizeof(MYFLT) : 0);
    ip->pextrab = ((int) ip->pextrab + 7) & (~7);
    ip->mdepends = threads >> 4;
    ip->lclkcnt = ST(lclnxtkcnt); */
    /* align to 8 bytes for "spectral" types */

    /*if ((int) sizeof(MYFLT) < 8 &&
        (ST(lclnxtwcnt) + ST(lclnxtpcnt)) > 0)
      ip->lclkcnt = (ip->lclkcnt + 1) & (~1);
    ip->lclwcnt = ST(lclnxtwcnt);
    ip->lclacnt = ST(lclnxtacnt);
    ip->lclpcnt = ST(lclnxtpcnt);
    ip->lclscnt = ST(lclnxtscnt);
    ip->lclfixed = ST(lclnxtkcnt) + ST(lclnxtwcnt) * Wfloats
                                  + ST(lclnxtpcnt) * Pfloats;*/
    /*ip->opdstot = opdstot;*/      /* store total opds reqd */
    /*ip->muted = 1;*/              /* Allow to play */


}



/**
 * Append an instrument to the end of Csound's linked list of instruments
 */
void append_instrument(CSOUND * csound, INSTRTXT * instrtxt) {
    INSTRTXT    *current = &(csound->instxtanchor);
    while(current->nxtinstxt != NULL) {
        current = current->nxtinstxt;
    }

    current->nxtinstxt = instrtxt;
}


/* IV - Oct 12 2002: new function to parse arguments of opcode definitions */

static int parse_opcode_args(CSOUND *csound, OENTRY *opc)
{
    OPCODINFO   *inm = (OPCODINFO*) opc->useropinfo;
    char    *types, *otypes;
    int     i, i_incnt, a_incnt, k_incnt, i_outcnt, a_outcnt, k_outcnt, err;
    short   *a_inlist, *k_inlist, *i_inlist, *a_outlist, *k_outlist, *i_outlist;

    /* count the number of arguments, and check types */
    i = i_incnt = a_incnt = k_incnt = i_outcnt = a_outcnt = k_outcnt = err = 0;
    types = inm->intypes; otypes = opc->intypes;
    opc->dsblksiz = (unsigned short) sizeof(UOPCODE);
    if (!strcmp(types, "0"))
      types++;                  /* no input args */
    while (*types) {
      switch (*types) {
      case 'a':
        a_incnt++; *otypes++ = *types;
        break;
      case 'K':
        i_incnt++;              /* also updated at i-time */
      case 'k':
        k_incnt++; *otypes++ = 'k';
        break;
      case 'i':
      case 'o':
      case 'p':
      case 'j':
        i_incnt++; *otypes++ = *types;
        break;
      default:
        synterr(csound, "invalid input type for opcode %s", inm->name);
        err++; i--;
      }
      i++; types++;
      if (i > OPCODENUMOUTS_MAX) {
        synterr(csound, "too many input args for opcode %s", inm->name);
        csound->LongJmp(csound, 1);
      }
    }
    *otypes++ = 'o'; *otypes = '\0';    /* optional arg for local ksmps */
    inm->inchns = i;                    /* total number of input chnls */
    inm->perf_incnt = a_incnt + k_incnt;
    opc->dsblksiz += (unsigned short) (sizeof(MYFLT*) * i);
    /* same for outputs */
    i = 0;
    types = inm->outtypes; otypes = opc->outypes;
    if (!strcmp(types, "0"))
      types++;                  /* no output args */
    while (*types) {
      if (i >= OPCODENUMOUTS_MAX) {
        synterr(csound, "too many output args for opcode %s", inm->name);
        csound->LongJmp(csound, 1);
      }
      switch (*types) {
      case 'a':
        a_outcnt++; *otypes++ = *types;
        break;
      case 'K':
        i_outcnt++;             /* also updated at i-time */
      case 'k':
        k_outcnt++; *otypes++ = 'k';
        break;
      case 'i':
        i_outcnt++; *otypes++ = *types;
        break;
      default:
        synterr(csound, "invalid output type for opcode %s", inm->name);
        err++; i--;
      }
      i++; types++;
    }
    *otypes = '\0';
    inm->outchns = i;                   /* total number of output chnls */
    inm->perf_outcnt = a_outcnt + k_outcnt;
    opc->dsblksiz += (unsigned short) (sizeof(MYFLT*) * i);
    opc->dsblksiz = ((opc->dsblksiz + (unsigned short) 15)
                     & (~((unsigned short) 15)));   /* align (needed ?) */
    /* now build index lists for the various types of arguments */
    i = i_incnt + inm->perf_incnt + i_outcnt + inm->perf_outcnt;
    i_inlist = inm->in_ndx_list = (short*) mmalloc(csound,
                                                   sizeof(short) * (i + 6));
    a_inlist = i_inlist + i_incnt + 1;
    k_inlist = a_inlist + a_incnt + 1;
    i = 0; types = inm->intypes;
    while (*types) {
      switch (*types++) {
        case 'a': *a_inlist++ = i; break;
        case 'k': *k_inlist++ = i; break;
        case 'K': *k_inlist++ = i;      /* also updated at i-time */
        case 'i':
        case 'o':
        case 'p':
        case 'j': *i_inlist++ = i;
      }
      i++;
    }
    *i_inlist = *a_inlist = *k_inlist = -1;     /* put delimiters */
    i_outlist = inm->out_ndx_list = k_inlist + 1;
    a_outlist = i_outlist + i_outcnt + 1;
    k_outlist = a_outlist + a_outcnt + 1;
    i = 0; types = inm->outtypes;
    while (*types) {
      switch (*types++) {
        case 'a': *a_outlist++ = i; break;
        case 'k': *k_outlist++ = i; break;
        case 'K': *k_outlist++ = i;     /* also updated at i-time */
        case 'i': *i_outlist++ = i;
      }
      i++;
    }
    *i_outlist = *a_outlist = *k_outlist = -1;  /* put delimiters */
    return err;
}

static int pnum(char *s)        /* check a char string for pnum format  */
                                /*   and return the pnum ( >= 0 )       */
{                               /* else return -1                       */
    int n;

    if (*s == 'p' || *s == 'P')
      if (sscanf(++s, "%d", &n))
        return(n);
    return(-1);
}

/** Insert INSTRTXT into Csound's list of INSTRTXT's, checking to see if number
 * is greater than number of pointers currently allocated and if so expand
 * pool of instruments
 */
void insert_instrtxt(CSOUND *csound, INSTRTXT *instrtxt, long instrNum) {
    int i;

     if (instrNum > csound->maxinsno) {
        int old_maxinsno = csound->maxinsno;

        /* expand */
        while (instrNum > csound->maxinsno) {
            csound->maxinsno += MAXINSNO;
        }

        csound->instrtxtp = (INSTRTXT**)mrealloc(csound,
                csound->instrtxtp, (1 + csound->maxinsno) * sizeof(INSTRTXT*));

        /* Array expected to be nulled so.... */
        for (i = old_maxinsno + 1; i <= csound->maxinsno; i++) {
              csound->instrtxtp[i] = NULL;
        }
    }

    if (csound->instrtxtp[instrNum] != NULL) {
        synterr(csound, Str("instr %ld redefined"), instrNum);
        /* err++; continue; */
    }

    csound->instrtxtp[instrNum] = instrtxt;
}

/**
 * Compile the given TREE node into structs for Csound to use
 */
void csound_orc_compile(CSOUND *csound, TREE *root) {
    csound->Message(csound, "Begin Compiling AST (Currently Implementing)\n");

    OPARMS      *O = csound->oparms;
    TEXT        *tp;
    int         init = 1;
    INSTRTXT    *ip = NULL;
    INSTRTXT    *prvinstxt = &(csound->instxtanchor);
    OPTXT       *bp, *prvbp = NULL;
    ARGLST      *alp;
    char        *s;
    long        pmax = -1, nn;
    long        n, opdstot = 0, count, sumcount, instxtcount, optxtcount;

    strsav_create(csound);

    if (csound->otranGlobals == NULL) {
      csound->otranGlobals = csound->Calloc(csound, sizeof(OTRAN_GLOBALS));
    }
    csound->instrtxtp = (INSTRTXT **) mcalloc(csound, (1 + csound->maxinsno)
                                                      * sizeof(INSTRTXT*));
    csound->opcodeInfo = NULL;          /* IV - Oct 20 2002 */

    strconstndx(csound, "\"\"");

    gblnamset(csound, "sr");    /* enter global reserved words */
    gblnamset(csound, "kr");
    gblnamset(csound, "ksmps");
    gblnamset(csound, "nchnls");
    gblnamset(csound, "0dbfs"); /* no commandline override for that! */
    gblnamset(csound, "$sr");   /* incl command-line overrides */
    gblnamset(csound, "$kr");
    gblnamset(csound, "$ksmps");

    csound->pool = (MYFLT*) mmalloc(csound, NCONSTS * sizeof(MYFLT));
    ST(poolcount) = 0;
    ST(nconsts) = NCONSTS;
    ST(constTbl) = (int*) mcalloc(csound, (256 + NCONSTS) * sizeof(int));
    constndx(csound, "0");


    TREE * current = root;

    INSTRTXT * instr0 = create_instrument0(csound, root);
    prvinstxt = prvinstxt->nxtinstxt = instr0;
    insert_instrtxt(csound, instr0, 0);

    while(current != NULL) {

        switch(current->type) {
            case S_ASSIGN:
                csound->Message(csound, "Assignment found\n");
                break;
            case T_INSTR:
                csound->Message(csound, "Instrument found\n");

                resetouts(csound); /* reset #out counts */
                lblclear(csound); /* restart labelist  */

                INSTRTXT * instrtxt = create_instrument(csound, current);

                prvinstxt = prvinstxt->nxtinstxt = instrtxt;

                /* Handle Inserting into CSOUND here by checking id's (name or
                 * numbered) and using new insert_instrtxt?
                 */

                /* Temporarily using the following code */
                    if(current->left->type == T_INTGR) { /* numbered instrument */
                        long instrNum = (long)current->left->value->value;

                        insert_instrtxt(csound, instrtxt, instrNum);
                    }

                break;
            case T_UDO:
                csound->Message(csound, "UDO found\n");

                resetouts(csound); /* reset #out counts */
                lblclear(csound); /* restart labelist  */


                break;
            default:
                csound->Message(csound, "Unknown TREE node of type %d found in root.\n", current->type);
                print_tree(csound, current);
        }

        current = current->next;

    }

    /* Begin code from otran */
    /* now add the instruments with names, assigning them fake instr numbers */
    named_instr_assign_numbers(csound);         /* IV - Oct 31 2002 */
    if (csound->opcodeInfo) {
      int num = csound->maxinsno;       /* store after any other instruments */
      OPCODINFO *inm = csound->opcodeInfo;
      /* IV - Oct 31 2002: now add user defined opcodes */
      while (inm) {
        /* we may need to expand the instrument array */
        if (++num > csound->maxopcno) {
          int i;
          i = (csound->maxopcno > 0 ? csound->maxopcno : csound->maxinsno);
          csound->maxopcno = i + MAXINSNO;
          csound->instrtxtp = (INSTRTXT**)
            mrealloc(csound, csound->instrtxtp, (1 + csound->maxopcno)
                                                * sizeof(INSTRTXT*));
          /* Array expected to be nulled so.... */
          while (++i <= csound->maxopcno) csound->instrtxtp[i] = NULL;
        }
        inm->instno = num;
        csound->instrtxtp[num] = inm->ip;
        inm = inm->prv;
      }
    }
    /* Deal with defaults and consistency */
    if (csound->tran_ksmps == FL(-1.0)) {
      if (csound->tran_sr == FL(-1.0)) csound->tran_sr = DFLT_SR;
      if (csound->tran_kr == FL(-1.0)) csound->tran_kr = DFLT_KR;
      csound->tran_ksmps = (MYFLT) ((int) (csound->tran_sr / csound->tran_kr
                                           + FL(0.5)));
    }
    else if (csound->tran_kr == FL(-1.0)) {
      if (csound->tran_sr == FL(-1.0)) csound->tran_sr = DFLT_SR;
      csound->tran_kr = csound->tran_sr / csound->tran_ksmps;
    }
    else if (csound->tran_sr == FL(-1.0)) {
      csound->tran_sr = csound->tran_kr * csound->tran_ksmps;
    }
    /* That deals with missing values, however we do need ksmps to be integer */
    {
      CSOUND    *p = (CSOUND*) csound;
      char      err_msg[128];
      sprintf(err_msg, "sr = %.7g, kr = %.7g, ksmps = %.7g\nerror:",
                       p->tran_sr, p->tran_kr, p->tran_ksmps);
      if (p->tran_sr <= FL(0.0))
        synterr(p, Str("%s invalid sample rate"), err_msg);
      if (p->tran_kr <= FL(0.0))
        synterr(p, Str("%s invalid control rate"), err_msg);
      else if (p->tran_ksmps < FL(0.75) ||
               FLOAT_COMPARE(p->tran_ksmps, MYFLT2LRND(p->tran_ksmps)))
        synterr(p, Str("%s invalid ksmps value"), err_msg);
      else if (FLOAT_COMPARE(p->tran_sr, (double) p->tran_kr * p->tran_ksmps))
        synterr(p, Str("%s inconsistent sr, kr, ksmps"), err_msg);
    }

    ip = csound->instxtanchor.nxtinstxt;
    bp = (OPTXT *) ip;
    while (bp != (OPTXT *) NULL && (bp = bp->nxtop) != NULL) {
      /* chk instr 0 for illegal perfs */
      int thread, opnum = bp->t.opnum;
      if (opnum == ENDIN) break;
      if (opnum == LABEL) continue;
      if ((thread = csound->opcodlst[opnum].thread) & 06 ||
          (!thread && bp->t.pftype != 'b'))
        synterr(csound, Str("perf-pass statements illegal in header blk\n"));
    }
    if (csound->synterrcnt) {
      print_opcodedir_warning(csound);
      csound->Die(csound, Str("%d syntax errors in orchestra.  "
                              "compilation invalid\n"), csound->synterrcnt);
    }
    if (O->odebug) {
      long  n;
      MYFLT *p;
      csound->Message(csound, "poolcount = %ld, strpool_cnt = %ld\n",
                              ST(poolcount), ST(strpool_cnt));
      csound->Message(csound, "pool:");
      for (n = ST(poolcount), p = csound->pool; n--; p++)
        csound->Message(csound, "\t%g", *p);
      csound->Message(csound, "\n");
      csound->Message(csound, "strpool:");
      for (n = 0L; n < ST(strpool_cnt); n++)
        csound->Message(csound, "\t%s", ST(strpool)[n]);
      csound->Message(csound, "\n");
    }
    ST(gblfixed) = ST(gblnxtkcnt) + ST(gblnxtpcnt) * (int) Pfloats;
    ST(gblkcount) = ST(gblnxtkcnt);
    /* align to 8 bytes for "spectral" types */
    if ((int) sizeof(MYFLT) < 8 && ST(gblnxtpcnt))
      ST(gblkcount) = (ST(gblkcount) + 1) & (~1);
    ST(gblacount) = ST(gblnxtacnt);
    ST(gblscount) = ST(gblnxtscnt);

    ip = &(csound->instxtanchor);
    for (sumcount = 0; (ip = ip->nxtinstxt) != NULL; ) {/* for each instxt */
      OPTXT *optxt = (OPTXT *) ip;
      int optxtcount = 0;
      while ((optxt = optxt->nxtop) != NULL) {      /* for each op in instr  */
        TEXT *ttp = &optxt->t;
        optxtcount += 1;
        if (ttp->opnum == ENDIN                     /*    (until ENDIN)      */
            || ttp->opnum == ENDOP) break;  /* (IV - Oct 26 2002: or ENDOP) */
        if ((count = ttp->inlist->count)!=0)
          sumcount += count +1;                     /* count the non-nullist */
        if ((count = ttp->outlist->count)!=0)       /* slots in all arglists */
          sumcount += (count + 1);
      }
      ip->optxtcount = optxtcount;                  /* optxts in this instxt */
    }
    ST(argoffsize) = (sumcount + 1) * sizeof(int);  /* alloc all plus 1 null */
    /* as argoff ints */
    csound->argoffspace = (int*) mmalloc(csound, ST(argoffsize));
    ST(nxtargoffp) = csound->argoffspace;
    ST(nulloffs) = (ARGOFFS *) csound->argoffspace; /* setup the null argoff */
    *ST(nxtargoffp)++ = 0;
    ST(argofflim) = ST(nxtargoffp) + sumcount;
    ip = &(csound->instxtanchor);
    while ((ip = ip->nxtinstxt) != NULL)        /* add all other entries */
      insprep(csound, ip);                      /*   as combined offsets */
    if (O->odebug) {
      int *p = csound->argoffspace;
      csound->Message(csound, "argoff array:\n");
      do {
        csound->Message(csound, "\t%d", *p++);
      } while (p < ST(argofflim));
      csound->Message(csound, "\n");
    }
    if (ST(nxtargoffp) != ST(argofflim))
      csoundDie(csound, Str("inconsistent argoff sumcount"));

    ip = &(csound->instxtanchor);               /* set the OPARMS values */
    instxtcount = optxtcount = 0;
    while ((ip = ip->nxtinstxt) != NULL) {
      instxtcount += 1;
      optxtcount += ip->optxtcount;
    }
    csound->instxtcount = instxtcount;
    csound->optxtsize = instxtcount * sizeof(INSTRTXT)
                        + optxtcount * sizeof(OPTXT);
    csound->poolcount = ST(poolcount);
    csound->gblfixed = ST(gblnxtkcnt) + ST(gblnxtpcnt) * (int) Pfloats;
    csound->gblacount = ST(gblnxtacnt);
    csound->gblscount = ST(gblnxtscnt);
    /* clean up */
    delete_local_namepool(csound);
    delete_global_namepool(csound);
    mfree(csound, ST(constTbl));
    ST(constTbl) = NULL;
    /* End code from otran */

    csound->Message(csound, "End Compiling AST\n");

}


/* prep an instr template for efficient allocs  */
/* repl arg refs by offset ndx to lcl/gbl space */
static void insprep(CSOUND *csound, INSTRTXT *tp)
{
    OPARMS      *O = csound->oparms;
    OPTXT       *optxt;
    OENTRY      *ep;
    int         n, opnum, inreqd;
    char        **argp;
    char        **labels, **lblsp;
    LBLARG      *larg, *largp;
    ARGLST      *outlist, *inlist;
    ARGOFFS     *outoffs, *inoffs;
    int         indx, *ndxp;

    labels = (char **)mmalloc(csound, (csound->nlabels) * sizeof(char *));
    lblsp = labels;
    larg = (LBLARG *)mmalloc(csound, (csound->ngotos) * sizeof(LBLARG));
    largp = larg;
    ST(lclkcnt) = tp->lclkcnt;
    ST(lclwcnt) = tp->lclwcnt;
    ST(lclfixed) = tp->lclfixed;
    ST(lclpcnt) = tp->lclpcnt;
    ST(lclscnt) = tp->lclscnt;
    ST(lclacnt) = tp->lclacnt;
    delete_local_namepool(csound);              /* clear lcl namlist */
    ST(lclnxtkcnt) = 0;                         /*   for rebuilding  */
    ST(lclnxtwcnt) = ST(lclnxtacnt) = 0;
    ST(lclnxtpcnt) = ST(lclnxtscnt) = 0;
    ST(lclpmax) = tp->pmax;                     /* set pmax for plgndx */
    ndxp = ST(nxtargoffp);
    optxt = (OPTXT *)tp;
    while ((optxt = optxt->nxtop) != NULL) {    /* for each op in instr */
      TEXT *ttp = &optxt->t;
      if ((opnum = ttp->opnum) == ENDIN         /*  (until ENDIN)  */
          || opnum == ENDOP)            /* (IV - Oct 31 2002: or ENDOP) */
        break;
      if (opnum == LABEL) {
        if (lblsp - labels >= csound->nlabels) {
          int oldn = lblsp - labels;
          csound->nlabels += NLABELS;
          if (lblsp - labels >= csound->nlabels)
            csound->nlabels = lblsp - labels + 2;
          if (csound->oparms->msglevel)
            csound->Message(csound,
                            Str("LABELS list is full...extending to %d\n"),
                            csound->nlabels);
          labels =
            (char**)mrealloc(csound, labels, csound->nlabels*sizeof(char*));
          lblsp = &labels[oldn];
        }
        *lblsp++ = ttp->opcod;
        continue;
      }
      ep = &(csound->opcodlst[opnum]);
      if (O->odebug) csound->Message(csound, "%s argndxs:", ep->opname);
      if ((outlist = ttp->outlist) == ST(nullist) || !outlist->count)
        ttp->outoffs = ST(nulloffs);
      else {
        ttp->outoffs = outoffs = (ARGOFFS *) ndxp;
        outoffs->count = n = outlist->count;
        argp = outlist->arg;                    /* get outarg indices */
        ndxp = outoffs->indx;
        while (n--) {
          *ndxp++ = indx = plgndx(csound, *argp++);
          if (O->odebug) csound->Message(csound, "\t%d", indx);
        }
      }
      if ((inlist = ttp->inlist) == ST(nullist) || !inlist->count)
        ttp->inoffs = ST(nulloffs);
      else {
        ttp->inoffs = inoffs = (ARGOFFS *) ndxp;
        inoffs->count = inlist->count;
        inreqd = strlen(ep->intypes);
        argp = inlist->arg;                     /* get inarg indices */
        ndxp = inoffs->indx;
        for (n=0; n < inlist->count; n++, argp++, ndxp++) {
          if (n < inreqd && ep->intypes[n] == 'l') {
            if (largp - larg >= csound->ngotos) {
              int oldn = csound->ngotos;
              csound->ngotos += NGOTOS;
              if (csound->oparms->msglevel)
                csound->Message(csound,
                                Str("GOTOS list is full..extending to %d\n"),
                                csound->ngotos);
              if (largp - larg >= csound->ngotos)
                csound->ngotos = largp - larg + 1;
              larg = (LBLARG *)
                mrealloc(csound, larg, csound->ngotos * sizeof(LBLARG));
              largp = &larg[oldn];
            }
            if (O->odebug)
              csound->Message(csound, "\t***lbl");  /* if arg is label,  */
            largp->lbltxt = *argp;
            largp->ndxp = ndxp;                     /*  defer till later */
            largp++;
          }
          else {
            char *s = *argp;
            indx = plgndx(csound, s);
            if (O->odebug) csound->Message(csound, "\t%d", indx);
            *ndxp = indx;
          }
        }
      }
      if (O->odebug) csound->Message(csound, "\n");
    }
 nxt:
    while (--largp >= larg) {                   /* resolve the lbl refs */
      char *s = largp->lbltxt;
      char **lp;
      for (lp = labels; lp < lblsp; lp++)
        if (strcmp(s, *lp) == 0) {
          *largp->ndxp = lp - labels + LABELOFS;
          goto nxt;
        }
      csoundDie(csound, Str("target label '%s' not found"), s);
    }
    ST(nxtargoffp) = ndxp;
    mfree(csound, labels);
    mfree(csound, larg);
}

/* build pool of floating const values  */
/* build lcl/gbl list of ds names, offsets */
/* (no need to save the returned values) */
static void lgbuild(CSOUND *csound, char *s)
{
    char    c;

    c = *s;
    /* must trap 0dbfs as name starts with a digit! */
    if ((c >= '1' && c <= '9') || c == '.' || c == '-' || c == '+' ||
        (c == '0' && strcmp(s, "0dbfs") != 0))
      constndx(csound, s);
    else if (c == '"')
      strconstndx(csound, s);
    else if (!(lgexist(csound, s))) {
      if (c == 'g' || (c == '#' && s[1] == 'g'))
        gblnamset(csound, s);
      else
        lclnamset(csound, s);
    }
}

/* get storage ndx of const, pnum, lcl or gbl */
/* argument const/gbl indexes are positiv+1, */
/* pnum/lcl negativ-1 called only after      */
/* poolcount & lclpmax are finalised */
static int plgndx(CSOUND *csound, char *s)
{
    char        c;
    int         n, indx;

    c = *s;

    /* must trap 0dbfs as name starts with a digit! */
    if ((c >= '1' && c <= '9') || c == '.' || c == '-' || c == '+' ||
        (c == '0' && strcmp(s, "0dbfs") != 0))
      indx = constndx(csound, s) + 1;
    else if (c == '"')
      indx = strconstndx(csound, s) + STR_OFS + 1;
    else if ((n = pnum(s)) >= 0)
      indx = -n;
    else if (c == 'g' || (c == '#' && *(s+1) == 'g') || gexist(csound, s))
      indx = (int) (ST(poolcount) + 1 + gbloffndx(csound, s));
    else
      indx = -(ST(lclpmax) + 1 + lcloffndx(csound, s));
/*    csound->Message(csound, " [%s -> %d (%x)]\n", s, indx, indx); */
    return(indx);
}

/* get storage ndx of string const value */
/* builds value pool on 1st occurrence   */
static int strconstndx(CSOUND *csound, const char *s)
{
    int     i, cnt;

    /* check syntax */
    cnt = (int) strlen(s);
    if (cnt < 2 || *s != '"' || s[cnt - 1] != '"') {
      synterr(csound, Str("string syntax '%s'"), s);
      return 0;
    }
    /* check if a copy of the string is already stored */
    for (i = 0; i < ST(strpool_cnt); i++) {
      if (strcmp(s, ST(strpool)[i]) == 0)
        return i;
    }
    /* not found, store new string */
    cnt = ST(strpool_cnt)++;
    if (!(cnt & 0x7F)) {
      /* extend list */
      if (!cnt) ST(strpool) = csound->Malloc(csound, 0x80 * sizeof(MYFLT*));
      else      ST(strpool) = csound->ReAlloc(csound, ST(strpool),
                                              (cnt + 0x80) * sizeof(MYFLT*));
    }
    ST(strpool)[cnt] = (char*) csound->Malloc(csound, strlen(s) + 1);
    strcpy(ST(strpool)[cnt], s);
    /* and return index */
    return cnt;
}

static inline unsigned int MYFLT_hash(const MYFLT *x)
{
    const unsigned char *c = (const unsigned char*) x;
    size_t              i;
    unsigned int        h = 0U;

    for (i = (size_t) 0; i < sizeof(MYFLT); i++)
      h = (unsigned int) strhash_tabl_8[(unsigned int) c[i] ^ h];

    return h;
}

/* get storage ndx of float const value */
/* builds value pool on 1st occurrence  */
/* final poolcount used in plgndx above */
/* pool may be moved w. ndx still valid */

static int constndx(CSOUND *csound, const char *s)
{
    MYFLT   newval;
    int     h, n, prv;

    {
      volatile MYFLT  tmpVal;   /* make sure it really gets rounded to MYFLT */
      char            *tmp = (char*) s;
      tmpVal = (MYFLT) strtod(s, &tmp);
      newval = tmpVal;
      if (tmp == s || *tmp != (char) 0) {
        synterr(csound, Str("numeric syntax '%s'"), s);
        return 0;
      }
    }
    /* calculate hash value (0 to 255) */
    h = (int) MYFLT_hash(&newval);
    n = ST(constTbl)[h];                        /* now search constpool */
    prv = 0;
    while (n) {
      if (csound->pool[n - 256] == newval) {    /* if val is there      */
        if (prv) {
          /* move to the beginning of the chain, so that */
          /* frequently searched values are found faster */
          ST(constTbl)[prv] = ST(constTbl)[n];
          ST(constTbl)[n] = ST(constTbl)[h];
          ST(constTbl)[h] = n;
        }
        return (n - 256);                       /*    return w. index   */
      }
      prv = n;
      n = ST(constTbl)[prv];
    }
    n = ST(poolcount)++;
    if (n >= ST(nconsts)) {
      ST(nconsts) = ((ST(nconsts) + (ST(nconsts) >> 3)) | (NCONSTS - 1)) + 1;
      if (csound->oparms->msglevel)
        csound->Message(csound, Str("extending Floating pool to %d\n"),
                                ST(nconsts));
      csound->pool = (MYFLT*) mrealloc(csound, csound->pool, ST(nconsts)
                                                             * sizeof(MYFLT));
      ST(constTbl) = (int*) mrealloc(csound, ST(constTbl), (256 + ST(nconsts))
                                                           * sizeof(int));
    }
    csound->pool[n] = newval;                   /* else enter newval    */
    ST(constTbl)[n + 256] = ST(constTbl)[h];    /*   link into chain    */
    ST(constTbl)[h] = n + 256;

    return n;                                   /*   and return new ndx */
}

/* tests whether variable name exists   */
/*      in gbl namelist                 */

static int gexist(CSOUND *csound, char *s)
{
    unsigned char h = name_hash(csound, s);
    NAME          *p;

    for (p = ST(gblNames)[h]; p != NULL && sCmp(p->namep, s); p = p->nxt);
    return (p == NULL ? 0 : 1);
}


/* builds namelist & type counts for gbl names */

static void gblnamset(CSOUND *csound, char *s)
{
    unsigned char h = name_hash(csound, s);
    NAME          *p = ST(gblNames)[h];
                                                /* search gbl namelist: */
    for ( ; p != NULL && sCmp(p->namep, s); p = p->nxt);
    if (p != NULL)                              /* if name is there     */
      return;                                   /*    return            */
    p = (NAME*) malloc(sizeof(NAME));
    if (p == NULL)
      csound->Die(csound, Str("gblnamset(): memory allocation failure"));
    p->namep = s;                               /* else record newname  */
    p->nxt = ST(gblNames)[h];
    ST(gblNames)[h] = p;
    if (*s == '#')  s++;
    if (*s == 'g')  s++;
    switch ((int) *s) {                         /*   and its type-count */
      case 'a': p->type = ATYPE; p->count = ST(gblnxtacnt)++; break;
      case 'S': p->type = STYPE; p->count = ST(gblnxtscnt)++; break;
      case 'f': p->type = PTYPE; p->count = ST(gblnxtpcnt)++; break;
      default:  p->type = KTYPE; p->count = ST(gblnxtkcnt)++;
    }
}

/* builds namelist & type counts for lcl names  */
/*  called by otran for each instr for lcl cnts */
/*  lists then redone by insprep via lcloffndx  */

static NAME *lclnamset(CSOUND *csound, char *s)
{
    unsigned char h = name_hash(csound, s);
    NAME          *p = ST(lclNames)[h];
                                                /* search lcl namelist: */
    for ( ; p != NULL && sCmp(p->namep, s); p = p->nxt);
    if (p != NULL)                              /* if name is there     */
      return p;                                 /*    return ptr        */
    p = (NAME*) malloc(sizeof(NAME));
    if (p == NULL)
      csound->Die(csound, Str("lclnamset(): memory allocation failure"));
    p->namep = s;                               /* else record newname  */
    p->nxt = ST(lclNames)[h];
    ST(lclNames)[h] = p;
    if (*s == '#')  s++;
    switch (*s) {                               /*   and its type-count */
      case 'w': p->type = WTYPE; p->count = ST(lclnxtwcnt)++; break;
      case 'a': p->type = ATYPE; p->count = ST(lclnxtacnt)++; break;
      case 'f': p->type = PTYPE; p->count = ST(lclnxtpcnt)++; break;
      case 'S': p->type = STYPE; p->count = ST(lclnxtscnt)++; break;
      default:  p->type = KTYPE; p->count = ST(lclnxtkcnt)++; break;
    }
    return p;
}

/* get named offset index into gbl dspace     */
/* called only after otran and gblfixed valid */

static int gbloffndx(CSOUND *csound, char *s)
{
    unsigned char h = name_hash(csound, s);
    NAME          *p = ST(gblNames)[h];

    for ( ; p != NULL && sCmp(p->namep, s); p = p->nxt);
    if (p == NULL)
      csoundDie(csound, Str("unexpected global name"));
    switch (p->type) {
      case ATYPE: return (ST(gblfixed) + p->count);
      case STYPE: return (ST(gblfixed) + ST(gblacount) + p->count);
      case PTYPE: return (ST(gblkcount) + p->count * (int) Pfloats);
    }
    return p->count;
}

/* get named offset index into instr lcl dspace   */
/* called by insprep aftr lclcnts, lclfixed valid */

static int lcloffndx(CSOUND *csound, char *s)
{
    NAME    *np = lclnamset(csound, s);         /* rebuild the table    */

    switch (np->type) {                         /* use cnts to calc ndx */
      case KTYPE: return np->count;
      case WTYPE: return (ST(lclkcnt) + np->count * Wfloats);
      case ATYPE: return (ST(lclfixed) + np->count);
      case PTYPE: return (ST(lclkcnt) + ST(lclwcnt) * Wfloats
                                      + np->count * (int) Pfloats);
      case STYPE: return (ST(lclfixed) + ST(lclacnt) + np->count);
      default:    csoundDie(csound, Str("unknown nametype"));
    }
    return 0;
}

static void delete_global_namepool(CSOUND *csound)
{
    int i;

    if (csound->otranGlobals == NULL)
      return;
    for (i = 0; i < 256; i++) {
      while (ST(gblNames)[i] != NULL) {
        NAME  *nxt = ST(gblNames)[i]->nxt;
        free(ST(gblNames)[i]);
        ST(gblNames)[i] = nxt;
      }
    }
}

static void delete_local_namepool(CSOUND *csound)
{
    int i;

    if (csound->otranGlobals == NULL)
      return;
    for (i = 0; i < 256; i++) {
      while (ST(lclNames)[i] != NULL) {
        NAME  *nxt = ST(lclNames)[i]->nxt;
        free(ST(lclNames)[i]);
        ST(lclNames)[i] = nxt;
      }
    }
}

 /* ------------------------------------------------------------------------ */

/* get size of string in MYFLT units */

static int strlen_to_samples(const char *s)
{
    int n = (int) strlen(s);
    n = (n + (int) sizeof(MYFLT)) / (int) sizeof(MYFLT);
    return n;
}

/* convert string constant */

static void unquote_string(char *dst, const char *src)
{
    int i, j, n = (int) strlen(src) - 1;
    for (i = 1, j = 0; i < n; i++) {
      if (src[i] != '\\')
        dst[j++] = src[i];
      else {
        switch (src[++i]) {
        case 'a':   dst[j++] = '\a';  break;
        case 'b':   dst[j++] = '\b';  break;
        case 'f':   dst[j++] = '\f';  break;
        case 'n':   dst[j++] = '\n';  break;
        case 'r':   dst[j++] = '\r';  break;
        case 't':   dst[j++] = '\t';  break;
        case 'v':   dst[j++] = '\v';  break;
        case '"':   dst[j++] = '"';   break;
        case '\\':  dst[j++] = '\\';  break;
        default:
          if (src[i] >= '0' && src[i] <= '7') {
            int k = 0, l = (int) src[i] - '0';
            while (++k < 3 && src[i + 1] >= '0' && src[i + 1] <= '7')
              l = (l << 3) | ((int) src[++i] - '0');
            dst[j++] = (char) l;
          }
          else {
            dst[j++] = '\\'; i--;
          }
        }
      }
    }
    dst[j] = '\0';
}

static int create_strconst_ndx_list(CSOUND *csound, int **lst, int offs)
{
    int     *ndx_lst;
    char    **strpool;
    int     strpool_cnt, ndx, i;

    strpool_cnt = ST(strpool_cnt);
    strpool = ST(strpool);
    /* strpool_cnt always >= 1 because of empty string at index 0 */
    ndx_lst = (int*) csound->Malloc(csound, strpool_cnt * sizeof(int));
    for (i = 0, ndx = offs; i < strpool_cnt; i++) {
      ndx_lst[i] = ndx;
      ndx += strlen_to_samples(strpool[i]);
    }
    *lst = ndx_lst;
    /* return with total size in MYFLT units */
    return (ndx - offs);
}

static void convert_strconst_pool(CSOUND *csound, MYFLT *dst)
{
    char    **strpool, *s;
    int     strpool_cnt, ndx, i;

    strpool_cnt = ST(strpool_cnt);
    strpool = ST(strpool);
    if (strpool == NULL)
      return;
    for (ndx = i = 0; i < strpool_cnt; i++) {
      s = (char*) ((MYFLT*) dst + (int) ndx);
      unquote_string(s, strpool[i]);
      ndx += strlen_to_samples(strpool[i]);
    }
    /* original pool is no longer needed */
    ST(strpool) = NULL;
    ST(strpool_cnt) = 0;
    for (i = 0; i < strpool_cnt; i++)
      csound->Free(csound, strpool[i]);
    csound->Free(csound, strpool);
}

char argtyp2(CSOUND *csound, char *s)
{                       /* find arg type:  d, w, a, k, i, c, p, r, S, B, b */
    char c = *s;        /*   also set lgprevdef if !c && !p && !S */

    csound->Message(csound, "\nArgtyp2: received %s\n", s);

    /*trap this before parsing for a number! */
    /* two situations: defined at header level: 0dbfs = 1.0
     *  and returned as a value:  idb = 0dbfs
     */
    if ((c >= '1' && c <= '9') || c == '.' || c == '-' || c == '+' ||
        (c == '0' && strcmp(s, "0dbfs") != 0))
      return('c');                              /* const */
    if (pnum(s) >= 0)
      return('p');                              /* pnum */
    if (c == '"')
      return('S');                              /* quoted String */
      /* ST(lgprevdef) = lgexist(csound, s);  */             /* (lgprev) */
    if (strcmp(s,"sr") == 0    || strcmp(s,"kr") == 0 ||
        strcmp(s,"0dbfs") == 0 ||
        strcmp(s,"ksmps") == 0 || strcmp(s,"nchnls") == 0)
      return('r');                              /* rsvd */
    if (c == 'w')               /* N.B. w NOT YET #TYPE OR GLOBAL */
      return(c);
    if (c == '#')
      c = *(++s);
    if (c == 'g')
      c = *(++s);
    if (strchr("akiBbfS", c) != NULL)
      return(c);
    else return('?');
}