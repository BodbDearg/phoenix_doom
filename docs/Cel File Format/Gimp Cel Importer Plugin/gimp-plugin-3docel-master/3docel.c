/*
 * 3docel.c:	Reads old 3DO Cel files.
 *
 * 		Uses information obtained from the 3DO Portfolio programmers
 * 		toolkit, which may not be strictly kosher.
 *
 * Leo L. Schwab					2012.10.02
 */
#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
#include <errno.h>
#include <err.h>

#include <glib/gstdio.h>
#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

#include "3docel.h"


#define	LOAD_PROC	"file-3DO-load"


/***************************************************************************
 * Structure definitions.
 */
typedef struct pxdata {
	uint16_t		pel;	/*  Raw value from cel data  */
	uint16_t		p1555;	/*  After decoding through PLUT.  */
	uint8_t			p;	/*  P-mode value  */
	uint8_t			mr, mg, mb;	/*  Multiply value.  */
	uint8_t			dr, dg, db;	/*  Divide value.  */
	uint8_t			equ_alpha;	/*  Equivalent alpha value.  */
} pxdata;

typedef struct chunkdata {
	uint8_t			*base;
	uint32_t		size;
	uint32_t		idx;
	int8_t			bit_idx; /*  Index into current byte (7-0)  */
} chunkdata;


typedef struct celfile {
	GError			**error;
	FILE			*file;
	char const		*filename;
} celfile;

typedef struct celinfo {
	struct CCC_chunk	*ccc;
	struct PLUT_chunk	*plut;
	struct PDAT_chunk	*pdat;
	uint32_t		PRE0, PRE1;
	uint16_t		width, height;	/*  Computed from cel data  */
	uint8_t			bpp;
	uint8_t			packed;
	uint8_t			indexed;	/*  "Coded" in 3DO parlance */
	uint8_t			rep8;
	uint8_t			pluta;		/*  Pre-shifted  */
} celinfo;


typedef struct pstore_context {
	uint8_t			*dest;
	uint8_t			*cur;
	GimpImageBaseType	base_type;
	GimpImageType		img_type;
	gint32			gimage;
	gint32			glayer;
	uint32_t		stride;
	uint8_t			nchan;
} pstore_context;

typedef struct decode_context {
	struct celinfo		ci;
	struct chunkdata	cd;
	struct pstore_context	pstore;
	GError			**error;
	int			framenum;
	int			frametime;	/*  Milliseconds.  */
} decode_context;


struct cccflag {
	uint32_t		flag;
	char			*name;
};
#define	DEFFLAG(name)	{ MASKEXPAND (name), #name }

#define	NUMOF(array)	(sizeof (array) / sizeof (*(array)))


static int decode_cel (struct decode_context *ctx);


/***************************************************************************
 * Print some celinfo in human readable form.
 */
static struct cccflag cccflags[] = {
	DEFFLAG (CCB_SKIP),
	DEFFLAG (CCB_LAST),
	DEFFLAG (CCB_NXTPTRTYPE),
	DEFFLAG (CCB_SRCPTRTYPE),
	DEFFLAG (CCB_PLUTPTRTYPE),
	DEFFLAG (CCB_LDSIZE),
	DEFFLAG (CCB_LDPRS),
	DEFFLAG (CCB_LDPPMP),
	DEFFLAG (CCB_LDPLUT),
	DEFFLAG (CCB_PRELOC),
	DEFFLAG (CCB_YOXY),
	DEFFLAG (CCB_ACSC),
	DEFFLAG (CCB_ALSC),
	DEFFLAG (CCB_ACW),
	DEFFLAG (CCB_ACCW),
	DEFFLAG (CCB_TWD),
	DEFFLAG (CCB_LCE),
	DEFFLAG (CCB_ACE),
	DEFFLAG (CCB_MARIA),
	DEFFLAG (CCB_PXOR),
	DEFFLAG (CCB_USEAV),
	DEFFLAG (CCB_PACKED),
	DEFFLAG (CCB_PLUTPOS),
	DEFFLAG (CCB_BGND),
	DEFFLAG (CCB_NOBLK),
};

static void
dump_ccc_flags (struct CCC_chunk *ccc)
{
	register uint32_t	flags;
	int			i;

	flags = ccc->ccc_Flags;

	for (i = NUMOF (cccflags);  --i >= 0; ) {
		if (flags & cccflags[i].flag) {
			printf ("%s\n", cccflags[i].name);
		}
	}
}


/***************************************************************************
 * Incremental reading of a memory buffer
 */
static void
init_chunkdata (struct chunkdata *cd, uint8_t *base, uint32_t size)
{
	cd->base    = base;
	cd->size    = size;
	cd->idx     = 0;
	cd->bit_idx = 7;
}

/*
 * Read next N bits from chunkdata buffer.  MSBs are consumed first.
 */
static int
get_next_nbits (
uint32_t		*retval,
struct chunkdata	*cd,
int			nbits
)
{
	uint32_t	val = 0;
	int		n;

	while (nbits) {
		if (cd->idx + sizeof (uint8_t) >= cd->size)
			return EOF;

		n = nbits > cd->bit_idx + 1  ? cd->bit_idx + 1  : nbits;

		/*
		 * We're abusing the GetBF() macro, which normally takes a
		 * partial ternary expression of two constants (i.e. 7:4).
		 */
		val |= GetBF (cd->base[cd->idx],
		              (cd->bit_idx) : (cd->bit_idx - n + 1));

		/*  This byte exhausted; advance to next one.  */
		if ((cd->bit_idx -= n) < 0) {
			++cd->idx;
			cd->bit_idx = 7;
		}

		/*  Bits yet to get; make room for them.  */
		if ((nbits -= n) > 0)
			val <<= nbits;
	}
	*retval = val;
	return 0;
}

static int
unpk_flush_rest_of_word (struct chunkdata *cd)
{
	uint32_t newidx;

	newidx = cd->idx;
	if (cd->bit_idx < 7) {
		++newidx;
	}
	/*  Bump index up to next uint32_t boundary.  */
	newidx = (newidx + 3) & ~3;
	if (newidx >= cd->size)
		return EOF;

	cd->idx = newidx;
	cd->bit_idx = 7;
	return 0;
}


static int
get_u32 (uint32_t *val, struct chunkdata *cd)
{
	register uint8_t	*src;

	if (cd->idx + sizeof (uint32_t) >= cd->size)
		return EOF;

	src = cd->base + cd->idx;
	*val = (src[0] << 24) | (src[1] << 16) | (src[2] << 8) | src[3];
	cd->idx += sizeof (uint32_t);
	cd->bit_idx = 7;
	return 0;
}

static int
get_u16 (uint16_t *val, struct chunkdata *cd)
{
	register uint8_t	*src;

	if (cd->idx + sizeof (uint16_t) >= cd->size)
		return EOF;

	src = cd->base + cd->idx;
	*val = (src[0] << 8) | src[1];
	cd->idx += sizeof (uint16_t);
	cd->bit_idx = 7;
	return 0;
}

static int
get_u8 (uint8_t *val, struct chunkdata *cd)
{
	register uint8_t	*src;

	if (cd->idx + sizeof (uint8_t) >= cd->size)
		return EOF;

	src = cd->base + cd->idx;
	*val = *src;
	++cd->idx;
	cd->bit_idx = 7;
	return 0;
}


/***************************************************************************
 * Chunk reading and parsing.
 */
static int
skip_chunk_payload (struct chunk_header *hdr, struct celfile *cf)
{
	if (fseek (cf->file, hdr->size - sizeof (*hdr), SEEK_CUR) < 0)
		return -errno;

	return 0;
}

static int
read_n_bytes (void *buf, size_t siz, struct celfile *cf)
{
	size_t	n;

	n = fread (buf, 1, siz, cf->file);
	if (n != siz) {
		g_set_error (cf->error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
		             "%s: Unexpected end of file.",
			     gimp_filename_to_utf8 (cf->filename));
		return -EINVAL;
	}
	return 0;
}

static int
read_chunk_header (struct chunk_header *hdr, struct celfile *cf)
{
	size_t	n;

	n = fread (hdr, 1, sizeof (*hdr), cf->file);
	if (!n) {
		return EOF;
	} else if (n != sizeof (*hdr)) {
		return -EINVAL;
	}

	/*  Convert fields to local representation.  */
	hdr->size = be32toh (hdr->size);
	return 0;
}

static int
read_chunk_payload (
void			**cl_buf,
struct chunk_header	*hdr,
struct celfile		*cf
)
{
	/*  Mind the typecasting...  */
	chunk_header	*buf;
	int		e;

	if (!(buf = malloc (hdr->size)))
		return -ENOMEM;

	*buf = *hdr;
	if (e = read_n_bytes (buf + 1,
	                      hdr->size - sizeof (chunk_header),
			      cf))
	{
		free (buf);
		return e;
	}

	*cl_buf = (void *) buf;
	return 0;
}


static int
read_chunk_CCC (
struct CCC_chunk	**cl_CCC,
struct chunk_header	*hdr,
struct celfile		*cf
)
{
	struct CCC_chunk	*ccc;
	int			e;

	if (e = read_chunk_payload ((void **) &ccc, hdr, cf))
		return e;

	/*  Convert fields to local representation.  */
	ccc->ccc_version	= be32toh (ccc->ccc_version);
	ccc->ccc_Flags		= be32toh (ccc->ccc_Flags);
	ccc->ccc_NextPtr	= be32toh (ccc->ccc_NextPtr);
	ccc->ccc_CelData	= be32toh (ccc->ccc_CelData);
	ccc->ccc_PLUTPtr	= be32toh (ccc->ccc_PLUTPtr);
	ccc->ccc_X		= be32toh (ccc->ccc_X);
	ccc->ccc_Y		= be32toh (ccc->ccc_Y);
	ccc->ccc_hdx		= be32toh (ccc->ccc_hdx);
	ccc->ccc_hdy		= be32toh (ccc->ccc_hdy);
	ccc->ccc_vdx		= be32toh (ccc->ccc_vdx);
	ccc->ccc_vdy		= be32toh (ccc->ccc_vdy);
	ccc->ccc_ddx		= be32toh (ccc->ccc_ddx);
	ccc->ccc_ddy		= be32toh (ccc->ccc_ddy);
	ccc->ccc_PPMP1		= be16toh (ccc->ccc_PPMP1);
	ccc->ccc_PPMP0		= be16toh (ccc->ccc_PPMP0);
	ccc->ccc_PRE0		= be32toh (ccc->ccc_PRE0);
	ccc->ccc_PRE1		= be32toh (ccc->ccc_PRE1);
	ccc->ccc_Width		= be32toh (ccc->ccc_Width);
	ccc->ccc_Height		= be32toh (ccc->ccc_Height);

	*cl_CCC = ccc;

	return 0;
}

static int
read_chunk_PLUT (
struct PLUT_chunk	**cl_plut,
struct chunk_header	*hdr,
struct celfile		*cf
)
{
	struct PLUT_chunk	*plut;
	int32_t			maxentries;
	int			e;
	int			i;

	if (e = read_chunk_payload ((void **) &plut, hdr, cf))
		return e;

	/*
 	 * Independently calculate the number of PLUT entries based on the
	 * chunk size.
	 */
	maxentries = (plut->header.size
	              - sizeof (plut->header)
		      - sizeof (plut->numentries))
	           / sizeof (XRGB1555);

	/*  Convert fields to local representation.  */
	plut->numentries = be32toh (plut->numentries);

	if (plut->numentries > maxentries) {
		g_set_error (cf->error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
		             "%s: File corrupt (bad PLUT)",
			     gimp_filename_to_utf8 (cf->filename));
		free (plut);
		return -EINVAL;
	}

	for (i = 0;  i < plut->numentries;  ++i)
		plut->PLUT[i] = be16toh (plut->PLUT[i]);

	*cl_plut = plut;
	return 0;
}

static int
read_chunk_PDAT (
struct PDAT_chunk	**cl_pdat,
struct chunk_header	*hdr,
struct celfile		*cf
)
{
	struct PDAT_chunk	*pdat;
	int			e;
	uint8_t			*buf;

	if (e = read_chunk_payload ((void **) &pdat, hdr, cf)) {
		if (e == -ENOMEM)
			g_set_error (cf->error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
			             "Out of memory.");
		return e;
	}

	*cl_pdat = pdat;
	return 0;
}


static int
read_chunk_ANIM (
struct ANIM_chunk	**cl_anim,
struct chunk_header	*hdr,
struct celfile		*cf
)
{
	struct ANIM_chunk	*anim;
	int			e;
	uint8_t			*buf;

	if (e = read_chunk_payload ((void **) &anim, hdr, cf)) {
		if (e == -ENOMEM)
			g_set_error (cf->error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
			             "Out of memory.");
		return e;
	}

	/*  Convert fields to local representation.  */
	anim->version		= be32toh (anim->version);
	anim->animType		= be32toh (anim->animType);
	anim->numFrames		= be32toh (anim->numFrames);
	anim->frameRate		= be32toh (anim->frameRate);
	anim->startFrame	= be32toh (anim->startFrame);
	anim->numLoops		= be32toh (anim->numLoops);

	*cl_anim = anim;
	return 0;
}


static int
parse_file (struct decode_context *ctx, struct celfile *cf)
{
	chunk_header	ckhdr;
	ANIM_chunk	*anim;
	int		e = 0;
	uint8_t		*pdat;
	uint8_t		seen_first_chunk = 0;

	while (!(e = read_chunk_header (&ckhdr, cf))) {
		if (!seen_first_chunk) {
			switch (ckhdr.ID) {
			case CHUNK_CCB:
				ctx->framenum = 0;
				break;

			case CHUNK_ANIM:
				ctx->framenum = 1;
				break;

			default:
				g_set_error
				 (cf->error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
				  "%s: Not a 3DO cel file.",
				  gimp_filename_to_utf8 (cf->filename));
				return -EINVAL;
			}
			seen_first_chunk = 1;
		}

		switch (ckhdr.ID) {
		case CHUNK_CCB:
			if (ctx->ci.ccc) {
				free (ctx->ci.ccc);
				ctx->ci.ccc = NULL;
			}
			if (e = read_chunk_CCC (&ctx->ci.ccc, &ckhdr, cf))
				return e;
			break;

		case CHUNK_PLUT:
			if (ctx->ci.plut) {
				free (ctx->ci.plut);
				ctx->ci.plut = NULL;
			}
			if (e = read_chunk_PLUT (&ctx->ci.plut, &ckhdr, cf))
				return e;
			break;

		case CHUNK_PDAT:
			/*
 			 * A successful load of PDAT means it's time to
 			 * start interpreting the cel data.
			 */
			if (ctx->ci.pdat) {
				free (ctx->ci.pdat);
				ctx->ci.pdat = NULL;
			}
			if (e = read_chunk_PDAT (&ctx->ci.pdat, &ckhdr, cf))
				return e;

			if (e = decode_cel (ctx))
				return e;

			if (ctx->framenum)
				++ctx->framenum;

			break;

		case CHUNK_ANIM:
			/*  We're just picking up the frame rate for now.  */
			if (e = read_chunk_ANIM (&anim, &ckhdr, cf))
				return e;
			ctx->frametime = anim->frameRate * 1000 / 60;
			free (anim);
			break;

		case CHUNK_XTRA:
		default:
			if (e = skip_chunk_payload (&ckhdr, cf))
				return e;
			break;
		}
	}
	return e;
}


/***************************************************************************
 * Pixel decoding.  This is actually a big pain in the neck.
 */
inline uint8_t
convert_divide_field (uint8_t fieldval)
{
	switch (fieldval) {
	case PPMPC_SF_16:	return 16;
	case PPMPC_SF_2:	return 2;
	case PPMPC_SF_4:	return 4;
	case PPMPC_SF_8:	return 8;
	}
}

void
expand_pixel_coded (struct pxdata *pd, struct celinfo *ci, uint16_t val)
{
	uint16_t	val5;
	uint16_t	plutval;
	uint16_t	ppmp;
	uint8_t		pluta;

	pd->pel = val;

	/*  Compose 5-bit index value.  */
	val5 = val & 0x1F;
	switch (ci->bpp) {
	case 1:
		val5 |= ci->pluta & 0x1E;
		break;

	case 2:
		val5 |= ci->pluta & 0x1C;
		break;

	case 4:
		val5 |= ci->pluta & 0x10;
		break;

	default:
		break;
	}

	/*  Obtain the P value.  */
	plutval = ci->plut->PLUT[val5];
	switch (ci->bpp) {
	case 1:
	case 2:
	case 4:
		pd->p = (plutval & 0x8000) ? 1 : 0;
		break;

	case 6:
		pd->p = (val & 0x20) ? 1 : 0;
		break;

	case 8:
		pd->p = 0;
		break;

	case 16:
		pd->p = (val & 0x8000) ? 1 : 0;
		break;
	}

	ppmp = pd->p ?  ci->ccc->ccc_PPMP1 :  ci->ccc->ccc_PPMP0;

	/*  Obtain Primary Multiply Value(s).  */
	switch (FIELD2VAL (PPMPC, MS, ppmp)) {
	case PPMPC_MS_CCB:
		pd->mr = 
		pd->mg = 
		pd->mb = FIELD2VAL (PPMPC, MF, ppmp) + 1;
		break;

	case PPMPC_MS_PIN:
		switch (ci->bpp) {
		case 1:
		case 2:
		case 4:
		case 6:
			pd->mr =
			pd->mg =
			pd->mb = 1;
			break;

		case 8:
			pd->mr =
			pd->mg =
			pd->mb = (val >> 5 & 0x7) + 1;
			break;

		case 16:
			pd->mr = (val >> 11 & 0x7) + 1;
			pd->mg = (val >>  8 & 0x7) + 1;
			pd->mb = (val >>  5 & 0x7) + 1;
			break;
		}
		break;

	case PPMPC_MS_PDC:
	case PPMPC_MS_PDC_MFONLY:
		pd->mr = (plutval >> 10 & 0x7) + 1;
		pd->mg = (plutval >>  5 & 0x7) + 1;
		pd->mb = (plutval       & 0x7) + 1;
		break;
	}

	/*  Obtain Primary Divide Value(s).  */
	switch (FIELD2VAL (PPMPC, MS, ppmp)) {
	case PPMPC_MS_CCB:
	case PPMPC_MS_PIN:
	case PPMPC_MS_PDC_MFONLY:
		pd->dr =
		pd->dg =
		pd->db = convert_divide_field (FIELD2VAL (PPMPC, SF, ppmp));

	case PPMPC_MS_PDC:
		pd->dr = convert_divide_field (plutval >> 13 & 3);
		pd->dg = convert_divide_field (plutval >>  8 & 3);
		pd->db = convert_divide_field (plutval >>  3 & 3);
		break;
	}
}


/***************************************************************************
 * Pixel data storage, i.e. storing pixels to the destination buffer(s).
 */
inline void
pstore_set_curptr (
struct pstore_context	*pctx,
struct celinfo		*ci,
int			x,
int			y
)
{
	pctx->cur = pctx->dest  +  pctx->stride * y  +  x * pctx->nchan;
}

static void
write_next_pixel (struct decode_context *ctx, uint32_t val)
{
	register uint8_t *p;

	p = ctx->pstore.cur;

	if (val == ~0) {
		/*  Declared transparency output from unpacker.  */
		if (ctx->ci.indexed) {
			*p++ = 0;
			if (ctx->pstore.nchan > 1)
				*p++ = 0;
		} else {
			*p++ = 0;
			*p++ = 0;
			*p++ = 0;
			if (ctx->pstore.nchan > 3)
				*p++ = 0;
		}
	} else {
		/*  Indexed ("coded") pixels.  */
		if (ctx->ci.indexed) {
			*p++ = (uint8_t) val;
			if (ctx->pstore.nchan > 1)
				*p++ = ~0;
		} else {
			/*  Uncoded pixels.  */
			if (ctx->ci.bpp == 16) {
				*p++ = ((val & 0x7C00) >> 7) | ((val & 0x7000) >> 12);
				*p++ = ((val & 0x03E0) >> 2) | ((val & 0x0380) >> 7);
				*p++ = ((val & 0x001F) << 3) | ((val & 0x001C) >> 2);
				if (ctx->pstore.nchan > 3)
					*p++ = ~0;
			} else {
				/*  bpp == 8  */
				if (ctx->ci.rep8) {
					uint8_t w;

					w = val & 0xE0;
					*p++ = w | (w >> 3) | (w >> 6);
					w = val & 0x1C;
					*p++ = (w << 3) | w | (w >> 3);
					w = val & 0x03;
					*p++ = (w << 6) | (w << 4) | (w << 2) | w;
					if (ctx->pstore.nchan > 3)
						*p++ = ~0;
				} else {
					*p++ = val & 0xE0;
					*p++ = (val & 0x1C) << 3;
					*p++ = (val & 0x03) << 6;
					if (ctx->pstore.nchan > 3)
						*p++ = ~0;
				}
			}
		}
	}
	ctx->pstore.cur = p;
}


/***************************************************************************
 * Pixel data decoding.
 */
/*
 * A packed row of pixels has the following format:
 *	8- or 10-bit offset to next row of pixels.
 *	do {
 *		2-bit packing control.
 *		6-bit count of output pixels.
 *		pixel value(s).
 *	} while (row_not_done);
 */
static int
unpk_get_offset (int *cl_offset, struct celinfo *ci, struct chunkdata *cd)
{
	int		e;
	uint16_t	v16;
	uint8_t		v8;

	switch (ci->bpp) {
	case 1:
	case 2:
	case 4:
	case 6:
		if (!(e = get_u8 (&v8, cd)))
			*cl_offset = (int) v8;
		return e;

	case 8:
	case 16:
		if (!(e = get_u16 (&v16, cd)))
			*cl_offset = (int) v16;
		return e;

	default:
		return -EINVAL;
	}
}

static int
unpk_row (struct decode_context *ctx, struct chunkdata *cd, int *calc_width)
{
	uint32_t	packctl;
	uint32_t	pixcnt;
	uint32_t	val;
	uint32_t	max_idx;
	uint16_t	width = 0;
	int		next_row_offset;
	int		e;
	int		i;

	max_idx = cd->idx;

	if (e = unpk_get_offset (&next_row_offset, &ctx->ci, cd))
		return e;
	next_row_offset += 2;	// HW fencepost.

	max_idx += next_row_offset * sizeof (uint32_t);

	do {
		if (e = get_next_nbits (&packctl, cd, 2)) {
			if (e == EOF)
				break;
			return e;
		}

		if (packctl != PACK_EOL) {
			if (e = get_next_nbits (&pixcnt, cd, 6))
				return e;
			++pixcnt;	// HW fencepost.
			width += pixcnt;
		}

		switch (packctl) {
		case PACK_EOL:
			break;

		case PACK_LITERAL:
			for (i = pixcnt;  --i >= 0; ) {
				if (e = get_next_nbits (&val, cd, ctx->ci.bpp))
					return e;
				if (!calc_width)
					write_next_pixel (ctx, val);
			}
			break;

		case PACK_TRANSPARENT:
			for (i = pixcnt;  --i >= 0; ) {
				/*  Write zeros with alpha=0 to canvas.  */
				if (!calc_width)
					write_next_pixel (ctx, ~0);
			}
			break;

		case PACK_PACKED:
			if (e = get_next_nbits (&val, cd, ctx->ci.bpp))
				return e;
			for (i = pixcnt;  --i >= 0; ) {
				if (!calc_width)
					write_next_pixel (ctx, val);
			}
			break;
		}
	} while (packctl != PACK_EOL  &&  cd->idx < max_idx);

	if (calc_width)
		*calc_width = width;

	/*  Skip past unused bits, if any.  */
	cd->idx = max_idx;
	cd->bit_idx = 7;

	return 0;
}


static int
decode_unpacked (struct decode_context *ctx)
{
	uint32_t	val;
	int		x, y;
	int		e;

	for (y = 0;  y < ctx->ci.height;  ++y) {
		pstore_set_curptr (&ctx->pstore, &ctx->ci, 0, y);
		for (x = 0;  x < ctx->ci.width;  ++x) {
			if (e = get_next_nbits (&val, &ctx->cd, ctx->ci.bpp))
				return e;
			write_next_pixel (ctx, val);
		}
		if (e = unpk_flush_rest_of_word (&ctx->cd))
			return e;
	}
	return 0;
}

static int
decode_packed (struct decode_context *ctx)
{
	int	y;
	int	e;

	for (y = 0;  y < ctx->ci.height;  ++y) {
		pstore_set_curptr (&ctx->pstore, &ctx->ci, 0, y);
		if (e = unpk_row (ctx, &ctx->cd, NULL))
			return e;
	}
}


/*
 * This routine is called when we have collected a valid CCB, PLUT, and PDAT
 * chunk, and attached them to the celinfo.
 */
static int
decode_cel (struct decode_context *ctx)
{
	GimpDrawable		*gdraw;
	GimpPixelRgn		pixel_rgn;
	struct CCC_chunk	*ccc;
	int			e;
	gchar			*framename;

	ccc = ctx->ci.ccc;
	ctx->ci.pluta	= FIELD2VAL (CCB, PLUTA, ccc->ccc_Flags) << 1;
	ctx->ci.packed	= FIELD2VAL (CCB, PACKED, ccc->ccc_Flags);

	init_chunkdata (&ctx->cd,
	                ctx->ci.pdat->data,
	                ctx->ci.pdat->header.size - sizeof (chunk_header));

	if (FIELD2VAL (CCB, PRELOC, ccc->ccc_Flags)) {
		ctx->ci.PRE0 = ccc->ccc_PRE0;
		ctx->ci.PRE1 = ccc->ccc_PRE1;
	} else {
		/*
 		 * First two uint32s of "pixel" data are the preamble words.
		 */
		e = get_u32 (&ctx->ci.PRE0, &ctx->cd);
		if (!ctx->ci.packed)
			e |= get_u32 (&ctx->ci.PRE1, &ctx->cd);
		if (e)
			return e;
	}

	ctx->ci.rep8 = FIELD2VAL (PRE0, REP8, ctx->ci.PRE0);
	ctx->ci.indexed = !FIELD2VAL (PRE0, LINEAR, ctx->ci.PRE0);
	if (ctx->ci.indexed) {
		if (!ctx->ci.plut) {
			g_set_error (ctx->error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
				     "File corrupt (missing PLUT)");
			return -EINVAL;
		}
		switch (FIELD2VAL (PRE0, BPP, ctx->ci.PRE0)) {
		case PRE0_BPP_1:
			ctx->ci.bpp = 1; break;
		case PRE0_BPP_2:
			ctx->ci.bpp = 2; break;
		case PRE0_BPP_4:
			ctx->ci.bpp = 4; break;
		case PRE0_BPP_6:
			ctx->ci.bpp = 6; break;
		case PRE0_BPP_8:
		case PRE0_BPP_16:
			g_set_error (ctx->error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
			             "Coded 8/16 cels not yet supported.");
			return -EINVAL;

		default:
			g_set_error (ctx->error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
			             "Invalid BPP in PRE0 field.");
			return -EINVAL;
		}
	} else {
		/*  Uncoded cel; only 8bpp and 16bpp are allowed.  */
		switch (FIELD2VAL (PRE0, BPP, ctx->ci.PRE0)) {
		case PRE0_BPP_1:
		case PRE0_BPP_2:
		case PRE0_BPP_4:
		case PRE0_BPP_6:
		default:
			g_set_error (ctx->error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
				     "Invalid BPP in uncoded cel");
			return -EINVAL;

		case PRE0_BPP_8:
			ctx->ci.bpp = 8; break;
		case PRE0_BPP_16:
			ctx->ci.bpp = 16; break;
		}
	}

	/*
 	 * Determine actual dimensions from cel data.
	 */
	ctx->ci.height = FIELD2VAL (PRE0, VCNT, ctx->ci.PRE0)
	               + PRE0_VCNT_PREFETCH;

	if (ctx->framenum) {
		/*
		 * This is an ANIM.  The decoded dimensions of the first frame
		 * may not be the dimensions of the frames overall.  So we
		 * choose to believe the ccc_Width field in this case.
		 */
		ctx->ci.width = ccc->ccc_Width;
	} else if (ctx->ci.packed) {
		/*
 		 * Perform a preliminary scan of packed data to determine
		 * what its actual width is (the ccc_Width field may be
		 * lying.)
		 */
		struct chunkdata	prescan;
		int			w, calc_width;
		int			i;

		prescan = ctx->cd;

		calc_width = 0;
		for (i = ctx->ci.height;  --i >= 0; ) {
			if (e = unpk_row (ctx, &prescan, &w))
				return e;
			if (w > calc_width)
				calc_width = w;
		}
		ctx->ci.width = calc_width;
	} else {
		ctx->ci.width = FIELD2VAL (PRE1, TLHPCNT, ctx->ci.PRE1)
		              + PRE1_TLHPCNT_PREFETCH;
	}

	/*
 	 * Allocate image buffers/layers.
	 */
	if (ctx->ci.indexed) {
		ctx->pstore.base_type	= GIMP_INDEXED;
		ctx->pstore.img_type	= GIMP_INDEXEDA_IMAGE;
		ctx->pstore.nchan	= 2;
	} else {
		ctx->pstore.base_type	= GIMP_RGB;
		ctx->pstore.img_type	= GIMP_RGBA_IMAGE;
		ctx->pstore.nchan	= 4;
	}

	if (!ctx->pstore.gimage) {
		ctx->pstore.gimage = gimp_image_new (ctx->ci.width, ctx->ci.height,
		                                     ctx->pstore.base_type);
		gimp_image_set_filename (ctx->pstore.gimage, "filename");
	}

	if (ctx->framenum > 1) {
		framename = g_strdup_printf ("Frame %d", ctx->framenum);
	} else {
		framename = g_strdup_printf ("Background");
	}
	ctx->pstore.glayer = gimp_layer_new (ctx->pstore.gimage, framename,
	                                     ctx->ci.width, ctx->ci.height,
	                                     ctx->pstore.img_type, 100, GIMP_NORMAL_MODE);
	g_free (framename);

	gimp_image_insert_layer (ctx->pstore.gimage, ctx->pstore.glayer, -1, 0);

	gdraw = gimp_drawable_get (ctx->pstore.glayer);

	ctx->pstore.stride = gdraw->width * ctx->pstore.nchan;
	ctx->pstore.dest = g_malloc0 (ctx->pstore.stride * gdraw->height);


	/*
 	 * Decode pixels into buffer(s).
	 */
	if (ctx->ci.packed) {
		decode_packed (ctx);
	} else {
		if (FIELD2VAL (PRE1, LRFORM, ctx->ci.PRE1)) {
			g_set_error (ctx->error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
				     "LRFORM cels not handled yet.");
			return -EINVAL;
		}
		decode_unpacked (ctx);
	}

	/*
 	 * Copy decoded buffer to GIMP.
	 */
	gimp_progress_update (1.0);

	gimp_pixel_rgn_init (&pixel_rgn, gdraw,
	                     0, 0, gdraw->width, gdraw->height, TRUE, FALSE);
	gimp_pixel_rgn_set_rect (&pixel_rgn, ctx->pstore.dest,
	                         0, 0, gdraw->width, gdraw->height);

	if (ctx->ci.indexed) {
		/*
 		 * Convert PLUT to GIMP colormap.  If the PLUT is shorter
		 * than the length implied by the bit depth (as can happen
		 * with coded-6, -8, and -16 cels, repeat the PLUT to fill
		 * out the entire colormap.
		 */
		int		n;
		XRGB1555	*plut;
		uint8_t		gimp_cmap[256*3];

		plut = ctx->ci.plut->PLUT;
		n = 0;
		while (n < (1 << ctx->ci.bpp) * 3) {
			int		idx;
			XRGB1555	c;	// [c]olor

			for (idx = 0;  idx < ctx->ci.plut->numentries;  ++idx)
			{
				c = plut[idx];

				gimp_cmap[n++] = ((c & 0x7C00) >> 7)
				               | ((c & 0x7000) >> 12);
				gimp_cmap[n++] = ((c & 0x03E0) >> 2)
				               | ((c & 0x0380) >> 7);
				gimp_cmap[n++] = ((c & 0x001F) << 3)
				               | ((c & 0x001C) >> 2);
			}
		}

		gimp_image_set_colormap
		 (ctx->pstore.gimage, gimp_cmap, 1 << ctx->ci.bpp);
	}

	gimp_drawable_flush (gdraw);
	gimp_drawable_detach (gdraw);
	g_free (ctx->pstore.dest);

	return 0;
}

static gint32
read_cel (gchar const *name, GError **error)
{
	struct decode_context	dctx;
	struct celfile		cf;
	int			e;

	cf.error	= error;
	cf.filename	= name;

	if (!(cf.file = g_fopen (cf.filename, "rb"))) {
		g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (errno),
		             "Can't open '%s' for reading: %s",
			     gimp_filename_to_utf8 (name), g_strerror (errno));
		return -1;
	}


	memset (&dctx, 0, sizeof (dctx));
	dctx.error = error;

	if (e = parse_file (&dctx, &cf)) {
		if (e != EOF) {
			printf ("oops!\n");
		}
	}
	// dump_ccc_flags (dctx.ci.ccc);

	// e = decode_cel (&dctx);

	if (dctx.ci.ccc)	free (dctx.ci.ccc);
	if (dctx.ci.plut)	free (dctx.ci.plut);
	if (dctx.ci.pdat)	free (dctx.ci.pdat);

	if (e  &&  e != EOF)
		return -1;

	return (dctx.pstore.gimage);
}


/***************************************************************************
 * Interface routines to GIMP.  (This is essentially copy-pasted from GIMP's
 * BMP loader.)
 */
gboolean     interactive = FALSE;

static void
query (void)
{
	static const GimpParamDef load_args[] = {
		{ GIMP_PDB_INT32,    "run-mode",     "The run mode { RUN-INTERACTIVE (0), RUN-NONINTERACTIVE (1) }" },
		{ GIMP_PDB_STRING,   "filename",     "The name of the file to load" },
		{ GIMP_PDB_STRING,   "raw-filename", "The name entered" },
	};
	static const GimpParamDef load_return_vals[] = {
		{ GIMP_PDB_IMAGE, "image", "Output image" },
	};

	gimp_install_procedure (LOAD_PROC,
	                        "Loads 3DO cel format files",
				"Loads 3DO cel format files",
				"Leo L. Schwab",
				"Leo L. Schwab",
				"2012",
				"3DO cel image",
				NULL,
				GIMP_PLUGIN,
				G_N_ELEMENTS (load_args),
				G_N_ELEMENTS (load_return_vals),
				load_args, load_return_vals);

	gimp_register_file_handler_mime (LOAD_PROC, "image/x-3docel");
	gimp_register_magic_load_handler (LOAD_PROC,
	                                  "",
					  "",
					  "0,string,CCB ");
	gimp_register_magic_load_handler (LOAD_PROC,
	                                  "",
					  "",
					  "0,string,ANIM");
}

static void
run (
const gchar	*name,
gint		nparams,
const GimpParam	*param,
gint		*nreturn_vals,
GimpParam	**return_vals
)
{
	static GimpParam	values[2];
	GimpRunMode		run_mode;
	GimpPDBStatusType	status = GIMP_PDB_SUCCESS;
	gint32			image_ID;
	gint32			drawable_ID;
	GimpExportReturn	export = GIMP_EXPORT_CANCEL;
	GError			*error  = NULL;

	run_mode = param[0].data.d_int32;

//	INIT_I18N ();

	*nreturn_vals = 1;
	*return_vals  = values;
	values[0].type          = GIMP_PDB_STATUS;
	values[0].data.d_status = GIMP_PDB_EXECUTION_ERROR;

	if (!strcmp (name, LOAD_PROC)) {
		switch (run_mode) {
		case GIMP_RUN_INTERACTIVE:
			interactive = TRUE;
			break;

		case GIMP_RUN_NONINTERACTIVE:
			/*  Make sure all the arguments are there!  */
			if (nparams != 3)
				status = GIMP_PDB_CALLING_ERROR;
			break;

		default:
			break;
		}

		if (status == GIMP_PDB_SUCCESS) {
			image_ID = read_cel (param[1].data.d_string, &error);

			if (image_ID != -1) {
				*nreturn_vals = 2;
				values[1].type         = GIMP_PDB_IMAGE;
				values[1].data.d_image = image_ID;
			} else {
				status = GIMP_PDB_EXECUTION_ERROR;
			}
		}
	} else {
		status = GIMP_PDB_CALLING_ERROR;
	}

	if (status != GIMP_PDB_SUCCESS  &&  error) {
		*nreturn_vals = 2;
		values[1].type          = GIMP_PDB_STRING;
		values[1].data.d_string = error->message;
	}

	values[0].data.d_status = status;
}


#if 1

GimpPlugInInfo const PLUG_IN_INFO =
{
	NULL,  /* init_proc  */
	NULL,  /* quit_proc  */
	query, /* query_proc */
	run,   /* run_proc   */
};

MAIN ()


#else

int
main (int ac, char **av)
{
	struct decode_context	dctx;
	struct celfile		cf;
	FILE			*file;
	size_t			nread;
	int			e;

	while (++av, --ac) {
		cf.filename = *av;

		if (!(cf.file = fopen (cf.filename, "r")))
			err (1, "%s", cf.filename);

		memset (&dctx, 0, sizeof (dctx));

		if (e = parse_file (&dctx, &cf)) {
			if (e != EOF) {
				printf ("oops!\n");
				exit (1);
			}
		} else {
			dump_ccc_flags (dctx.ci.ccc);
		}

		decode_cel (&dctx);
	}
}


#endif
