/****************************************************************************
*
*                            Open Watcom Project
*
* Copyright (c) 2024      The Open Watcom Contributors. All Rights Reserved.
*    Portions Copyright (c) 1983-2002 Sybase, Inc. All Rights Reserved.
*
*  ========================================================================
*
*    This file contains Original Code and/or Modifications of Original
*    Code as defined in and that are subject to the Sybase Open Watcom
*    Public License version 1.0 (the 'License'). You may not use this file
*    except in compliance with the License. BY USING THIS FILE YOU AGREE TO
*    ALL TERMS AND CONDITIONS OF THE LICENSE. A copy of the License is
*    provided with the Original Code and Modifications, and is also
*    available at www.sybase.com/developer/opensource.
*
*    The Original Code and all software distributed under the License are
*    distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
*    EXPRESS OR IMPLIED, AND SYBASE AND ALL CONTRIBUTORS HEREBY DISCLAIM
*    ALL SUCH WARRANTIES, INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF
*    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR
*    NON-INFRINGEMENT. Please see the License for the specific language
*    governing rights and limitations under the License.
*
*  ========================================================================
*
* Description:  WHEN YOU FIGURE OUT WHAT THIS FILE DOES, PLEASE
*               DESCRIBE IT HERE!
*
****************************************************************************/


#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "cfloati.h"


static void efInit( char ue[] )
{
    int     i;

    for( i = 0; i < CF_MAX_PREC; i++ ) {
        ue[i] = 0;
    }
}

static int efGet( cfloat *f, char ue[], int i )
{
    if( i >= f->len ) {
        return( ue[i - f->len] );
    } else {
        return( f->mant[i] - '0' );
    }
}

static void efSet( cfloat *f, char ue[], int i, int val )
{
    if( i >= f->len ) {
        ue[i - f->len] = (char)val;
    } else {
        f->mant[i] = (char)val + '0';
    }
}

static cfloat *scalarMultiply( cfhandle h, cfloat *f, int s )
{
    cfloat      *result;
    div_t       d;
    int         i;

    result = CFAlloc( h, f->len + 1 );

    result->len  = f->len + 1;
    result->exp  = f->exp;
    result->sign = f->sign;

    d.quot = 0;

    for( i = f->len; i-- > 0; ) {
        d = div( s * CFAccess( f, i ) + d.quot, 10 );
        CFDeposit( result, i + 1, d.rem );
    }
    CFDeposit( result, 0, d.quot );
    return( result );
}

static void expandCF( cfhandle h, cfloat **f, int scale )
{
    cfloat      *result;
    int         result_len;
    int         len;

    len = (*f)->len;
    result_len = len + scale;
    result = CFAlloc( h, result_len );
    memcpy( result, *f, CFLOAT_SIZE + len - 1 );
    while( len < result_len ) {
        result->mant[len++] = '0';
    }
    result->mant[len] = NULLCHAR;
    result->len  = result_len;

    CFFree( h, *f );

    *f = result;
}

static void roundupCF( cfloat *f )
{
    int     i;

    for( i = f->len - 1; i >= 0; i-- ) {
        if( f->mant[i] == '9' ) {
            f->mant[i] = '0';
        } else {
            f->mant[i] += 1;
            return;
        }
    }
    f->mant[0] = '1';
    f->exp += 1;
}

/*
 * CFDiv:  Computes  f1 / f2
 */
cfloat  *CFDiv( cfhandle h, cfloat *f1, cfloat *f2 )
{
    cfloat         *result;
    cfloat         *u, *v;
    int             i, qa, ua, va, v1, cy, scale;
    int             j;
    div_t           d;
    char            ue[CF_MAX_PREC];

    if( f2->sign == 0 ) {                       // Attempt to divide by zero.
        result = CFAlloc( h, 1 );
        result->mant[0] = '1';
        result->sign    = 1;
        result->exp     = CF_ERR_EXP;           // Return error-type.
        return( result );
    }

    efInit( ue );                               // Initialize extended float.

    result  = CFAlloc( h, CF_MAX_PREC );        // Allocate mem. for result.

    result->sign = f1->sign * f2->sign;         // Set sign of result.
    result->exp  = f1->exp - f2->exp + 1;       // Set exponent of result.
    result->len  = 0;

    if( CFAccess( f2, 0 ) < 5 ) {
        scale = 10 / (CFAccess( f2, 0 ) + 1);
    } else {
        scale = 1;
    }

    u = scalarMultiply( h, f1, scale );         // Extra digit added.
    v = scalarMultiply( h, f2, scale );         // Extra digit added.

    if( v->len < 3 ) {                          // Divisor must have at least
        expandCF( h, &v, 1 );                   // two digits (ignore leading 0)
    }
    if( v->len >= u->len ) {                    // Dividend must have more
        expandCF( h, &u, v->len - u->len + 1 ); // digits than divisor.
    }
    /*
     * We now use the classical division algorithm described in Knuth,
     * _Seminumerical_Algorithms_, section 4.3.1.
     *
     * The following implementation uses base 10, and the initial approximation
     * is made by taking qa = floor( uj.uj1.uj2 / v1.v2 ) instead of simply
     * floor( uj.uj1 / v1 ).  According to Knuth, this initial approximation
     * is always greater than the real quotient digit, and off by at most two.
     */
    v1 = CFAccess( v, 1 );
    va = 10 * v1 + CFAccess( v, 2 );

    for( j = 0; j <= CF_MAX_PREC; j++ ) {
        /*
         * Make initial approximation of the quotient digit.
         */
        if( v1 == efGet( u, ue, j ) ) {
            qa = 9;
        } else {
            ua = efGet( u, ue, j ) * 100 + efGet( u, ue, j + 1 ) * 10 + efGet( u, ue, j + 2 );
            qa = ua / va;
        }
        /*
         * Replace (uj.uj1...ujn) with (uj.uj1..ujn) - qa * (v1.v2..vn)
         */
        cy = 0;
        for( i = v->len - 1; i >= 0; i-- ) {
            d = div( efGet( u, ue, i + j ) - qa * CFAccess( v, i ) + cy, 10 );
            if( d.rem < 0 ) {
                d.rem  += 10;
                d.quot -= 1;
            }
            cy = d.quot;
            efSet( u, ue, i + j, d.rem );
        }
        /*
         * The above subtraction resulted in a negative number.  So qa was
         * in fact off by one.  Correct that, and add back to correct
         * (uj.uj1...ujn).
         */
        if( cy ) {
            qa--;
            cy = 0;
            for( i = v->len - 1; i >= 0; i-- ) {
                d = div( efGet( u, ue, i + j ) + CFAccess( v, i ) + cy, 10 );
                efSet( u, ue, i + j, d.rem );
                cy = d.quot;
            }
        }
        /*
         * Set the real quotient digit, and do some rounding when maximum
         * precision is reached.  [Rounding is unbiased.]
         */
        if( j < CF_MAX_PREC ) {
            CFDeposit( result, j, qa );
            result->len++;
        } else {
            if( qa > 5 ) {
                roundupCF( result );
            } else if( qa == 5 ) {
                cy = 0;
                for( i = v->len - 1; i >= 0; i-- ) {
                    if( efGet( u, ue, i + j ) != 0 ) {
                        cy = 1;
                        break;
                    }
                }
                if( cy ) {
                    roundupCF( result );
                } else if( CFAccess( result, CF_MAX_PREC - 1 ) % 2 != 0 ) {
                    roundupCF( result );
                }
            }
        }
    }
    /*
     * Clean up the mess we made.
     */
    CFFree( h, u );
    CFFree( h, v );
    /*
     * normalize result
     */
    CFClean( result );
    return( result );
}
