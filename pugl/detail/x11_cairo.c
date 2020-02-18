/*
  Copyright 2012-2019 David Robillard <http://drobilla.net>

  Permission to use, copy, modify, and/or distribute this software for any
  purpose with or without fee is hereby granted, provided that the above
  copyright notice and this permission notice appear in all copies.

  THIS SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

/**
   @file x11_cairo.c Cairo graphics backend for X11.
*/

#include "pugl/detail/types.h"
#include "pugl/detail/x11.h"
#include "pugl/pugl.h"
#include "pugl/pugl_cairo.h"
#include "pugl/pugl_stub.h"

#include <X11/Xutil.h>
#include <cairo-xlib.h>
#include <cairo.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
	cairo_surface_t* back;
	cairo_surface_t* front;
	cairo_t*         cr;
} PuglX11CairoSurface;

static void
puglX11CairoClose(PuglView* view)
{
	PuglInternals* const       impl    = view->impl;
	PuglX11CairoSurface* const surface = (PuglX11CairoSurface*)impl->surface;

	cairo_surface_destroy(surface->front);
	cairo_surface_destroy(surface->back);
	surface->front = surface->back = NULL;
}

static PuglStatus
puglX11CairoOpen(PuglView* view)
{
	PuglInternals* const       impl    = view->impl;
	PuglX11CairoSurface* const surface = (PuglX11CairoSurface*)impl->surface;

	surface->back = cairo_xlib_surface_create(impl->display,
	                                          impl->win,
	                                          impl->vi->visual,
	                                          (int)view->frame.width,
	                                          (int)view->frame.height);

	surface->front = cairo_surface_create_similar(
	        surface->back,
	        cairo_surface_get_content(surface->back),
	        (int)view->frame.width,
	        (int)view->frame.height);

	if (cairo_surface_status(surface->back) ||
	    cairo_surface_status(surface->front)) {
		puglX11CairoClose(view);
		return PUGL_CREATE_CONTEXT_FAILED;
	}

	return PUGL_SUCCESS;
}

static PuglStatus
puglX11CairoCreate(PuglView* view)
{
	PuglInternals* const impl = view->impl;

	impl->surface = (cairo_surface_t*)calloc(1, sizeof(PuglX11CairoSurface));

	return PUGL_SUCCESS;
}

static PuglStatus
puglX11CairoDestroy(PuglView* view)
{
	PuglInternals* const       impl    = view->impl;
	PuglX11CairoSurface* const surface = (PuglX11CairoSurface*)impl->surface;

	puglX11CairoClose(view);
	free(surface);

	return PUGL_SUCCESS;
}

static PuglStatus
puglX11CairoEnter(PuglView* view, const PuglEventExpose* expose)
{
	PuglInternals* const       impl    = view->impl;
	PuglX11CairoSurface* const surface = (PuglX11CairoSurface*)impl->surface;
	PuglStatus                 st      = PUGL_SUCCESS;

	if (expose && !(st = puglX11CairoOpen(view))) {
		surface->cr = cairo_create(surface->front);

		if (cairo_status(surface->cr)) {
			st = PUGL_CREATE_CONTEXT_FAILED;
		}
	}

	return st;
}

static PuglStatus
puglX11CairoLeave(PuglView* view, const PuglEventExpose* expose)
{
	PuglInternals* const       impl    = view->impl;
	PuglX11CairoSurface* const surface = (PuglX11CairoSurface*)impl->surface;

	if (expose) {
		// Destroy front context and create a new one for drawing to the back
		cairo_destroy(surface->cr);
		surface->cr = cairo_create(surface->back);

		// Clip to expose region
		cairo_rectangle(surface->cr,
		                expose->x,
		                expose->y,
		                expose->width,
		                expose->height);
		cairo_clip(surface->cr);

		// Paint front onto back
		cairo_set_source_surface(surface->cr, surface->front, 0, 0);
		cairo_set_operator(surface->cr, CAIRO_OPERATOR_SOURCE);
		cairo_paint(surface->cr);

		// Flush to X and close everything
		cairo_destroy(surface->cr);
		cairo_surface_flush(surface->back);
		puglX11CairoClose(view);
		surface->cr = NULL;
	}

	return PUGL_SUCCESS;
}

static void*
puglX11CairoGetContext(PuglView* view)
{
	PuglInternals* const       impl    = view->impl;
	PuglX11CairoSurface* const surface = (PuglX11CairoSurface*)impl->surface;

	return surface->cr;
}

const PuglBackend*
puglCairoBackend(void)
{
	static const PuglBackend backend = {
		puglX11StubConfigure,
		puglX11CairoCreate,
		puglX11CairoDestroy,
		puglX11CairoEnter,
		puglX11CairoLeave,
		puglStubResize,
		puglX11CairoGetContext
	};

	return &backend;
}
