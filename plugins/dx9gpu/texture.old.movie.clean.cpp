/***************************************************************************
texture_movie.cpp  -  description
-------------------
begin                : Sun Mar 08 2009
copyright            : (C) 1999-2009 by Pete Bernert
web                  : www.pbernert.com   
***************************************************************************/

/***************************************************************************
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version. See also the license.txt file for *
*   additional informations.                                              *
*                                                                         *
***************************************************************************/

#include "stdafx.h"
#include "externals.h"
#include "backend.h"
#include "texture.h"
#include "gpu.h"
#include "prim.h"

#define MRED(x)   ((x>>3) & 0x1f)
#define MGREEN(x) ((x>>6) & 0x3e0)
#define MBLUE(x)  ((x>>9) & 0x7c00)

#define XMGREEN(x) ((x>>5)  & 0x07c0)
#define XMRED(x)   ((x<<8)  & 0xf800)
#define XMBLUE(x)  ((x>>18) & 0x003e)

#define MOVIE_TEXTURE_SIZE 1024

static uint8_t *	pTextureData = NULL;

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
static bk_texture_t movie_texture = NULL;
////////////////////////////////////////////////////////////////////////
// movie texture: define
////////////////////////////////////////////////////////////////////////
static void CreateMovieTexture() {
	// Create a big texture (1024 x 1024)
	if(movie_texture == NULL)
	{
		movie_texture = BK_CreateTexture(MOVIE_TEXTURE_SIZE, MOVIE_TEXTURE_SIZE);
		pTextureData = (uint8_t*) malloc(MOVIE_TEXTURE_SIZE * MOVIE_TEXTURE_SIZE * 4);
				
		memset(pTextureData, 0, MOVIE_TEXTURE_SIZE * MOVIE_TEXTURE_SIZE * 4);

		BK_SetTextureData(movie_texture, MOVIE_TEXTURE_SIZE, MOVIE_TEXTURE_SIZE, pTextureData);
	}
}

static uint8_t * GetMovieTextureData() {
	// if texture doesn't existe create it
	CreateMovieTexture();
	// return temp buffer
	return pTextureData;
}

static void SetMovieTextureData() {
	BK_SetSubTextureData(
		movie_texture, 0, 0,
		MOVIE_TEXTURE_SIZE, 
        MOVIE_TEXTURE_SIZE, pTextureData
	);
}

////////////////////////////////////////////////////////////////////////
// movie texture: load
////////////////////////////////////////////////////////////////////////

bk_texture_t LoadTextureMovie(void)
{
	short row, column, dx;
	unsigned int startxy;
	BOOL b_X,b_Y;

	b_X=FALSE;b_Y=FALSE;

	if((xrMovieArea.x1-xrMovieArea.x0)<255)  b_X=TRUE;
	if((xrMovieArea.y1-xrMovieArea.y0)<255)  b_Y=TRUE;

	uint32_t * dst = (uint32_t *)GetMovieTextureData();	
	uint32_t * ta = dst;

	if(PSXDisplay.RGB24)
	{
		unsigned char * pD;

		if(b_X)
		{
			for(column=xrMovieArea.y0;column<xrMovieArea.y1;column++)
			{
				ta = (uint32_t *)&dst[MOVIE_TEXTURE_SIZE * column + xrMovieArea.x0];
				startxy=((1024)*column)+xrMovieArea.x0;
				pD=(unsigned char *)&psxVuw[startxy];
				for(row=xrMovieArea.x0;row<xrMovieArea.x1;row++)
				{
					*ta++=*((uint32_t *)pD)|0xff000000;
					pD+=3;
				}
				*ta++=*(ta-1);
			}
			if(b_Y)
			{
				dx=xrMovieArea.x1-xrMovieArea.x0+1;
				for(row=xrMovieArea.x0;row<xrMovieArea.x1;row++)
					*ta++=*(ta-dx);
				*ta++=*(ta-1);
			}
		}
		else
		{
			for(column=xrMovieArea.y0;column<xrMovieArea.y1;column++)
			{
				ta = (uint32_t *)&dst[MOVIE_TEXTURE_SIZE * column + xrMovieArea.x0];
				startxy=((1024)*column)+xrMovieArea.x0;
				pD=(unsigned char *)&psxVuw[startxy];
				for(row=xrMovieArea.x0;row<xrMovieArea.x1;row++)
				{
					*ta++=*((uint32_t *)pD)|0xff000000;
					pD+=3;
				}
			}
			if(b_Y)
			{
				dx=xrMovieArea.x1-xrMovieArea.x0;
				for(row=xrMovieArea.x0;row<xrMovieArea.x1;row++)
					*ta++=*(ta-dx);
			}
		}
	}
	else
	{
		uint32_t (*LTCOL)(uint32_t);

		LTCOL=XP8RGBA_0;

		ubOpaqueDraw=0;

		if(b_X)
		{
			for(column=xrMovieArea.y0;column<xrMovieArea.y1;column++)
			{
				ta = (uint32_t *)&dst[MOVIE_TEXTURE_SIZE * column + xrMovieArea.x0];
				startxy=((1024)*column)+xrMovieArea.x0;
				for(row=xrMovieArea.x0;row<xrMovieArea.x1;row++)
					*ta++=LTCOL(psxVuw[startxy++]|0x8000);
				*ta++=*(ta-1);
			}

			if(b_Y)
			{
				dx=xrMovieArea.x1-xrMovieArea.x0+1;
				for(row=xrMovieArea.x0;row<xrMovieArea.x1;row++)
					*ta++=*(ta-dx);
				*ta++=*(ta-1);
			}
		}
		else
		{
			for(column=xrMovieArea.y0;column<xrMovieArea.y1;column++)
			{
				ta = (uint32_t *)&dst[MOVIE_TEXTURE_SIZE * column + xrMovieArea.x0];
				startxy=((1024)*column)+xrMovieArea.x0;
				for(row=xrMovieArea.x0;row<xrMovieArea.x1;row++)
					*ta++=LTCOL(psxVuw[startxy++]|0x8000);
			}

			if(b_Y)
			{
				dx=xrMovieArea.x1-xrMovieArea.x0;
				for(row=xrMovieArea.x0;row<xrMovieArea.x1;row++)
					*ta++=*(ta-dx);
			}
		}
	}

	SetMovieTextureData();
   
	return movie_texture;
}
