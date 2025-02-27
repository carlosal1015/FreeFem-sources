/****************************************************************************/
/* This file is part of FreeFEM.                                            */
/*                                                                          */
/* FreeFEM is free software: you can redistribute it and/or modify          */
/* it under the terms of the GNU Lesser General Public License as           */
/* published by the Free Software Foundation, either version 3 of           */
/* the License, or (at your option) any later version.                      */
/*                                                                          */
/* FreeFEM is distributed in the hope that it will be useful,               */
/* but WITHOUT ANY WARRANTY; without even the implied warranty of           */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the            */
/* GNU Lesser General Public License for more details.                      */
/*                                                                          */
/* You should have received a copy of the GNU Lesser General Public License */
/* along with FreeFEM. If not, see <http://www.gnu.org/licenses/>.          */
/****************************************************************************/
/* SUMMARY : ...                                                            */
/* LICENSE : LGPLv3                                                         */
/* ORG     : LJLL Universite Pierre et Marie Curie, Paris, FRANCE           */
/* AUTHORS : Pascal Frey                                                    */
/* E-MAIL  : pascal.frey@sorbonne-universite.fr                             */

#ifdef __cplusplus
extern "C" {
#endif

#include <math.h>
#include "medit.h"
#include "extern.h"
#include "sproto.h"

static float ident[16] = {
	1., 0., 0., 0., 0., 1., 0., 0., 0., 0., 1., 0., 0., 0., 0., 1.
};

void setPersp (pScene sc, pPersp p, int pmode) {
	float tgalpha, deltax, deltay, alphax, alphay, alpha, beta;
	float rapy, Deltax, Deltay, yy;

	/* defalut */
	Deltax = sc->par.xs;
	Deltay = sc->par.ys;
	alphax = p->fovy / 2.0f;
	alphay = p->fovy / 2.0f;

	deltax = (p->rubix + p->rubfx - Deltax) / (float)Deltax;
	deltay = (p->rubiy + p->rubfy - Deltay) / (float)Deltay;

	alpha = atan(deltax * tan(alphax * DTOR));
	beta = atan(deltay * tan(alphay * DTOR));
	yy = cos(alpha) * cos(alpha) * sin(beta) * sin(beta);
	yy /= yy + cos(beta) * cos(beta);
	yy = sqrt(yy);

	p->gamma = asin(yy) * RTOD;
	if (deltay < 0.)
		p->gamma = -p->gamma;

	p->alpha = alpha * RTOD;

	/* new fovy */
	tgalpha = tan(p->fovy * DTOR);
	rapy = fabs(p->rubfy - p->rubiy) / (float)sc->par.ys;

	if (pmode == 1)
		p->fovy = atan(tgalpha * rapy) * RTOD;
	else if (pmode == 0)
		p->fovy = atan(tgalpha / rapy) * RTOD;

	p->rubix = p->rubfx = 0;
	p->rubiy = p->rubfy = 0;
}

pPersp initPersp (pPersp p, float dmax) {
	pPersp pp;

	if (p) {
		p->fovy = 35.0f;
		p->rubber = 0;
		p->rubix = p->rubfx = 0;
		p->rubiy = p->rubfy = 0;
		p->alpha = p->gamma = 0.0f;
		p->depth = -2.0 * dmax;
		p->pmode = PERSPECTIVE;
		memcpy(p->matrix, ident, 16 * sizeof(float));
		return (p);
	} else {
		pp = (pPersp)M_calloc(1, sizeof(struct sperspective), "persp");
		assert(pp);

		pp->fovy = 35.0f;
		pp->rubber = 0;
		pp->rubix = pp->rubfx = 0;
		pp->rubiy = pp->rubfy = 0;
		pp->alpha = pp->gamma = 0.0f;
		pp->depth = -2.0 * dmax;
		pp->pmode = PERSPECTIVE;
		memcpy(pp->matrix, ident, 16 * sizeof(float));
		return (pp);
	}
}

#ifdef __cplusplus
}
#endif
