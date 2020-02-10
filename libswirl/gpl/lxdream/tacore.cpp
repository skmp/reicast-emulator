/*
    Derived from lxdream, original copyright notice follows
*/
#include <license/gpl>

/**
 * $Id$
 *
 * PVR2 Tile Accelerator implementation
 *
 * Copyright (c) 2005 Nathan Keynes.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <string.h>
#include <stdint.h>
#include <limits.h>

#include "tacore.h"
#include "hw/pvr/pvr_regs.h"
#include "hw/pvr/pvr_mem.h"
#include "hw/pvr/pvr_sb_regs.h"
#include "hw/holly/holly_intc.h"

#define PVR2_RAM_SIZE VRAM_SIZE
#define PVR2_RAM_MASK VRAM_MASK

u8* pvr2_main_ram_hidden;
#define PVRRAM(addr) (*(uint32_t *)(pvr2_main_ram_hidden + (pvr_map32(addr))))

/*
#include "lxdream.h"
#include "pvr2/pvr2.h"
#include "pvr2/pvr2mmio.h"
#include "asic.h"
#include "dream.h"
*/

#define CLAMP(v, low, high) (v < low? low : (v > high? high: v))
#define MIN(a, b) (a < b ? a : b)
#define MAX(a, b) (a > b ? a : b)

#define TRUE true
#define FALSE false

#define SEGMENT_END         0x80000000
#define SEGMENT_ZCLEAR      0x40000000
#define SEGMENT_SORT_TRANS  0x20000000
#define SEGMENT_START       0x10000000
#define SEGMENT_X(c)        (((c) >> 2) & 0x3F)
#define SEGMENT_Y(c)        (((c) >> 8) & 0x3F)
#define NO_POINTER          0x80000000
#define IS_TILE_PTR(p)      ( ((p)&NO_POINTER) == 0 )
#define IS_LAST_SEGMENT(s)  (((s)->control) & SEGMENT_END)

struct tile_segment {
    uint32_t control;
    pvraddr_t opaque_ptr;
    pvraddr_t opaquemod_ptr;
    pvraddr_t trans_ptr;
    pvraddr_t transmod_ptr;
    pvraddr_t punchout_ptr;
};


struct tile_bounds {
    int32_t x1, y1, x2, y2;
};


#define STATE_IDLE                 0
#define STATE_IN_LIST              1
#define STATE_IN_POLYGON           2
#define STATE_EXPECT_POLY_BLOCK2   3
#define STATE_EXPECT_VERTEX_BLOCK2 4
#define STATE_ERROR                5
#define STATE_EXPECT_END_VERTEX_BLOCK2 7

#define TA_CMD(i) ( (i) >> 29 )
#define TA_CMD_END_LIST 0
#define TA_CMD_CLIP 1
#define TA_CMD_POLYGON_CONTEXT 4
#define TA_CMD_SPRITE_CONTEXT 5
#define TA_CMD_VERTEX 7

#define TA_LIST_NONE -1
#define TA_LIST_OPAQUE 0
#define TA_LIST_OPAQUE_MOD 1
#define TA_LIST_TRANS 2
#define TA_LIST_TRANS_MOD 3
#define TA_LIST_PUNCH_OUT 4
#define TA_IS_MODIFIER_LIST(list) (list == TA_LIST_OPAQUE_MOD || list == TA_LIST_TRANS_MOD)

#define TA_GROW_UP 0
#define TA_GROW_DOWN 1

#define TA_VERTEX_NONE                        -1
#define TA_VERTEX_PACKED                      0x00
#define TA_VERTEX_TEX_PACKED                  0x08
#define TA_VERTEX_TEX_SPEC_PACKED             0x0C
#define TA_VERTEX_TEX_UV16_PACKED             0x09
#define TA_VERTEX_TEX_UV16_SPEC_PACKED        0x0D
#define TA_VERTEX_FLOAT                       0x10
#define TA_VERTEX_TEX_FLOAT                   0x18
#define TA_VERTEX_TEX_SPEC_FLOAT              0x1C
#define TA_VERTEX_TEX_UV16_FLOAT              0x19
#define TA_VERTEX_TEX_UV16_SPEC_FLOAT         0x1D
#define TA_VERTEX_INTENSITY                   0x20
#define TA_VERTEX_TEX_INTENSITY               0x28
#define TA_VERTEX_TEX_SPEC_INTENSITY          0x2C
#define TA_VERTEX_TEX_UV16_INTENSITY          0x29
#define TA_VERTEX_TEX_UV16_SPEC_INTENSITY     0x2D
#define TA_VERTEX_PACKED_MOD                  0x40
#define TA_VERTEX_TEX_PACKED_MOD              0x48
#define TA_VERTEX_TEX_SPEC_PACKED_MOD         0x4C
#define TA_VERTEX_TEX_UV16_PACKED_MOD         0x49
#define TA_VERTEX_TEX_UV16_SPEC_PACKED_MOD    0x4D
#define TA_VERTEX_INTENSITY_MOD               0x60
#define TA_VERTEX_TEX_INTENSITY_MOD           0x68
#define TA_VERTEX_TEX_SPEC_INTENSITY_MOD      0x6C
#define TA_VERTEX_TEX_UV16_INTENSITY_MOD      0x69
#define TA_VERTEX_TEX_UV16_SPEC_INTENSITY_MOD 0x6D
#define TA_VERTEX_SPRITE                      0x80
#define TA_VERTEX_TEX_SPRITE                  0x88
#define TA_VERTEX_MOD_VOLUME                  0x81
#define TA_VERTEX_LISTLESS                    0xFF

#define TA_IS_NORMAL_POLY() (ta_status.current_vertex_type < TA_VERTEX_SPRITE)

static int strip_lengths[4] = {3,4,6,8}; /* in vertexes */
#define TA_POLYCMD_LISTTYPE(i) ( ((i) >> 24) & 0x0F )
#define TA_POLYCMD_USELENGTH(i) ( i & 0x00800000 )
#define TA_POLYCMD_LENGTH(i)  strip_lengths[((i >> 18) & 0x03)]
#define TA_POLYCMD_CLIP(i)  ((i>>16)&0x03)
#define TA_POLYCMD_CLIP_NONE 0
#define TA_POLYCMD_CLIP_INSIDE 2
#define TA_POLYCMD_CLIP_OUTSIDE 3
#define TA_POLYCMD_COLOURFMT(i)  (i & 0x00000030)
#define TA_POLYCMD_COLOURFMT_ARGB32 0x00000000
#define TA_POLYCMD_COLOURFMT_FLOAT 0x00000010
#define TA_POLYCMD_COLOURFMT_INTENSITY 0x00000020
#define TA_POLYCMD_COLOURFMT_LASTINT 0x00000030

#define TA_POLYCMD_MODIFIED 0x00000080
#define TA_POLYCMD_FULLMOD  0x00000040
#define TA_POLYCMD_TEXTURED 0x00000008
#define TA_POLYCMD_SPECULAR 0x00000004
#define TA_POLYCMD_SHADED 0x00000002
#define TA_POLYCMD_UV16 0x00000001

#define TA_POLYCMD_IS_SPECULAR(i) ((i & 0x0000000C)==0x0000000C) /* Only applies to textured polys */
#define TA_POLYCMD_IS_FULLMOD(i) ((i & 0x000000C0)==0x000000C0)


#define TA_IS_END_VERTEX(i) (i & 0x10000000)

/** Note these are not the IEEE 754 definitions - the TA treats NANs
 * as if they were INFs of the appropriate sign.
 */
#define TA_IS_INF(f) (((*((uint32_t *)&f)) & 0xFF800000) == 0x7F800000)
#define TA_IS_NINF(f) (((*((uint32_t *)&f)) & 0xFF800000) == 0xFF800000)

#define MIN3( x1, x2, x3 ) ( (x1)<(x2)? ((x1)<(x3)?(x1):(x3)) : ((x2)<(x3)?(x2):(x3)) )
#define MAX3( x1, x2, x3 ) ( (x1)>(x2)? ((x1)>(x3)?(x1):(x3)) : ((x2)>(x3)?(x2):(x3)) )

#define TILESLOT( x, y ) (ta_status.current_tile_matrix + (ta_status.current_tile_size * (y * ta_status.width+ x) << 2))

struct pvr2_ta_vertex {
    float x,y,z;
    uint32_t detail[8]; /* 0-8 detail words */
};

struct pvr2_ta_status {
    int32_t state;
    int32_t width, height; /* Tile resolution, ie 20x15 */
    int32_t tilelist_dir; /* Growth direction of the tilelist, 0 = up, 1 = down */
    uint32_t tilelist_size; /* Size of the tilelist segments */
    uint32_t tilelist_start; /* Initial address of the tilelist */
    uint32_t polybuf_start; /* Initial bank address of the polygon buffer (ie &0x00F00000) */
    int32_t current_vertex_type;
    uint32_t accept_vertexes; /* 0 = NO, 1 = YES */
    int32_t vertex_count; /* index of last start-vertex seen, or -1 if no vertexes 
     * are present
     */
    uint32_t max_vertex;     /* Maximum number of vertexes in the current polygon (3/4/6/8) */
    uint32_t current_list_type;
    uint32_t current_tile_matrix; /* Memory location of the first tile for the current list. */
    uint32_t current_tile_size; /* Size of the tile matrix space  in 32-bit words (0/8/16/32)*/
    uint32_t intensity1, intensity2;
    struct tile_bounds clip;
    int32_t clip_mode;
    /**
     * Current working object
     */
    int32_t poly_context_size;
    int32_t poly_vertex_size;
    int32_t poly_parity;
    uint32_t poly_context[5];
    uint32_t poly_pointer;
    struct tile_bounds last_triangle_bounds;
    struct pvr2_ta_vertex poly_vertex[8];
    uint32_t debug_output;
};

static struct pvr2_ta_status ta_status;

static int tilematrix_sizes[4] = {0,8,16,32};

/**
 * Convenience union - ta data is either 32-bit integer or 32-bit float.
 */
union ta_data {
    unsigned int i;
    float f;
};


void lxd_ta_reset() {
    ta_status.state = STATE_ERROR; /* State not valid until initialized */
    ta_status.debug_output = 0;
}

void lxd_ta_init(u8* vram) {
    pvr2_main_ram_hidden = vram;

    ta_status.state = STATE_IDLE;
    ta_status.current_list_type = -1;
    ta_status.current_vertex_type = -1;
    ta_status.poly_parity = 0;
    ta_status.vertex_count = 0;
    ta_status.max_vertex = 3;
    ta_status.current_vertex_type = TA_VERTEX_LISTLESS;
    ta_status.poly_vertex_size = 0;
    memset(&ta_status.poly_context[1], 0, 4);
    ta_status.last_triangle_bounds.x1 = -1;
    ta_status.accept_vertexes = TRUE;
    ta_status.clip.x1 = 0;
    ta_status.clip.y1 = 0;
    ta_status.clip_mode = TA_POLYCMD_CLIP_NONE;

    uint32_t size =  TA_GLOB_TILE_CLIP.full;// MMIO_READ( PVR2, TA_TILESIZE );
    ta_status.width = (size & 0xFFFF) + 1;
    ta_status.height = (size >> 16) + 1;
    ta_status.clip.x2 = ta_status.width-1;
    ta_status.clip.y2 = ta_status.height-1;
    uint32_t control = TA_ALLOC_CTRL; //MMIO_READ( PVR2, TA_TILECFG );
    ta_status.tilelist_dir = (control >> 20) & 0x01;
    ta_status.tilelist_size = tilematrix_sizes[ (control & 0x03) ];
    TA_ISP_CURRENT = TA_ISP_BASE;
    //MMIO_WRITE( PVR2, TA_POLYPOS, MMIO_READ( PVR2, TA_POLYBASE ) );
    uint32_t plistpos = TA_NEXT_OPB_INIT >> 2; //MMIO_READ( PVR2, TA_LISTBASE )
    if( ta_status.tilelist_dir == TA_GROW_DOWN ) {
        plistpos -= ta_status.tilelist_size;
    }
    TA_NEXT_OPB = plistpos;
    //MMIO_WRITE( PVR2, TA_LISTPOS, plistpos );
    ta_status.tilelist_start = plistpos;
    ta_status.polybuf_start = TA_ISP_BASE & 0x00F00000; // MMIO_READ( PVR2, TA_POLYBASE )
}

static uint32_t parse_float_colour( float a, float r, float g, float b ) {
    int ai,ri,gi,bi;

    if( TA_IS_INF(a) ) {
        ai = 255;
    } else {
        ai = 256 * CLAMP(a,0.0,1.0) - 1;
        if( ai < 0 ) ai = 0;
    }
    if( TA_IS_INF(r) ) {
        ri = 255;
    } else {
        ri = 256 * CLAMP(r,0.0,1.0) - 1;
        if( ri < 0 ) ri = 0;
    }
    if( TA_IS_INF(g) ) {
        gi = 255;
    } else {
        gi = 256 * CLAMP(g,0.0,1.0) - 1;
        if( gi < 0 ) gi = 0;
    }
    if( TA_IS_INF(b) ) {
        bi = 255;
    } else {
        bi = 256 * CLAMP(b,0.0,1.0) - 1;
        if( bi < 0 ) bi = 0;
    }
    return (ai << 24) | (ri << 16) | (gi << 8) | bi;
}

static uint32_t parse_intensity_colour( uint32_t base, float intensity )
{
    unsigned int i = (unsigned int)(256 * CLAMP(intensity, 0.0,1.0));

    return
    (((((base & 0xFF) * i) & 0xFF00) |
            (((base & 0xFF00) * i) & 0xFF0000) |
            (((base & 0xFF0000) * i) & 0xFF000000)) >> 8) |
            (base & 0xFF000000);
}

/**
 * Initialize the specified TA list.
 */
static void ta_init_list( unsigned int listtype ) {
    int config = TA_ALLOC_CTRL;//MMIO_READ( PVR2, TA_TILECFG );
    int tile_matrix = TA_OL_BASE; //MMIO_READ( PVR2, TA_TILEBASE );
    int list_end = TA_OL_LIMIT;//MMIO_READ( PVR2, TA_LISTEND );

    ta_status.current_tile_matrix = tile_matrix;

    /* If the list grows down, the end must be < tile matrix start. 
     * If it grows up, the end must be > tile matrix start.
     * Don't ask me why, it just does...
     */
    if( ((ta_status.tilelist_dir == TA_GROW_DOWN && list_end <= tile_matrix) ||
            (ta_status.tilelist_dir == TA_GROW_UP && list_end >= tile_matrix )) &&
            listtype <= TA_LIST_PUNCH_OUT ) {
        int i;
        uint32_t *p;
        for( i=0; i < listtype; i++ ) {
            int size = tilematrix_sizes[(config & 0x03)] << 2;
            ta_status.current_tile_matrix += ta_status.width * ta_status.height * size;
            config >>= 4;
        }
        ta_status.current_tile_size = tilematrix_sizes[(config & 0x03)];

        /* Initialize each tile to 0xF0000000 */
        if( ta_status.current_tile_size != 0 ) {
            pvraddr_t p = (ta_status.current_tile_matrix);
            for( i=0; i< ta_status.width * ta_status.height; i++ ) {
                PVRRAM(p) = 0xF0000000;
                p += ta_status.current_tile_size * 4;
            }
        }
    } else {
        ta_status.current_tile_size = 0;
    }

    if( tile_matrix == list_end ) {
        ta_status.current_tile_size = 0;
    }

    ta_status.state = STATE_IN_LIST;
    ta_status.current_list_type = listtype;
    ta_status.last_triangle_bounds.x1 = -1;
}

/*
static int list_events[5] = {EVENT_PVR_OPAQUE_DONE, EVENT_PVR_OPAQUEMOD_DONE, 
        EVENT_PVR_TRANS_DONE, EVENT_PVR_TRANSMOD_DONE,
        EVENT_PVR_PUNCHOUT_DONE };
*/
static HollyInterruptID list_events[5] = {holly_OPAQUE, holly_OPAQUEMOD, 
        holly_TRANS, holly_TRANSMOD,
        holly_PUNCHTHRU };

static void ta_end_list() {
    if( ta_status.current_list_type != TA_LIST_NONE ) {
        //asic_event( list_events[ta_status.current_list_type] );
        asic_RaiseInterrupt(list_events[ta_status.current_list_type]);
    }
    ta_status.current_list_type = TA_LIST_NONE;
    ta_status.current_vertex_type = TA_VERTEX_LISTLESS;
    ta_status.poly_vertex_size = 0;
    memset(&ta_status.poly_context[1], 0, 4);
    ta_status.state = STATE_IDLE;
}

static void ta_bad_input_error() {
    verify(false);
    //asic_event( EVENT_PVR_BAD_INPUT );
}

/**
 * Write data out to the polygon buffer.
 * If the end-of-buffer is reached, asserts EVENT_PVR_PRIM_ALLOC_FAIL
 * @param data to be written
 * @param length Number of 32-bit words to write.
 * @return number of words actually written
 */
static int ta_write_polygon_buffer( uint32_t *data, int length )
{
    int rv;
    int posn = TA_ISP_CURRENT;// MMIO_READ( PVR2, TA_POLYPOS );
    int end = TA_ISP_LIMIT;// MMIO_READ( PVR2, TA_POLYEND );
    pvraddr_t target = posn;
    for( rv=0; rv < length; rv++ ) {
        if( posn == end ) {
            verify(false);
            //asic_event( EVENT_PVR_PRIM_ALLOC_FAIL );
            ////	    ta_status.state = STATE_ERROR;
            break;
        }
        if( posn < PVR2_RAM_SIZE ) {
            PVRRAM(posn) = *data++;
        }
        posn += 4;
    }

    TA_ISP_CURRENT = posn;
    //MMIO_WRITE( PVR2, TA_POLYPOS, posn );
    return rv;
}

#define TA_NO_ALLOC 0xFFFFFFFF

/**
 * Allocate a new tile list block from the grow space and update the
 * word at reference to be a link to the new block.
 */
static uint32_t ta_alloc_tilelist( uint32_t reference ) {
    uint32_t posn = TA_NEXT_OPB;//MMIO_READ( PVR2, TA_LISTPOS );
    uint32_t limit = TA_OL_LIMIT >> 2;//MMIO_READ( PVR2, TA_LISTEND ) >> 2;
    uint32_t newposn;
    if( ta_status.tilelist_dir == TA_GROW_DOWN ) {
        newposn = posn - ta_status.tilelist_size;
        if( posn == limit ) {
            PVRRAM(posn<<2) = 0xF0000000;
            PVRRAM(reference) = 0xE0000000 | (posn<<2);
            return TA_NO_ALLOC;
        } else if( posn < limit ) {
            PVRRAM(reference) = 0xE0000000 | (posn<<2);
            return TA_NO_ALLOC;
        } else if( newposn <= limit ) {
        } else if( newposn <= (limit + ta_status.tilelist_size) ) {
            verify(false);
            //asic_event( EVENT_PVR_MATRIX_ALLOC_FAIL );
            TA_NEXT_OPB = newposn;
            //MMIO_WRITE( PVR2, TA_LISTPOS, newposn );
        } else {
            TA_NEXT_OPB = newposn;
            //MMIO_WRITE( PVR2, TA_LISTPOS, newposn );
        }
        PVRRAM(reference) = 0xE0000000 | (posn<<2);
        return posn << 2;
    } else {
        newposn = posn + ta_status.tilelist_size;
        if( posn == limit ) {
            PVRRAM(posn<<2) = 0xF0000000;
            PVRRAM(reference) = 0xE0000000 | (posn<<2);
            return TA_NO_ALLOC;
        } else if ( posn > limit ) {
            PVRRAM(reference) = 0xE0000000 | (posn<<2);
            return TA_NO_ALLOC;
        } else if( newposn >= limit ) {
        } else if( newposn >= (limit - ta_status.tilelist_size) ) {
            verify(false);
            //asic_event( EVENT_PVR_MATRIX_ALLOC_FAIL );
            TA_NEXT_OPB = newposn;
            //MMIO_WRITE( PVR2, TA_LISTPOS, newposn );
        } else {
            TA_NEXT_OPB = newposn;
            //MMIO_WRITE( PVR2, TA_LISTPOS, newposn );
        }	    
        PVRRAM(reference) = 0xE0000000 | (posn<<2);
        return posn << 2;
    }
}

/**
 * Write a tile entry out to the matrix.
 */
static void ta_write_tile_entry( int x, int y, uint32_t tile_entry ) {
    uint32_t tile = TILESLOT(x,y);
    uint32_t tilestart = tile;
    uint32_t value;
    uint32_t lasttri = 0;
    int i;

    if( ta_status.clip_mode == TA_POLYCMD_CLIP_OUTSIDE &&
            x >= ta_status.clip.x1 && x <= ta_status.clip.x2 &&
            y >= ta_status.clip.y1 && y <= ta_status.clip.y2 ) {
        /* Tile clipped out */
        return;
    }

    if( (tile_entry & 0x80000000) && 
            ta_status.last_triangle_bounds.x1 != -1 &&
            ta_status.last_triangle_bounds.x1 <= x &&
            ta_status.last_triangle_bounds.x2 >= x &&
            ta_status.last_triangle_bounds.y1 <= y &&
            ta_status.last_triangle_bounds.y2 >= y ) {
        /* potential for triangle stacking */
        lasttri = tile_entry & 0xE1E00000;
    }


    if( PVRRAM(tile) == 0xF0000000 ) {
        PVRRAM(tile) = tile_entry;
        PVRRAM(tile+4) = 0xF0000000;
        return;
    }

    while(1) {
        value = PVRRAM(tile);
        for( i=1; i<ta_status.current_tile_size; i++ ) {
            tile += 4;
            uint32_t nextval = PVRRAM(tile);
            if( nextval == 0xF0000000 ) {
                if( lasttri != 0 && lasttri == (value&0xE1E00000) ) {
                    int count = (value & 0x1E000000) + 0x02000000;
                    if( count < 0x20000000 ) {
                        PVRRAM(tile-4) = (value & 0xE1FFFFFF) | count;
                        return;
                    }
                }
                if( i < ta_status.current_tile_size-1 ) {
                    PVRRAM(tile) = tile_entry;
                    PVRRAM(tile+4) = 0xF0000000;
                    return;
                }
            }
            value = nextval;
        }

        if( value == 0xF0000000 ) {
            tile = ta_alloc_tilelist(tile);
            if( tile != TA_NO_ALLOC ) {
                PVRRAM(tile) = tile_entry;
                PVRRAM(tile+4) = 0xF0000000;
            }
            return;
        } else if( (value & 0xFF000000) == 0xE0000000 ) {
            value &= 0x00FFFFFF;
            if( value == tilestart )
                return; /* Loop */
            tilestart = tile = value;
        } else {
            /* This should never happen */
            return;
        }
    }
}

/**
 * Write a completed polygon out to the memory buffers 
 * OPTIMIZEME: This is not terribly efficient at the moment.
 */
static void ta_commit_polygon( ) {
    int i, x, y;
    int tx[ta_status.vertex_count], ty[ta_status.vertex_count];
    struct tile_bounds triangle_bound[ta_status.vertex_count - 2];
    struct tile_bounds polygon_bound;
    uint32_t poly_context[5];

    memcpy( poly_context, ta_status.poly_context, ta_status.poly_context_size * 4 );

    /* Compute the tile coordinates for each vertex (need to be careful with
     * clamping here)
     */
    for( i=0; i<ta_status.vertex_count; i++ ) {
        if( ta_status.poly_vertex[i].x < 0.0 || TA_IS_NINF(ta_status.poly_vertex[i].x) ) {
            tx[i] = -1;
        } else if( ta_status.poly_vertex[i].x > (float)INT_MAX || TA_IS_INF(ta_status.poly_vertex[i].x) ) {
            tx[i] = INT_MAX/32;
        } else {
            tx[i] = (int)(ta_status.poly_vertex[i].x / 32.0);
        }
        if( ta_status.poly_vertex[i].y < 0.0 || TA_IS_NINF(ta_status.poly_vertex[i].y)) {
            ty[i] = -1;
        } else if( ta_status.poly_vertex[i].y > (float)INT_MAX || TA_IS_INF(ta_status.poly_vertex[i].y) ) {
            ty[i] = INT_MAX/32;
        } else {
            ty[i] = (int)(ta_status.poly_vertex[i].y / 32.0);
        }

    }

    /* Compute bounding box for each triangle individually, as well
     * as the overall polygon.
     */

    triangle_bound[0].x1 = MIN3(tx[0],tx[1],tx[2]);
    triangle_bound[0].x2 = MAX3(tx[0],tx[1],tx[2]);
    triangle_bound[0].y1 = MIN3(ty[0],ty[1],ty[2]);
    triangle_bound[0].y2 = MAX3(ty[0],ty[1],ty[2]);
    polygon_bound.x1 = triangle_bound[0].x1;
    polygon_bound.y1 = triangle_bound[0].y1;
    polygon_bound.x2 = triangle_bound[0].x2;
    polygon_bound.y2 = triangle_bound[0].y2;

    for( i=1; i<ta_status.vertex_count-2; i++ ) {
        triangle_bound[i].x1 = MIN3(tx[i],tx[i+1],tx[i+2]);
        triangle_bound[i].x2 = MAX3(tx[i],tx[i+1],tx[i+2]);
        triangle_bound[i].y1 = MIN3(ty[i],ty[i+1],ty[i+2]);
        triangle_bound[i].y2 = MAX3(ty[i],ty[i+1],ty[i+2]);
        polygon_bound.x1 = MIN(polygon_bound.x1, triangle_bound[i].x1);
        polygon_bound.x2 = MAX(polygon_bound.x2, triangle_bound[i].x2);
        polygon_bound.y1 = MIN(polygon_bound.y1, triangle_bound[i].y1);
        polygon_bound.y2 = MAX(polygon_bound.y2, triangle_bound[i].y2);
    }

    /* Clamp the polygon bounds to the frustum */
    if( polygon_bound.x1 < 0 ) polygon_bound.x1 = 0;
    if( polygon_bound.x2 >= ta_status.width ) polygon_bound.x2 = ta_status.width-1;
    if( polygon_bound.y1 < 0 ) polygon_bound.y1 = 0;
    if( polygon_bound.y2 >= ta_status.width ) polygon_bound.y2 = ta_status.height-1;

    /* Set the "single tile" flag if it's entirely contained in 1 tile */
    if( polygon_bound.x1 == polygon_bound.x2 &&
            polygon_bound.y1 == polygon_bound.y2 ) {
        poly_context[0] |= 0x00200000;
    }

    /* If the polygon is entirely clipped, don't even write the polygon data */
    switch( ta_status.clip_mode ) {
    case TA_POLYCMD_CLIP_NONE:
        if( polygon_bound.x2 < 0 || polygon_bound.x1 >= ta_status.width ||
                polygon_bound.y2 < 0 || polygon_bound.y1 >= ta_status.height ) {
            return;
        }
        break;
    case TA_POLYCMD_CLIP_INSIDE:
        if( polygon_bound.x2 < ta_status.clip.x1 || polygon_bound.x1 > ta_status.clip.x2 ||
                polygon_bound.y2 < ta_status.clip.y1 || polygon_bound.y1 > ta_status.clip.y2 ) {
            return;
        } else {
            /* Clamp to clip bounds */
            if( polygon_bound.x1 < ta_status.clip.x1 ) polygon_bound.x1 = ta_status.clip.x1;
            if( polygon_bound.x2 > ta_status.clip.x2 ) polygon_bound.x2 = ta_status.clip.x2;
            if( polygon_bound.y1 < ta_status.clip.y1 ) polygon_bound.y1 = ta_status.clip.y1;
            if( polygon_bound.y2 > ta_status.clip.y2 ) polygon_bound.y2 = ta_status.clip.y2;
        }
        break;
    case TA_POLYCMD_CLIP_OUTSIDE:
        if( polygon_bound.x1 >= ta_status.clip.x1 && polygon_bound.x2 <= ta_status.clip.x2 &&
                polygon_bound.y1 >= ta_status.clip.y1 && polygon_bound.y2 <= ta_status.clip.y2 ) {
            return;
        }
        break;
    }

    /* Ok, we're good to go - write out the polygon first */
    uint32_t tile_entry = (TA_ISP_CURRENT/*MMIO_READ( PVR2, TA_POLYPOS )*/ - ta_status.polybuf_start) >> 2 | 
    ta_status.poly_pointer;

    int status = ta_write_polygon_buffer( poly_context, ta_status.poly_context_size );
    if( status == 0 ) {
        /* No memory available - abort */
        return;
    } else {
        for( i=0; i<ta_status.vertex_count && status != 0; i++ ) {
            status = ta_write_polygon_buffer( (uint32_t *)(&ta_status.poly_vertex[i]), 3 + ta_status.poly_vertex_size );
        }
    }

    if( ta_status.current_tile_size == 0 ) {
        /* No memory for tile entry, so don't write anything */
        return;
    }

    /* And now the tile entries. Triangles are different from everything else */
    if( ta_status.vertex_count == 3 ) {
        tile_entry |= 0x80000000;
        for( y=polygon_bound.y1; y<=polygon_bound.y2; y++ ) {
            for( x=polygon_bound.x1; x<=polygon_bound.x2; x++ ) {
                ta_write_tile_entry( x,y,tile_entry );
            }
        }
        ta_status.last_triangle_bounds.x1 = polygon_bound.x1;
        ta_status.last_triangle_bounds.y1 = polygon_bound.y1;
        ta_status.last_triangle_bounds.x2 = polygon_bound.x2;
        ta_status.last_triangle_bounds.y2 = polygon_bound.y2;
    } else if( ta_status.current_vertex_type == TA_VERTEX_SPRITE ||
            ta_status.current_vertex_type == TA_VERTEX_TEX_SPRITE ) {
        tile_entry |= 0xA0000000;
        for( y=polygon_bound.y1; y<=polygon_bound.y2; y++ ) {
            for( x=polygon_bound.x1; x<=polygon_bound.x2; x++ ) {
                ta_write_tile_entry( x,y,tile_entry );
            }
        }
        ta_status.last_triangle_bounds.x1 = polygon_bound.x1;
        ta_status.last_triangle_bounds.y1 = polygon_bound.y1;
        ta_status.last_triangle_bounds.x2 = polygon_bound.x2;
        ta_status.last_triangle_bounds.y2 = polygon_bound.y2;
    } else {
        for( y=polygon_bound.y1; y<=polygon_bound.y2; y++ ) {
            for( x=polygon_bound.x1; x<=polygon_bound.x2; x++ ) {
                uint32_t entry = tile_entry;
                for( i=0; i<ta_status.vertex_count-2; i++ ) {
                    if( triangle_bound[i].x1 <= x && triangle_bound[i].x2 >= x &&
                            triangle_bound[i].y1 <= y && triangle_bound[i].y2 >= y ) {
                        entry |= (0x40000000>>i);
                    }
                }
                ta_write_tile_entry( x, y, entry );
            }
        }
        ta_status.last_triangle_bounds.x1 = -1;
    }
}

/**
 * Variant of ta_split_polygon called when vertex_count == max_vertex, but 
 * the client hasn't sent the LAST VERTEX flag. Commit the poly as normal
 * first, then start a new poly with the first 2 vertexes taken from the 
 * current one.
 */
static void ta_split_polygon() {
    ta_commit_polygon();
    if( TA_IS_NORMAL_POLY() ) { 
        /* This only applies to ordinary polys - Sprites + modifier lists are
         * handled differently
         */
        if( ta_status.vertex_count == 3 ) {
            /* Triangles use an odd/even scheme */
            if( ta_status.poly_parity == 0 ) {
                memcpy( &ta_status.poly_vertex[0], &ta_status.poly_vertex[2], 
                        sizeof(struct pvr2_ta_vertex) );
                ta_status.poly_parity = 1;
            } else {
                memcpy( &ta_status.poly_vertex[1], &ta_status.poly_vertex[2],
                        sizeof(struct pvr2_ta_vertex) );
                ta_status.poly_parity = 0;
            }
        } else {
            /* Everything else just uses the last 2 vertexes in order */
            memcpy( &ta_status.poly_vertex[0], &ta_status.poly_vertex[ta_status.vertex_count-2], 
                    sizeof(struct pvr2_ta_vertex)*2 );
            ta_status.poly_parity = 0;
        }
        ta_status.vertex_count = 2;
    } else {
        ta_status.vertex_count = 0;
    }
}

/**
 * Parse the polygon context block and setup the internal state to receive
 * vertexes.
 * @param data 32 bytes of parameter data.
 */
static void ta_parse_polygon_context( union ta_data *data ) {
    int colourfmt = TA_POLYCMD_COLOURFMT(data[0].i);
    if( TA_POLYCMD_USELENGTH(data[0].i) ) {
        ta_status.max_vertex = TA_POLYCMD_LENGTH(data[0].i);
    }
    ta_status.clip_mode = TA_POLYCMD_CLIP(data[0].i);
    if( ta_status.clip_mode == 1 ) { /* Reserved - treat as CLIP_INSIDE */
        ta_status.clip_mode = TA_POLYCMD_CLIP_INSIDE;
    }
    ta_status.vertex_count = 0;
    ta_status.poly_context[0] = 
        (data[1].i & 0xFC1FFFFF) | ((data[0].i & 0x0B) << 22);
    ta_status.poly_context[1] = data[2].i;
    ta_status.poly_context[3] = data[4].i;
    ta_status.poly_parity = 0;
    if( data[0].i & TA_POLYCMD_TEXTURED ) {
        ta_status.current_vertex_type = data[0].i & 0x0D;
        ta_status.poly_context[2] = data[3].i;
        ta_status.poly_context[4] = data[5].i;
        if( data[0].i & TA_POLYCMD_SPECULAR ) {
            ta_status.poly_context[0] |= 0x01000000;
            ta_status.poly_vertex_size = 4;
        } else {
            ta_status.poly_vertex_size = 3;
        }
        if( data[0].i & TA_POLYCMD_UV16 ) {
            ta_status.poly_vertex_size--;
        }
    } else {
        ta_status.current_vertex_type = 0;
        ta_status.poly_vertex_size = 1;
        ta_status.poly_context[2] = 0;
        ta_status.poly_context[4] = 0;
    }

    ta_status.poly_pointer = (ta_status.poly_vertex_size << 21);
    ta_status.poly_context_size = 3;
    if( data[0].i & TA_POLYCMD_MODIFIED ) {
        ta_status.poly_pointer |= 0x01000000;
        if( data[0].i & TA_POLYCMD_FULLMOD ) {
            ta_status.poly_context_size = 5;
            ta_status.poly_vertex_size <<= 1;
            ta_status.current_vertex_type |= 0x40;
            /* Modified/float not supported - behaves as per last intensity */
            if( colourfmt == TA_POLYCMD_COLOURFMT_FLOAT ) {
                colourfmt = TA_POLYCMD_COLOURFMT_LASTINT;
            }
        }
    }

    if( colourfmt == TA_POLYCMD_COLOURFMT_INTENSITY ) {
        if( TA_POLYCMD_IS_FULLMOD(data[0].i) ||
                TA_POLYCMD_IS_SPECULAR(data[0].i) ) {
            ta_status.state = STATE_EXPECT_POLY_BLOCK2;
        } else {
            ta_status.intensity1 = 
                parse_float_colour( data[4].f, data[5].f, data[6].f, data[7].f );
        }
    } else if( colourfmt == TA_POLYCMD_COLOURFMT_LASTINT ) {
        colourfmt = TA_POLYCMD_COLOURFMT_INTENSITY;
    }

    ta_status.current_vertex_type |= colourfmt;
}

/**
 * Parse the modifier volume context block and setup the internal state to 
 * receive modifier vertexes.
 * @param data 32 bytes of parameter data.
 */
static void ta_parse_modifier_context( union ta_data *data ) {
    ta_status.current_vertex_type = TA_VERTEX_MOD_VOLUME;
    ta_status.poly_vertex_size = 0;
    ta_status.clip_mode = TA_POLYCMD_CLIP(data[0].i);
    if( ta_status.clip_mode == 1 ) { /* Reserved - treat as CLIP_INSIDE */
        ta_status.clip_mode = TA_POLYCMD_CLIP_INSIDE;
    }
    ta_status.poly_context_size = 3;
    ta_status.poly_context[0] = (data[1].i & 0xFC1FFFFF) |
    ((data[0].i & 0x0B)<<22);
    if( TA_POLYCMD_IS_SPECULAR(data[0].i) ) {
        ta_status.poly_context[0] |= 0x01000000;
    }
    ta_status.poly_context[1] = 0;
    ta_status.poly_context[2] = 0;
    ta_status.vertex_count = 0;
    ta_status.max_vertex = 3;
    ta_status.poly_pointer = 0;
}

/**
 * Parse the sprite context block and setup the internal state to receive
 * vertexes.
 * @param data 32 bytes of parameter data.
 */
static void ta_parse_sprite_context( union ta_data *data ) {
    ta_status.poly_context_size = 3;
    ta_status.poly_context[0] = (data[1].i & 0xFC1FFFFF) |
    ((data[0].i & 0x0B)<<22) | 0x00400000;
    ta_status.clip_mode = TA_POLYCMD_CLIP(data[0].i);
    if( ta_status.clip_mode == 1 ) { /* Reserved - treat as CLIP_INSIDE */
        ta_status.clip_mode = TA_POLYCMD_CLIP_INSIDE;
    }
    if( TA_POLYCMD_IS_SPECULAR(data[0].i) ) {
        ta_status.poly_context[0] |= 0x01000000;
    }
    ta_status.poly_context[1] = data[2].i;
    ta_status.poly_context[2] = data[3].i;
    if( data[0].i & TA_POLYCMD_TEXTURED ) {
        ta_status.poly_vertex_size = 2;
        ta_status.poly_vertex[2].detail[1] = data[4].i;
        ta_status.current_vertex_type = TA_VERTEX_TEX_SPRITE;
    } else {
        ta_status.poly_vertex_size = 1;
        ta_status.poly_vertex[2].detail[0] = data[4].i;
        ta_status.current_vertex_type = TA_VERTEX_SPRITE;
    }
    ta_status.vertex_count = 0;
    ta_status.max_vertex = 4;
    ta_status.poly_pointer = (ta_status.poly_vertex_size << 21);
}

/**
 * Copy the last read vertex into all vertexes up to max_vertex. Used for
 * Aborted polygons under some circumstances.
 */
static void ta_fill_vertexes( ) {
    int i;
    for( i=ta_status.vertex_count; i<ta_status.max_vertex; i++ ) {
        memcpy( &ta_status.poly_vertex[i], &ta_status.poly_vertex[ta_status.vertex_count-1],
                sizeof( struct pvr2_ta_vertex ) );
    }
}

static void ta_parse_vertex( union ta_data *data ) {
    struct pvr2_ta_vertex *vertex = &ta_status.poly_vertex[ta_status.vertex_count];
    vertex->x = data[1].f;
    vertex->y = data[2].f;
    vertex->z = data[3].f;

    switch( ta_status.current_vertex_type ) {
    case TA_VERTEX_PACKED:
        vertex->detail[0] = data[6].i;
        break;
    case TA_VERTEX_FLOAT:
        vertex->detail[0] = parse_float_colour( data[4].f, data[5].f, data[6].f, data[7].f );
        break;
    case TA_VERTEX_INTENSITY:
        vertex->detail[0] = parse_intensity_colour( ta_status.intensity1, data[6].f );
        break;

    case TA_VERTEX_TEX_SPEC_PACKED:
        vertex->detail[3] = data[7].i; /* ARGB */
        /* Fallthrough */
    case TA_VERTEX_TEX_PACKED:
        vertex->detail[0] = data[4].i; /* U */
        vertex->detail[1] = data[5].i; /* V */
        vertex->detail[2] = data[6].i; /* ARGB */
        break;
    case TA_VERTEX_TEX_UV16_SPEC_PACKED:
        vertex->detail[2] = data[7].i; /* ARGB */
        /* Fallthrough */
    case TA_VERTEX_TEX_UV16_PACKED:
        vertex->detail[0] = data[4].i; /* UV */
        vertex->detail[1] = data[6].i; /* ARGB */
        break;

    case TA_VERTEX_TEX_FLOAT:
    case TA_VERTEX_TEX_SPEC_FLOAT:
        vertex->detail[0] = data[4].i; /* U */
        vertex->detail[1] = data[5].i; /* UV */
        ta_status.state = STATE_EXPECT_VERTEX_BLOCK2;
        break;
    case TA_VERTEX_TEX_UV16_FLOAT:
    case TA_VERTEX_TEX_UV16_SPEC_FLOAT:
        vertex->detail[0] = data[4].i; /* UV */
        ta_status.state = STATE_EXPECT_VERTEX_BLOCK2;
        break;

    case TA_VERTEX_TEX_SPEC_INTENSITY:
        vertex->detail[3] = parse_intensity_colour( ta_status.intensity2, data[7].f );
        /* Fallthrough */
    case TA_VERTEX_TEX_INTENSITY:
        vertex->detail[0] = data[4].i; /* U */
        vertex->detail[1] = data[5].i; /* V */
        vertex->detail[2] = parse_intensity_colour( ta_status.intensity1, data[6].f );
        break;
    case TA_VERTEX_TEX_UV16_SPEC_INTENSITY:
        vertex->detail[2] = parse_intensity_colour( ta_status.intensity2, data[7].f );
        /* Fallthrough */
    case TA_VERTEX_TEX_UV16_INTENSITY:
        vertex->detail[0] = data[4].i; /* UV */
        vertex->detail[1] = parse_intensity_colour( ta_status.intensity1, data[6].f );
        break;

    case TA_VERTEX_PACKED_MOD:
        vertex->detail[0] = data[4].i; /* ARGB */
        vertex->detail[1] = data[5].i; /* ARGB */
        break;
    case TA_VERTEX_INTENSITY_MOD:
        vertex->detail[0] = parse_intensity_colour( ta_status.intensity1, data[4].f );
        vertex->detail[1] = parse_intensity_colour( ta_status.intensity2, data[5].f );
        break;

    case TA_VERTEX_TEX_SPEC_PACKED_MOD:
        vertex->detail[3] = data[7].i; /* ARGB0 */
        /* Fallthrough */
    case TA_VERTEX_TEX_PACKED_MOD:
        vertex->detail[0] = data[4].i; /* U0 */
        vertex->detail[1] = data[5].i; /* V0 */
        vertex->detail[2] = data[6].i; /* ARGB0 */
        ta_status.state = STATE_EXPECT_VERTEX_BLOCK2;
        break;
    case TA_VERTEX_TEX_UV16_SPEC_PACKED_MOD:
        vertex->detail[2] = data[7].i; /* ARGB0 */
        /* Fallthrough */
    case TA_VERTEX_TEX_UV16_PACKED_MOD:
        vertex->detail[0] = data[4].i; /* UV0 */
        vertex->detail[1] = data[6].i; /* ARGB0 */
        ta_status.state = STATE_EXPECT_VERTEX_BLOCK2;
        break;

    case TA_VERTEX_TEX_SPEC_INTENSITY_MOD:
        vertex->detail[3] = parse_intensity_colour( ta_status.intensity1, data[7].f );
        /* Fallthrough */
    case TA_VERTEX_TEX_INTENSITY_MOD:
        vertex->detail[0] = data[4].i; /* U0 */
        vertex->detail[1] = data[5].i; /* V0 */
        vertex->detail[2] = parse_intensity_colour( ta_status.intensity1, data[6].f );
        ta_status.state = STATE_EXPECT_VERTEX_BLOCK2;
        break;
    case TA_VERTEX_TEX_UV16_SPEC_INTENSITY_MOD:
        vertex->detail[2] = parse_intensity_colour( ta_status.intensity1, data[7].f );
        /* Fallthrough */
    case TA_VERTEX_TEX_UV16_INTENSITY_MOD:
        vertex->detail[0] = data[4].i; /* UV0 */
        vertex->detail[1] = parse_intensity_colour( ta_status.intensity1, data[6].f );
        ta_status.state = STATE_EXPECT_VERTEX_BLOCK2;
        break;

    case TA_VERTEX_SPRITE:
    case TA_VERTEX_TEX_SPRITE:
    case TA_VERTEX_MOD_VOLUME:
    case TA_VERTEX_LISTLESS:
        vertex++;
        vertex->x = data[4].f;
        vertex->y = data[5].f;
        vertex->z = data[6].f;
        vertex++;
        vertex->x = data[7].f;
        ta_status.vertex_count += 2;
        ta_status.state = STATE_EXPECT_VERTEX_BLOCK2;
        break;
    }
    ta_status.vertex_count++;
}

static void ta_parse_vertex_block2( union ta_data *data ) {
    struct pvr2_ta_vertex *vertex = &ta_status.poly_vertex[ta_status.vertex_count-1];

    switch( ta_status.current_vertex_type ) {
    case TA_VERTEX_TEX_SPEC_FLOAT:
        vertex->detail[3] = parse_float_colour( data[4].f, data[5].f, data[6].f, data[7].f );
        /* Fallthrough */
    case TA_VERTEX_TEX_FLOAT:
        vertex->detail[2] = parse_float_colour( data[0].f, data[1].f, data[2].f, data[3].f );
        break;
    case TA_VERTEX_TEX_UV16_SPEC_FLOAT:
        vertex->detail[2] = parse_float_colour( data[4].f, data[5].f, data[6].f, data[7].f );
        /* Fallthrough */
    case TA_VERTEX_TEX_UV16_FLOAT:
        vertex->detail[1] = parse_float_colour( data[0].f, data[1].f, data[2].f, data[3].f );
        break;
    case TA_VERTEX_TEX_PACKED_MOD:
        vertex->detail[3] = data[0].i; /* U1 */
        vertex->detail[4] = data[1].i; /* V1 */
        vertex->detail[5] = data[2].i; /* ARGB1 */
        break;
    case TA_VERTEX_TEX_SPEC_PACKED_MOD:
        vertex->detail[4] = data[0].i; /* U1 */
        vertex->detail[5] = data[1].i; /* V1 */
        vertex->detail[6] = data[2].i; /* ARGB1 */
        vertex->detail[7] = data[3].i; /* ARGB1 */
        break;
    case TA_VERTEX_TEX_UV16_PACKED_MOD:
        vertex->detail[2] = data[0].i; /* UV1 */
        vertex->detail[3] = data[2].i; /* ARGB1 */
        break;
    case TA_VERTEX_TEX_UV16_SPEC_PACKED_MOD:
        vertex->detail[3] = data[0].i; /* UV1 */
        vertex->detail[4] = data[2].i; /* ARGB1 */
        vertex->detail[5] = data[3].i; /* ARGB1 */
        break;

    case TA_VERTEX_TEX_INTENSITY_MOD:
        vertex->detail[3] = data[0].i; /* U1 */
        vertex->detail[4] = data[1].i; /* V1 */
        vertex->detail[5] = parse_intensity_colour( ta_status.intensity2, data[2].f ); /* ARGB1 */
        break;
    case TA_VERTEX_TEX_SPEC_INTENSITY_MOD:
        vertex->detail[4] = data[0].i; /* U1 */
        vertex->detail[5] = data[1].i; /* V1 */
        vertex->detail[6] = parse_intensity_colour( ta_status.intensity2, data[2].f ); /* ARGB1 */
        vertex->detail[7] = parse_intensity_colour( ta_status.intensity2, data[3].f ); /* ARGB1 */
        break;
    case TA_VERTEX_TEX_UV16_INTENSITY_MOD:
        vertex->detail[2] = data[0].i; /* UV1 */
        vertex->detail[3] = parse_intensity_colour( ta_status.intensity2, data[2].f ); /* ARGB1 */
        break;
    case TA_VERTEX_TEX_UV16_SPEC_INTENSITY_MOD:
        vertex->detail[3] = data[0].i; /* UV1 */
        vertex->detail[4] = parse_intensity_colour( ta_status.intensity2, data[2].f ); /* ARGB1 */
        vertex->detail[5] = parse_intensity_colour( ta_status.intensity2, data[3].f ); /* ARGB1 */
        break;

    case TA_VERTEX_SPRITE:
        vertex->y = data[0].f;
        vertex->z = data[1].f;
        vertex++;
        ta_status.vertex_count++;
        vertex->x = data[2].f;
        vertex->y = data[3].f;
        vertex->z = 0;
        vertex->detail[0] = 0;
        ta_status.poly_vertex[0].detail[0] = 0;
        ta_status.poly_vertex[1].detail[0] = 0;
        break;
    case TA_VERTEX_TEX_SPRITE:
        vertex->y = data[0].f;
        vertex->z = data[1].f;
        vertex++;
        ta_status.vertex_count++;
        vertex->x = data[2].f;
        vertex->y = data[3].f;
        vertex->z = 0;
        vertex->detail[0] = 0;
        vertex->detail[1] = 0;
        ta_status.poly_vertex[0].detail[0] = data[5].i;
        ta_status.poly_vertex[0].detail[1] = 0;
        ta_status.poly_vertex[1].detail[0] = data[6].i;
        ta_status.poly_vertex[1].detail[1] = 0;
        ta_status.poly_vertex[2].detail[0] = data[7].i;
        break;
    case TA_VERTEX_MOD_VOLUME:
    case TA_VERTEX_LISTLESS:
        vertex->y = data[0].f;
        vertex->z = data[1].f;
        break;
    }
    ta_status.state = STATE_IN_POLYGON;
}

/**
 * Process 1 32-byte block of ta data
 */
void pvr2_ta_process_block( unsigned char *input ) {
    union ta_data *data = (union ta_data *)input;

    switch( ta_status.state ) {
    case STATE_ERROR:
        /* Fatal error raised - stop processing until reset */
        return;

    case STATE_EXPECT_POLY_BLOCK2:
        /* This is always a pair of floating-point colours */
        ta_status.intensity1 = 
            parse_float_colour( data[0].f, data[1].f, data[2].f, data[3].f );
        ta_status.intensity2 =
            parse_float_colour( data[4].f, data[5].f, data[6].f, data[7].f );
        ta_status.state = STATE_IN_LIST;
        break;

    case STATE_EXPECT_VERTEX_BLOCK2:
        ta_parse_vertex_block2( data );
        if( ta_status.vertex_count == ta_status.max_vertex ) {
            ta_split_polygon();
        }
        break;

    case STATE_EXPECT_END_VERTEX_BLOCK2:
        ta_parse_vertex_block2( data );
        if( ta_status.vertex_count < 3 ) {
            ta_bad_input_error();
        } else {
            ta_commit_polygon();
        }
        ta_status.vertex_count = 0;
        ta_status.poly_parity = 0;
        ta_status.state = STATE_IN_LIST;
        break;
    case STATE_IN_LIST:
    case STATE_IN_POLYGON:
    case STATE_IDLE:
        switch( TA_CMD( data->i ) ) {
        case TA_CMD_END_LIST:
            if( ta_status.state == STATE_IN_POLYGON ) {
                ta_bad_input_error();
                ta_end_list();
                ta_status.state = STATE_ERROR; /* Abort further processing */
            } else {
                ta_end_list();
            }
            break;
        case TA_CMD_CLIP:
            if( ta_status.state == STATE_IN_POLYGON ) {
                ta_bad_input_error();
                ta_status.accept_vertexes = FALSE;
                /* Enter stuffed up mode */
            }
            ta_status.clip.x1 = data[4].i & 0x3F;
            ta_status.clip.y1 = data[5].i & 0x0F;
            ta_status.clip.x2 = data[6].i & 0x3F;
            ta_status.clip.y2 = data[7].i & 0x0F;
            if( ta_status.clip.x2 >= ta_status.width )
                ta_status.clip.x2 = ta_status.width - 1;
            if( ta_status.clip.y2 >= ta_status.height )
                ta_status.clip.y2 = ta_status.height - 1;
            break;
        case TA_CMD_POLYGON_CONTEXT:
            if( ta_status.state == STATE_IDLE ) {
                ta_init_list( TA_POLYCMD_LISTTYPE( data->i ) );
            }

            if( ta_status.vertex_count != 0 ) {
                /* Error, and not a very well handled one either */
                ta_bad_input_error();
                ta_status.accept_vertexes = FALSE;
            } else {
                if( TA_IS_MODIFIER_LIST( ta_status.current_list_type ) ) {
                    ta_parse_modifier_context(data);
                } else {
                    ta_parse_polygon_context(data);
                }
            }
            break;
        case TA_CMD_SPRITE_CONTEXT:
            if( ta_status.state == STATE_IDLE ) {
                ta_init_list( TA_POLYCMD_LISTTYPE( data->i ) );
            }

            if( ta_status.vertex_count != 0 ) {
                ta_fill_vertexes();
                ta_commit_polygon();
            }

            ta_parse_sprite_context(data);
            break;
        case TA_CMD_VERTEX:
            ta_status.state = STATE_IN_POLYGON;
            ta_parse_vertex(data);

            if( ta_status.state == STATE_EXPECT_VERTEX_BLOCK2 ) {
                if( TA_IS_END_VERTEX(data[0].i) ) {
                    ta_status.state = STATE_EXPECT_END_VERTEX_BLOCK2;
                }
            } else if( TA_IS_END_VERTEX(data->i) ) {
                if( ta_status.vertex_count < 3 ) {
                    ta_bad_input_error();
                } else {
                    ta_commit_polygon();
                }
                ta_status.vertex_count = 0;
                ta_status.poly_parity = 0;
                ta_status.state = STATE_IN_LIST;
            } else if( ta_status.vertex_count == ta_status.max_vertex ) {
                ta_split_polygon();
            }
            break;
        default:
            if( ta_status.state == STATE_IN_POLYGON ) {
                ta_bad_input_error();
            }
        }
        break;
    }

}

/**
 * Find the first polygon or sprite context in the supplied buffer of TA
 * data.
 * @return A pointer to the context, or NULL if it cannot be found 
 */
uint32_t *pvr2_ta_find_polygon_context( uint32_t *buf, uint32_t length )
{
    uint32_t *poly;
    for( poly = buf; poly < buf+(length>>2); poly += 8 ) {
        if( TA_CMD(*poly) == TA_CMD_POLYGON_CONTEXT ||
            TA_CMD(*poly) == TA_CMD_SPRITE_CONTEXT ) {
            return poly;
        }
    }
    return NULL;
}

/**
 * Write a block of data to the tile accelerator, adding the data to the 
 * current scene. We don't make any particular attempt to interpret the data
 * at this stage, deferring that until render time.
 *
 * Currently copies the data verbatim to the vertex buffer, processing only
 * far enough to generate the correct end-of-list events. Tile buffer is
 * entirely ignored.
 */
void lxd_ta_write( unsigned char *buf, uint32_t length )
{
    for( ; length >=32; length -= 32 ) {
        pvr2_ta_process_block( buf );
        buf += 32;
    }
}

void lxd_ta_write_burst( sh4addr_t addr, unsigned char *data )
{
    pvr2_ta_process_block( data );
}
