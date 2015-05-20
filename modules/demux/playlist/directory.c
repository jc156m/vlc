/*****************************************************************************
 * directory.c : Use access readdir to output folder content to playlist
 *****************************************************************************
 * Copyright (C) 2014 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Julien 'Lta' BALLET <contact # lta . io >
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_demux.h>

#include "playlist.h"

struct demux_sys_t
{
    bool b_dir_sorted;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int Demux( demux_t *p_demux );


int Import_Dir ( vlc_object_t *p_this)
{
    demux_t  *p_demux = (demux_t *)p_this;

    bool b_is_dir = false;
    bool b_dir_sorted = false;
    int i_err = stream_Control( p_demux->s, STREAM_IS_DIRECTORY, &b_is_dir,
                                &b_dir_sorted, NULL );

    if ( !( i_err == VLC_SUCCESS && b_is_dir ) )
        return VLC_EGENERIC;

    STANDARD_DEMUX_INIT_MSG( "reading directory content" );
    p_demux->p_sys->b_dir_sorted = b_dir_sorted;

    return VLC_SUCCESS;
}

void Close_Dir ( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t *)p_this;
    free( p_demux->p_sys );
}

static int compar_type( input_item_t *p1, input_item_t *p2 )
{
    if( p1->i_type != p2->i_type )
    {
        if( p1->i_type == ITEM_TYPE_DIRECTORY )
            return -1;
        if( p2->i_type == ITEM_TYPE_DIRECTORY )
            return 1;
    }
    return 0;
}

static int compar_collate( input_item_t *p1, input_item_t *p2 )
{
    int i_ret = compar_type( p1, p2 );

    if( i_ret != 0 )
        return i_ret;

#ifdef HAVE_STRCOLL
    return strcoll( p1->psz_name, p2->psz_name );
#else
    return strcmp( p1->psz_name, p2->psz_name );
#endif
}

static int compar_version( input_item_t *p1, input_item_t *p2 )
{
    int i_ret = compar_type( p1, p2 );

    if( i_ret != 0 )
        return i_ret;

    return strverscmp( p1->psz_name, p2->psz_name );
}

static int Demux( demux_t *p_demux )
{
    int i_ret = VLC_SUCCESS;
    input_item_t *p_input;
    input_item_node_t *p_node;
    input_item_t *p_item;

    p_input = GetCurrentItem( p_demux );
    p_node = input_item_node_Create( p_input );
    input_item_Release(p_input);

    while( !i_ret && ( p_item = stream_ReadDir( p_demux->s ) ) )
    {
        int i_name_len = p_item->psz_name ? strlen( p_item->psz_name ) : 0;

        /* skip "." and ".." items */
        if( ( i_name_len == 1 && p_item->psz_name[0] == '.' ) ||
            ( i_name_len == 2 && p_item->psz_name[0] == '.' &&
              p_item->psz_name[1] == '.' ) )
            goto skip_item;

        input_item_CopyOptions( p_node->p_item, p_item );
        if( !input_item_node_AppendItem( p_node, p_item ) )
            i_ret = VLC_ENOMEM;
skip_item:
        input_item_Release( p_item );
    }

    if( i_ret )
    {
        msg_Warn( p_demux, "unable to read directory" );
        input_item_node_Delete( p_node );
        return i_ret;
    }

    if( !p_demux->p_sys->b_dir_sorted )
    {
        input_item_compar_cb compar_cb = NULL;
        char *psz_sort = var_InheritString( p_demux, "directory-sort" );

        if( psz_sort )
        {
            if( !strcasecmp( psz_sort, "version" ) )
                compar_cb = compar_version;
            else if( strcasecmp( psz_sort, "none" ) )
                compar_cb = compar_collate;
        }
        free( psz_sort );
        if( compar_cb )
            input_item_node_Sort( p_node, compar_cb );
    }

    input_item_node_PostAndDelete( p_node );
    return VLC_SUCCESS;
}
