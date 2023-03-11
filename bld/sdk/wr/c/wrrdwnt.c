/****************************************************************************
*
*                            Open Watcom Project
*
* Copyright (c) 2002-2023 The Open Watcom Contributors. All Rights Reserved.
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


#include "wrglbl.h"
#include <io.h>
#include "wrrdwnt.h"
#include "wrmsg.h"
#include "wrmemi.h"
#include "rcrtns.h"


/* forward declarations */
bool WRReadResourceEntry( FILE *fp, uint_32 offset, resource_entry *res_entry );

/****************************************************************************/
/* external function prototypes                                             */
/****************************************************************************/

/****************************************************************************/
/* macro definitions                                                        */
/****************************************************************************/
#define PE_OFFSET                 0x3c

/****************************************************************************/
/* type definitions                                                         */
/****************************************************************************/

/****************************************************************************/
/* static function prototypes                                               */
/****************************************************************************/
static bool     WRWinNTHeaderHasResourceTable( pe_exe_header * );
static int      WRCalcObjTableOffset( FILE *, pe_exe_header * );
static bool     WRReadNTObjectTable( FILE *, pe_exe_header *, pe_object ** );
static bool     WRLoadWResDirFromWinNTEXE( FILE *, WResDir * );
static bool     WRHandleWinNTTypeDir( FILE *, WResDir *, uint_32 );
static bool     WRHandleWinNTTypeEntry( FILE *, WResDir *, resource_dir_entry *, bool );
static bool     WRHandleWinNTNameDir( FILE *, WResDir *, WResID *, uint_32 );
static bool     WRHandleWinNTNameEntry( FILE *, WResDir *, WResID *, resource_dir_entry *, bool );
static bool     WRHandleWinNTLangIDDir( FILE *, WResDir *, WResID *, WResID *, uint_32 );
static bool     WRHandleWinNTLangIDEntry( FILE *, WResDir *, WResID *, WResID *, resource_dir_entry * );
static bool     WRReadResourceHeader( FILE *, uint_32, resource_dir_header *, resource_dir_entry ** );
static WResID   *WRGetUniCodeWResID( FILE *, uint_32 );

/****************************************************************************/
/* static variables                                                         */
/****************************************************************************/
static uint_32 res_offset = 0;
static uint_32 res_rva    = 0;

#define WR_MAP_DATA_RVA( rva )  (uint_32)((rva - res_rva) + res_offset)
#define WR_MAP_RES_RVA( rva )   (uint_32)(rva + res_offset)

bool WRLoadResourceFromWinNTEXE( WRInfo *info )
{
    FILE        *fp;
    bool        ok;

    ok = ( (fp = ResOpenFileRO( info->file_name )) != NULL );

    if( ok ) {
        ok = WRLoadWResDirFromWinNTEXE( fp, &info->dir );
    }

    if( fp != NULL ) {
        ResCloseFile( fp );
    }

    return( ok );
}

long WRReadWinNTExeHeader( FILE *fp, pe_exe_header *pehdr )
{
    bool        old_pos;
    uint_16     offset;
    bool        ok;

    old_pos = false;

    ok = ( fp != NULL && pehdr != NULL );

    if( ok ) {
        ok = old_pos = !RESSEEK( fp, 0x18, SEEK_SET );
    }

    /* check the reloc offset */
    if( ok ) {
        ResReadUint16( &offset, fp );
        ok = ( offset >= 0x0040 );
    }

    if( ok ) {
        ok = !RESSEEK( fp, PE_OFFSET, SEEK_SET );
    }

    /* check header offset */
    if( ok ) {
        ResReadUint16( &offset, fp );
        ok = ( offset != 0 );
    }

    if( ok ) {
        ok = !RESSEEK( fp, offset, SEEK_SET );
    }

    if( ok ) {
        ok = ( RESREAD( fp, pehdr, PE_HDR_SIZE ) == PE_HDR_SIZE );
        if( ok ) {
            ok = ( RESREAD( fp, (char *)pehdr + PE_HDR_SIZE, PE_OPT_SIZE( *pehdr ) ) == PE_OPT_SIZE( *pehdr ) );
        }
    }

    /* check for valid Win32 EXE */
    if( ok ) {
        ok = WRIsHeaderValidWINNT( pehdr );
    }

    if( old_pos ) {
        ok = ( !RESSEEK( fp, 0x18, SEEK_SET ) && ok );
    }

    if( !ok ) {
        WRDisplayErrorMsg( WR_INVALIDNTEXE );
        offset = 0;
    }

    return( offset );
}

int WRCalcObjTableOffset( FILE *fp, pe_exe_header *pehdr )
{
    uint_16  pe_offset;
    int      offset;
    bool     ok;

    ok = !ResSeek( fp, PE_OFFSET, SEEK_SET );

    if( ok ) {
        ResReadUint16( &pe_offset, fp );
        ok = ( pe_offset != 0 );
    }

    if ( ok ) {
        offset = pe_offset + PE_SIZE( *pehdr );
    } else {
        offset = 0;
    }

    return( offset );
}

bool WRReadNTObjectTable( FILE *fp, pe_exe_header *pehdr, pe_object **ot )
{
    size_t  size;
    int     ot_offset;

    ot_offset = WRCalcObjTableOffset( fp, pehdr );
    if( ot_offset == 0 || ResSeek( fp, ot_offset, SEEK_SET ) ) {
        return( false );
    }
    size = sizeof( pe_object ) * pehdr->fheader.num_objects;
    *ot = (pe_object *)MemAlloc( size );
    if( *ot != NULL ) {
        if( RESREAD( fp, *ot, size ) != size ) {
            MemFree( *ot );
            *ot = NULL;
        }
    }

    return( *ot != NULL );
}

bool WRIsHeaderValidWINNT( pe_exe_header *header )
{
    /* at some point will we have to check the CPUTYPE ????!!!! */
    return( header->signature == EXESIGN_PE );
}

bool WRWinNTHeaderHasResourceTable( pe_exe_header *header )
{
    return( PE( *header, num_tables ) > PE_TBL_RESOURCE 
            && PE_DIRECTORY( *header, PE_TBL_RESOURCE ).rva != 0 
            && PE_DIRECTORY( *header, PE_TBL_RESOURCE ).size != 0 );
}

bool WRLoadWResDirFromWinNTEXE( FILE *fp, WResDir *dir )
{
    pe_exe_header       pehdr;
    pe_object           *otable;
    uint_32             physical_size;
    uint_32             physical_offset;
    bool                ok;
    unsigned_32         resource_rva;

    ok = ( fp != NULL );

    if( ok ) {
        ok = ( (*dir = WResInitDir()) != NULL );
    }

    if( ok ) {
        ok = ( WRReadWinNTExeHeader( fp, &pehdr ) != 0 );
    }

    /* check if a resource table is present */
    if( ok ) {
        ok = WRWinNTHeaderHasResourceTable( &pehdr );
        if( !ok ) {
            WRDisplayErrorMsg( WR_EXENORES );
            return( true );
        }
    }

    /* read NT object table */
    otable = NULL;
    if( ok ) {
        ok = WRReadNTObjectTable( fp, &pehdr, &otable );
    }

    /* find resource object in object table */
    resource_rva = 0;
    if( ok ) {
        unsigned    i;
        unsigned_32 file_align;

        physical_size = 0;
        physical_offset = 0;
        resource_rva = PE_DIRECTORY( pehdr, PE_TBL_RESOURCE ).rva;
        file_align = PE( pehdr, file_align );
        for( i = 0; i < pehdr.fheader.num_objects; i++ ) {
            if( otable[i].rva == resource_rva ) {
                physical_size = otable[i].physical_size;
                physical_offset = otable[i].physical_offset;
                break;
            }
        }
        ok = ( physical_size != 0 && physical_offset != 0 && (physical_size % file_align) == 0 && (physical_offset % file_align) == 0 );
    }

    if( otable != NULL ) {
        MemFree( otable );
    }

    /* read the resource information */
    if( ok ) {
        res_offset = physical_offset;
        res_rva = resource_rva;
        ok = WRHandleWinNTTypeDir( fp, dir, physical_offset );
    }

    return( ok );
}

bool WRHandleWinNTTypeDir( FILE *fp, WResDir *dir, uint_32 offset )
{
    resource_dir_header rd_hdr;
    resource_dir_entry  *rd_entry;
    int                 i;
    bool                ok;

    ok = WRReadResourceHeader( fp, offset, &rd_hdr, &rd_entry );

    if( ok ) {
        for( i = 0; i < rd_hdr.num_name_entries; i++ ) {
            WRHandleWinNTTypeEntry( fp, dir, &rd_entry[i], true );
        }
        for( i = rd_hdr.num_name_entries; i < rd_hdr.num_name_entries + rd_hdr.num_id_entries; i++ ) {
            WRHandleWinNTTypeEntry( fp, dir, &rd_entry[i], false );
        }
        MemFree( rd_entry );
    }

    return( ok );
}

bool WRHandleWinNTTypeEntry( FILE *fp, WResDir *dir, resource_dir_entry *rd_entry, bool is_name )
{
    WResID  *type;
    bool    ok;

    /* verify the id_name */
    if( ( (rd_entry->id_name & PE_RESOURCE_MASK_ON) != 0 ) ^ is_name ) {
        WRDisplayErrorMsg( WR_BADIDDISCARDTYPE );
        return( false );
    }

    if( is_name ) {
        type = WRGetUniCodeWResID( fp, PE_RESOURCE_MASK & rd_entry->id_name );
    } else {
        type = WResIDFromNum( rd_entry->id_name );
    }

    ok = ( type != NULL );

    if( ok ) {
        /* verify that entry_rva is a subdir */
        ok = ( (rd_entry->entry_rva & PE_RESOURCE_MASK_ON) != 0 );
        if( !ok ) {
            WRDisplayErrorMsg( WR_BADRVADISCARDTYPE );
        }
    }

    if( ok ) {
        ok = WRHandleWinNTNameDir( fp, dir, type, rd_entry->entry_rva & PE_RESOURCE_MASK );
    }

    if( type != NULL ) {
        WResIDFree( type );
    }

    return( ok );
}

bool WRHandleWinNTNameDir( FILE *fp, WResDir *dir, WResID *type, uint_32 rva )
{
    resource_dir_header rd_hdr;
    resource_dir_entry  *rd_entry;
    uint_32             offset;
    int                 i;
    bool                ok;

    offset = WR_MAP_RES_RVA( rva );

    ok = WRReadResourceHeader( fp, offset, &rd_hdr, &rd_entry );

    if( ok ) {
        for( i = 0; i < rd_hdr.num_name_entries; i++ ) {
            WRHandleWinNTNameEntry( fp, dir, type, &rd_entry[i], true );
        }
        for( i = rd_hdr.num_name_entries; i < rd_hdr.num_name_entries + rd_hdr.num_id_entries; i++ ) {
            WRHandleWinNTNameEntry( fp, dir, type, &rd_entry[i], false );
        }
        MemFree( rd_entry );
    }

    return( ok );
}

bool WRHandleWinNTNameEntry( FILE *fp, WResDir *dir, WResID *type,
                            resource_dir_entry *rd_entry, bool is_name )
{
    WResLangType    lang;
    resource_entry  res_entry;
    uint_32         offset;
    WResID          *name;
    bool            add_now;
    bool            ok;

    lang.lang = DEF_LANG;
    lang.sublang = DEF_SUBLANG;

    /* verify the id_name */
    if( ( (rd_entry->id_name & PE_RESOURCE_MASK_ON) != 0 ) ^ is_name ) {
        WRDisplayErrorMsg( WR_BADIDDISCARDNAME );
        return( false );
    }

    if( is_name ) {
        name = WRGetUniCodeWResID( fp, PE_RESOURCE_MASK & rd_entry->id_name );
    } else {
        name = WResIDFromNum( rd_entry->id_name );
    }

    ok = ( name != NULL );

    if( ok ) {
        /* is the entry_rva is a subdir */
        if( rd_entry->entry_rva & PE_RESOURCE_MASK_ON ) {
            add_now = false;
            ok = WRHandleWinNTLangIDDir( fp, dir, type, name, rd_entry->entry_rva & PE_RESOURCE_MASK );
        } else {
            /* will this to happen often ???? */
            add_now = true;
            offset = WR_MAP_RES_RVA( rd_entry->entry_rva );
            ok = WRReadResourceEntry( fp, offset, &res_entry );
        }
    }

    if( ok ) {
        if( add_now ) {
            offset = WR_MAP_RES_RVA( res_entry.data_rva );
            ok = !WResAddResource( type, name, 0, offset, res_entry.size,
                                   *dir, &lang, NULL );
        }
    }

    if( name != NULL ) {
        WResIDFree( name );
    }

    return( ok );
}

bool WRHandleWinNTLangIDDir( FILE *fp, WResDir *dir,
                            WResID *type, WResID *name, uint_32 rva )
{
    resource_dir_header rd_hdr;
    resource_dir_entry  *rd_entry;
    uint_32             offset;
    bool                ok;
    int                 i;

    offset = WR_MAP_RES_RVA( rva );

    ok = WRReadResourceHeader( fp, offset, &rd_hdr, &rd_entry );

    if( ok ) {
        for( i = 0; i < rd_hdr.num_name_entries; i++ ) {
            WRHandleWinNTLangIDEntry( fp, dir, type, name, &rd_entry[i] );
        }
        for( i = rd_hdr.num_name_entries; i < rd_hdr.num_name_entries + rd_hdr.num_id_entries; i++ ) {
            WRHandleWinNTLangIDEntry( fp, dir, type, name, &rd_entry[i] );
        }
        /* until more info is available only look for the first id entry */
        MemFree( rd_entry );
    }

    return( ok );
}

bool WRHandleWinNTLangIDEntry( FILE *fp, WResDir *dir, WResID *type,
                              WResID *name, resource_dir_entry *rd_entry )
{
    WResLangType    lang;
    resource_entry  res_entry;
    uint_32         offset;
    bool            ok;

    ok = ( name != NULL );

    if( ok ) {
        /* is the entry_rva is a subdir */
        if( rd_entry->entry_rva & PE_RESOURCE_MASK_ON ) {
            WRDisplayErrorMsg( WR_BADLANGDISCARDNAME );
            ok = false;
        } else {
            offset = WR_MAP_RES_RVA( rd_entry->entry_rva );
            ok = WRReadResourceEntry( fp, offset, &res_entry );
        }
    }

    if( ok ) {
        offset = WR_MAP_DATA_RVA( res_entry.data_rva );
        lang.lang = HIWORD( rd_entry->id_name );
        lang.sublang = LOBYTE( LOWORD( rd_entry->id_name ) );
        ok = !WResAddResource( type, name, 0, offset, res_entry.size, *dir, &lang, NULL );
    }

    return( ok );
}

bool WRReadResourceHeader( FILE *fp, uint_32 offset,
                          resource_dir_header *rd_hdr,
                          resource_dir_entry **rd_entry )
{
    bool    ok;
    size_t  rde_size;

    *rd_entry = NULL;

    /* if offset is zero don't perform the seek to beginning of resource directory */
    ok = ( offset == 0 || !RESSEEK( fp, offset, SEEK_SET ) );

    /* read the resource directory header */
    if( ok ) {
        ok = ( RESREAD( fp, rd_hdr, sizeof( resource_dir_header ) ) == sizeof( resource_dir_header ) );
    }

    if( ok ) {
        rde_size = sizeof( resource_dir_entry ) * ( rd_hdr->num_name_entries + rd_hdr->num_id_entries );
        *rd_entry = (resource_dir_entry *)MemAlloc( rde_size );
        ok = ( *rd_entry != NULL );
    }

    if( ok ) {
        ok = ( RESREAD( fp, *rd_entry, rde_size ) == rde_size );
    }

    if( !ok && *rd_entry != NULL ) {
        MemFree( *rd_entry );
    }

    return( ok );
}

WResID *WRGetUniCodeWResID( FILE *fp, uint_32 rva )
{
    bool    old_pos;
    uint_32 offset;
    uint_16 len;
    bool    ok;
    char    *unistr;
    WResID  *id;
    int     i;

    offset = WR_MAP_RES_RVA( rva );

    unistr = NULL;

    /* seek to the location of the Unicode string */
    ok = old_pos = !RESSEEK( fp, offset, SEEK_SET );

    /* read the Unicode string */
    if( ok ) {
        ResReadUint16( &len, fp );
        len *= 2;
        unistr = (char *)MemAlloc( len + 2 );
        ok = ( unistr != NULL );
    }

    if( ok ) {
        unistr[len] = 0;
        unistr[len + 1] = 0;
        ok = ( RESREAD( fp, unistr, len ) == len );
    }

    if( ok ) {
        for( i = 0; i < len + 2; i += 2 ) {
            unistr[i / 2] = unistr[i];
        }
        ok = ( (id = WResIDFromStr( unistr )) != NULL );
    }

#if 0
    if( old_pos ) {
        ok = ( !RESSEEK( fp, offset, SEEK_SET ) && ok );
    }
#endif

    if( unistr != NULL ) {
        MemFree( unistr );
    }

    if( ok ) {
        return( id );
    } else {
        return( NULL );
    }
}

bool WRReadResourceEntry( FILE *fp, uint_32 offset, resource_entry *res_entry )
{
    bool ok;

    /* if offset is zero don't perform the seek to beginning of resource directory */
    ok = ( offset == 0 || !ResSeek( fp, offset, SEEK_SET ) );

    /* read the resource entry */
    if( ok ) {
        ok = ( RESREAD( fp, res_entry, sizeof( resource_entry ) ) == sizeof( resource_entry ) );
    }

    return( ok );
}
