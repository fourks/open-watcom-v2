/****************************************************************************
*
*                            Open Watcom Project
*
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
* Description:  Alpha AXP procedure prolog/epilog generation.
*
****************************************************************************/


#include "cgstd.h"
#include "cgdefs.h"
#include "coderep.h"
#include "cgmem.h"
#include "zoiks.h"
#include "axpencod.h"
#include "data.h"
#include "rtrtn.h"
#include "objout.h"
#include "dbsyms.h"
#include "rscconst.h"
#include "object.h"
#include "feprotos.h"

extern  uint_32         CountBits( uint_32 );
extern  void            CodeLabelLinenum( label_handle, unsigned, cg_linenum );
extern  hw_reg_set      *GPRegs( void );
extern  hw_reg_set      *FPRegs( void );
extern  hw_reg_set      *ParmRegs( void );
extern  hw_reg_set      SaveRegs( void );
extern  void            GenMEMINS( uint_8, uint_8, uint_8, signed_16 );
extern  void            GenOPINS( uint_8, uint_8, uint_8, uint_8, uint_8 );
extern  hw_reg_set      StackReg( void );
extern  hw_reg_set      ScratchReg( void );
extern  hw_reg_set      VarargsHomePtr( void );
extern  void            GenRET( void );
extern  void            OutFuncStart( label_handle label, offset start, int line );
extern  void            OutFileStart( int line );
extern  void            OutFuncEnd( offset end );
extern void             OutPDataRec( label_handle label, uint_32 pro_size, uint_32 proc_end );
extern  hw_reg_set      FrameBaseReg(void);
extern  byte            RegTrans(hw_reg_set);
extern  void            GenCallLabelReg( label_handle lbl, uint reg );
extern  type_length     TempLocation( name * );
extern  hw_reg_set      ReturnAddrReg( void );
extern  void            EmitFuncStart( label_handle cg_linenum );
extern  void            TellKeepLabel( label_handle );
extern  void            TellProcLabel( label_handle );
extern  void            GenLOADS32( signed_32, uint_8 );

static  void    calcUsedRegs( void )
/**********************************/
{
    block       *blk;
    instruction *ins;
    name        *result;
    hw_reg_set  used;

    HW_CAsgn( used, HW_EMPTY );
    for( blk = HeadBlock; blk != NULL; blk = blk->next_block ) {
        if( ( blk->class & CALL_LABEL ) != EMPTY ) {
            HW_TurnOn( used, ReturnAddrReg() );
        }
        for( ins = blk->ins.hd.next; ins->head.opcode != OP_BLOCK; ins = ins->head.next ) {
            result = ins->result;
            if( result != NULL && result->n.class == N_REGISTER ) {
                HW_TurnOn( used, result->r.reg );
            }
            /* placeholder for big label doesn't really zap anything */
            if( ins->head.opcode != OP_NOP ) {
                HW_TurnOn( used, ins->zap->reg );
            }
        }
    }
    if( FEAttr( AskForLblSym( CurrProc->label ) ) & FE_VARARGS ) {
        HW_TurnOn( used, VarargsHomePtr() );
    }
    HW_TurnOn( CurrProc->state.used, used );
}


extern  void    AddCacheRegs( void )
/**********************************/
{
}

#define _unused( x )    ( (x) = (x) )

static  void    initParmCache( stack_record *pc, type_length *offset )
/********************************************************************/
{
    pc->start = *offset;
    pc->size = MaxStack;
    if( MaxStack > 0 ) {
        *offset += MaxStack;
    }
}


static  void    emitParmCacheProlog( stack_record *pc )
/*****************************************************/
{
    _unused( pc );
}


static  void    emitParmCacheEpilog( stack_record *pc )
/*****************************************************/
{
    _unused( pc );
}


static  void    initLocals( stack_record *locals, type_length *offset )
/*********************************************************************/
{
    locals->start = *offset;
    locals->size = CurrProc->locals.size;
    *offset += locals->size;
}


static  void    emitLocalProlog( stack_record *locals )
/*****************************************************/
{
    _unused( locals );
}


static  void    emitLocalEpilog( stack_record *locals )
/*****************************************************/
{
    _unused( locals );
}


static  uint_32 registerMask( hw_reg_set rs, hw_reg_set *rl )
/***********************************************************/
{
    hw_reg_set          *curr;
    uint_32             result;

    result = 0;
    for( curr = rl; !HW_CEqual( *curr, HW_EMPTY ); curr++ ) {
        if( HW_Ovlap( rs, *curr ) ) {
            result |= 1 << RegTrans( *curr );
        }
    }
    return( result );
}


static  void    initSavedRegs( stack_record *saved_regs, type_length *offset )
/****************************************************************************/
{
    unsigned            num_regs;
    hw_reg_set          saved;

    calcUsedRegs();
    saved = SaveRegs();
    if( FEAttr( AskForLblSym( CurrProc->label ) ) & FE_VARARGS ) {
        HW_TurnOn( saved, VarargsHomePtr() );
    }
    CurrProc->targ.gpr_mask = registerMask( saved, GPRegs() );
    CurrProc->targ.fpr_mask = registerMask( saved, FPRegs() );
    num_regs  = CountBits( CurrProc->targ.gpr_mask );
    num_regs += CountBits( CurrProc->targ.fpr_mask );
    saved_regs->size = num_regs * REG_SIZE;
    saved_regs->start = *offset;
    *offset += saved_regs->size;
}

#define STORE_QUADWORD  0x2d
#define STORE_DOUBLE    0x27
#define LOAD_QUADWORD   0x29
#define LOAD_DOUBLE     0x23
#define LEA_OPCODE      0x08
#define LEAH_OPCODE     0x09

#define VARARGS_PTR     14
#define RT_PARM2        2
#define RT_PARM1        1
#define RT_RET_REG      0


static  void    genMove( uint_32 src, uint_32 dst )
/*************************************************/
{
    GenOPINS( 0x11, 0x20, AXP_ZERO_SINK, src, dst );
}


static  void    genLea( uint_32 src, signed_16 disp, uint_32 dst )
/****************************************************************/
{
    GenMEMINS( LEA_OPCODE, dst, src, disp );
}


static  uint_32 addressableRegion( stack_record *region, type_length *offset )
/****************************************************************************/
{
    if( region->start > AXP_MAX_OFFSET ) {
        *offset = 0;
        GenLOADS32( region->start, AXP_GPR_SCRATCH );
        // add sp, r28 -> r28
        GenOPINS( 0x10, 0x00, AXP_STACK_REG, AXP_GPR_SCRATCH, AXP_GPR_SCRATCH );
        return( AXP_GPR_SCRATCH );
    } else {
        *offset = region->start;
        return( AXP_STACK_REG );
    }
}


static  void    saveReg( uint_32 reg, uint_32 index, type_length offset, bool fp )
/********************************************************************************/
{
    uint_8              opcode;

    opcode = STORE_QUADWORD;
    if( fp ) {
        opcode = STORE_DOUBLE;
    }
    GenMEMINS( opcode, index, reg, offset );
}


static  void    loadReg( uint_32 reg, uint_32 index, type_length offset, bool fp )
/********************************************************************************/
{
    uint_8              opcode;

    opcode = LOAD_QUADWORD;
    if( fp ) {
        opcode = LOAD_DOUBLE;
    }
    GenMEMINS( opcode, index, reg, offset );
}


static  void    saveRegSet( uint_32 index_reg,
                        uint_32 reg_set, type_length offset, bool fp )
/********************************************************************/
{
    uint_32     index;
    uint_32     high_bit;

    index = sizeof( reg_set ) * 8 - 1;
    high_bit = 1 << index;
    for( ; reg_set != 0; reg_set <<= 1 ) {
        if( reg_set & high_bit ) {
            offset -= 8;
            saveReg( index_reg, index, offset, fp );
        }
        index -= 1;
    }
}


static  void    loadRegSet( uint_32 index_reg,
                        uint_32 reg_set, type_length offset, bool fp )
/********************************************************************/
{
    uint_32     index;

    index = 0;
    for( ; reg_set != 0; reg_set >>= 1 ) {
        if( reg_set & 1 ) {
            loadReg( index_reg, index, offset, fp );
            offset += 8;
        }
        index++;
    }
}


static  void    emitSavedRegsProlog( stack_record *saved_regs )
/*************************************************************/
{
    type_length         offset;
    uint_32             index_reg;

    index_reg = addressableRegion( saved_regs, &offset );
    offset += saved_regs->size;
    saveRegSet( index_reg, CurrProc->targ.gpr_mask, offset, FALSE );
    offset -= CountBits( CurrProc->targ.gpr_mask ) * REG_SIZE;
    saveRegSet( index_reg, CurrProc->targ.fpr_mask, offset, TRUE );
}


static  void    emitSavedRegsEpilog( stack_record *saved_regs )
/*************************************************************/
{
    type_length         offset;
    uint_32             index_reg;

    index_reg = addressableRegion( saved_regs, &offset );
    loadRegSet( index_reg, CurrProc->targ.fpr_mask, offset, TRUE );
    offset += CountBits( CurrProc->targ.fpr_mask ) * REG_SIZE;
    loadRegSet( index_reg, CurrProc->targ.gpr_mask, offset, FALSE );
}


static  void    initVarargs( stack_record *varargs, type_length *offset )
/***********************************************************************/
{
    cg_sym_handle       sym;
    fe_attr             attr;

    varargs->start = *offset;
    varargs->size = 0;
    sym = AskForLblSym( CurrProc->label );
    attr = FEAttr( sym );
    if( attr & FE_VARARGS ) {
        varargs->size = 12 * REG_SIZE;
        *offset += varargs->size;
    }
}


static  void    emitVarargsProlog( stack_record *varargs )
/********************************************************/
{
    type_length         offset;
    uint_32             index_reg;

    if( varargs->size != 0 ) {
        index_reg = addressableRegion( varargs, &offset );
        offset += varargs->size;
        saveRegSet( index_reg, 0x3f << 16, offset, FALSE );
        offset -= 6 * REG_SIZE;
        saveRegSet( index_reg, 0x3f << 16, offset, TRUE );
    }
}


static  void    emitVarargsEpilog( stack_record *varargs )
/********************************************************/
{
    // NB see FrameSaveEpilog below
    _unused( varargs );
}


static  void    initFrameSave( stack_record *fs, type_length *offset )
/********************************************************************/
{
    fs->start = *offset;
    fs->size = 0;
    if( CurrProc->targ.base_is_fp ) {
        fs->size = REG_SIZE;
        *offset += fs->size;
    }
}


static  void    emitFrameSaveProlog( stack_record *fs )
/*****************************************************/
{
    uint_32     index_reg;
    type_length offset;

    if( fs->size != 0 ) {
        index_reg = addressableRegion( fs, &offset );
        saveReg( index_reg, AXP_FRAME_REG, offset, FALSE );
    }
}


static  void    emitFrameSaveEpilog( stack_record *fs )
/*****************************************************/
{
    uint_32     index_reg;
    type_length offset;

    // NB This instruction must immediately precede the
    // stack restoration instruction - which means that the
    // varargs epilog above must be empty
    if( fs->size != 0 ) {
        index_reg = addressableRegion( fs, &offset );
        loadReg( index_reg, AXP_FRAME_REG, offset, FALSE );
    }
}


static  void    initSlop( stack_record *slop, type_length *offset )
/*****************************************************************/
{
    type_length         off;

    off = *offset;
    slop->start = off;
    slop->size = 0;
    if( off & (STACK_ALIGNMENT - 1) ) {
        slop->size = STACK_ALIGNMENT - (off & (STACK_ALIGNMENT - 1));
        *offset += slop->size;
    }
}


static  void    emitSlopProlog( stack_record *fs )
/************************************************/
{
    _unused( fs );
}


static  void    emitSlopEpilog( stack_record *fs )
/************************************************/
{
    _unused( fs );
}


static  signed_32 frameSize( stack_map *map )
/*******************************************/
{
    signed_32           size;

    size = map->slop.size + map->varargs.size + map->frame_save.size + map->saved_regs.size +
                map->locals.size + map->parm_cache.size;
    assert( (size & (STACK_ALIGNMENT - 1)) == 0 );
    return( size );
}


static  void    initStackLayout( stack_map *map )
/***********************************************/
{
    type_length         offset;

    offset = 0;
    initParmCache( &map->parm_cache, &offset );
    initLocals( &map->locals, &offset );
    initSavedRegs( &map->saved_regs, &offset );
    initFrameSave( &map->frame_save, &offset );
    initSlop( &map->slop, &offset );
    initVarargs( &map->varargs, &offset );
}


static  void    SetupVarargsReg( stack_map *map )
/***********************************************/
{
    if( map->varargs.size != 0 ) {
        type_length     offset;

        offset = map->varargs.start + 6 * REG_SIZE;
        // Skip hidden parameter in first register
        if( CurrProc->targ.return_points != NULL ) {
            offset += REG_SIZE;
        }
        if( offset > AXP_MAX_OFFSET ) {
            GenLOADS32( offset, VARARGS_PTR );
            GenOPINS( 0x10, 0x00, AXP_STACK_REG, VARARGS_PTR, VARARGS_PTR );
        } else {
            genLea( AXP_STACK_REG, offset, VARARGS_PTR );
        }
    }
}


static  void    emitProlog( stack_map *map )
/******************************************/
{
    type_length         frame_size;

    frame_size = frameSize( map );
    if( frame_size != 0 ) {
        if( frame_size <= AXP_MAX_OFFSET ) {
            genLea( AXP_STACK_REG, -frame_size, AXP_STACK_REG );
        } else {
            GenLOADS32( frame_size, AXP_GPR_SCRATCH );
            // sub sp,r28 -> sp
            GenOPINS( 0x10, 0x09, AXP_STACK_REG, AXP_GPR_SCRATCH, AXP_STACK_REG );
        }
        if( frame_size >= _TARGET_PAGE_SIZE ) {
            if( frame_size <= AXP_MAX_OFFSET ) {
                genLea( AXP_ZERO_SINK, frame_size, RT_PARM1 );
            } else {
                genMove( AXP_GPR_SCRATCH, RT_PARM1 );
            }
            GenCallLabelReg( RTLabel( RT_STK_CRAWL_SIZE ), RT_RET_REG );
        }
    }
    if( map->locals.size != 0 || map->parm_cache.size != 0 ) {
        if( _IsTargetModel( STACK_INIT ) ) {
            type_length         size;
            size = map->locals.size + map->parm_cache.size;
            if( size > AXP_MAX_OFFSET ) {
                GenLOADS32( size, RT_PARM1 );
            } else {
                genLea( AXP_ZERO_SINK, map->locals.size + map->parm_cache.size, RT_PARM1 );
            }
            GenCallLabelReg( RTLabel( RT_STK_STOMP ), RT_RET_REG );
        }
    }
    emitVarargsProlog( &map->varargs );
    emitSlopProlog( &map->slop );
    emitFrameSaveProlog( &map->frame_save );
    emitSavedRegsProlog( &map->saved_regs );
    emitLocalProlog( &map->locals );
    emitParmCacheProlog( &map->parm_cache );
    if( map->frame_save.size != 0 ) {
        genMove( AXP_STACK_REG, AXP_FRAME_REG );
    }
}


static  void    emitEpilog( stack_map *map )
/******************************************/
{
    type_length         frame_size;

    if( map->frame_save.size != 0 ) {
        // NB should just use AXP_FRAME_REG instead of AXP_STACK_REG in restore
        // code and not bother emitting this instruction
        genMove( AXP_FRAME_REG, AXP_STACK_REG );
    }
    emitParmCacheEpilog( &map->parm_cache );
    emitLocalEpilog( &map->locals );
    emitSavedRegsEpilog( &map->saved_regs );
    emitFrameSaveEpilog( &map->frame_save );
    emitSlopEpilog( &map->slop );
    emitVarargsEpilog( &map->varargs );
    frame_size = frameSize( map );
    if( frame_size != 0 ) {
        if( frame_size <= AXP_MAX_OFFSET ) {
            genLea( AXP_STACK_REG, frame_size, AXP_STACK_REG );
        } else {
            GenLOADS32( frame_size, AXP_GPR_SCRATCH );
            GenOPINS( 0x10, 0x00, AXP_STACK_REG, AXP_GPR_SCRATCH, AXP_STACK_REG );
        }
    }
}


extern  void    GenProlog( void )
/*******************************/
{
    segment_id          old;
    label_handle        label;

    old = SetOP( AskCodeSeg() );
    label = CurrProc->label;
    if( _IsModel( NUMBERS ) ) {
        OutFileStart( HeadBlock->ins.hd.line_num );
    }
    TellKeepLabel( label );
    TellProcLabel( label );
    CodeLabelLinenum( label, DepthAlign( PROC_ALIGN ), HeadBlock->ins.hd.line_num );
    if( _IsModel( DBG_LOCALS ) ) {  // d1+ or d2
        // DbgRtnBeg( CurrProc->targ.debug, lc );
        EmitRtnBeg( /*label, HeadBlock->ins.hd.line_num*/ );
    }
    // keep stack aligned
    CurrProc->locals.size = _RoundUp( CurrProc->locals.size, REG_SIZE );
    CurrProc->parms.base = 0;
    CurrProc->parms.size = CurrProc->state.parm.offset;
    initStackLayout( &CurrProc->targ.stack_map );
    emitProlog( &CurrProc->targ.stack_map );
    EmitProEnd();
    SetupVarargsReg( &CurrProc->targ.stack_map );
    CurrProc->targ.frame_size = frameSize( &CurrProc->targ.stack_map );
    SetOP( old );
}


void    GenEpilog( void )
/***********************/
{
    segment_id      old;

    old = SetOP( AskCodeSeg() );
    EmitEpiBeg();
    emitEpilog( &CurrProc->targ.stack_map );
    GenRET();
    CurrProc->prolog_state |= GENERATED_EPILOG;
    EmitRtnEnd();
    SetOP( old );
}


extern  int     AskDisplaySize( int level )
/*****************************************/
{
    level = level;
    return( 0 );
}


extern  void    InitStackDepth( block *blk )
/******************************************/
{
    blk = blk;
}


extern  type_length     PushSize( type_length len )
/*************************************************/
{
    if( len < REG_SIZE ) return( REG_SIZE );
    return( len );
}


extern  type_length     NewBase( name *op )
/*****************************************/
{
    return( TempLocation( op ) );
}


extern  int     ParmsAtPrologue( void )
/*************************************/
{
    return( 0 );
}
