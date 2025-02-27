############################################################################
# This file is part of FreeFEM.                                            #
#                                                                          #
# FreeFEM is free software: you can redistribute it and/or modify          #
# it under the terms of the GNU Lesser General Public License as           #
# published by the Free Software Foundation, either version 3 of           #
# the License, or (at your option) any later version.                      #
#                                                                          #
# FreeFEM is distributed in the hope that it will be useful,               #
# but WITHOUT ANY WARRANTY; without even the implied warranty of           #
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the            #
# GNU Lesser General Public License for more details.                      #
#                                                                          #
# You should have received a copy of the GNU Lesser General Public License #
# along with FreeFEM. If not, see <http://www.gnu.org/licenses/>.          #
############################################################################
# SUMMARY : Makefile for FreeFEM
# LICENSE : LGPLv3
# ORG     : LJLL Universite Pierre et Marie Curie, Paris, FRANCE
# AUTHORS : ...
# E-MAIL  : ...

LOG_DRIVER = $(SHELL) $(top_srcdir)/bin/test-driver-ff
TESTS_ENVIRONMENT = TEST_FFPP=$(TEST_FFPP) FLAGS_FFPP=-nw

all-local: all.edp regtests.edp freefem++.pref

if METIS
CONDITIONAL_METIS = schwarz-nm-3d.edp
endif

if TETGEN
CONDITIONAL_TETGEN = cylinder.edp \
	Period-Poisson-cube-ballon.edp \
	Poisson3d.edp \
	Poisson-cube-ballon.edp \
	refinesphere.edp \
	sphereincube.edp \
	tetgencube.edp \
	tetgenholeregion.edp

if FREEYAMS
CONDITIONAL_TETGEN_FREEYAMS = Sphere-Isocahedron.edp

if MMG3D
if MSHMET
CONDITIONAL_TETGEN_FREEYAMS_MMG3D_MSHMET = Laplace-Adapt-aniso-3d.edp
endif MSHMET
endif MMG3D
endif FREEYAMS

if GSL
CONDITIONAL_TETGEN_GSL = bottle.edp
endif GSL

if MMG3D
CONDITIONAL_TETGEN_MMG3D = fallingspheres.edp
endif MMG3D

if MSHMET
CONDITIONAL_TETGEN_MSHMET = Laplace-Adapt-3d.edp
endif
endif TETGEN

TESTS = 3d-Leman.edp \
	ArrayFE-3d.edp \
	beam-3d.edp \
	cone.edp \
	convect-3d.edp \
	cube-period.edp \
	cylinder-3d.edp \
	EqPoisson.edp \
	extract-boundary3d.edp \
	first.edp \
	intlevelset3d.edp \
	Lac.edp \
	Laplace3d.edp \
	LaplaceRT-3d.edp \
	meditddm.edp \
	NSI3d-carac.edp \
	NSI3d.edp \
	p.edp \
	periodic-3d.edp \
	Poisson.edp \
	pyramide.edp \
	sphere2.edp \
	sphere6.edp \
	Stokes.edp \
	TruncLac.edp \
	$(CONDITIONAL_METIS) \
	$(CONDITIONAL_TETGEN) \
	$(CONDITIONAL_TETGEN_FREEYAMS) \
	$(CONDITIONAL_TETGEN_FREEYAMS_MMG3D_MSHMET) \
	$(CONDITIONAL_TETGEN_GSL)  \
	$(CONDITIONAL_TETGEN_MMG3D) \
	$(CONDITIONAL_TETGEN_MSHMET)

XFAIL_TESTS =

EXTRA_DIST = *.edp \
	all.edp \
	regtests.edp \
	regtests.m4 \
	ref.edp \
	dodecaedre01.mesh \
	lac-leman-v4.msh

all.edp: Makefile
	(echo "NoGraphicWindow=true;NoUseOfWait=true;int verbosityy=verbosity;int MEM1234=storageused();"; \
	for i in *`ls *.edp|grep -v -E '^(all|regtests|makeref|ref)\.edp$$'` ; do \
		echo ' cout << "--------- file : '$$i' --------------------------------------------------------" << endl;' ;\
		echo "verbosity=verbosityy;searchMethod=0;" ; \
		echo \{ include \"$$i\"\;\}\; ;\
		echo ' cout << " mem leak = " <<storageused() - MEM1234 << endl << "------------------------------------------------------------------------------ " << endl;' ;\
	done) > $@

# To create a new set of reference values in "ref.edp"
Ref: makeref.edp freefem++.pref
	export FF_LOADPATH=../../plugin/seq/.;../../src/nw/FreeFem++-nw makeref.edp

makeref.edp: regtests.m4 ../../etc/config/m4/regtests.m4
	m4 regtests.m4 > makeref.edp

freefem++.pref:Makefile
	echo includepath = \"../../idp/\" > freefem++.pref
	echo loadpath = \"../../plugin/seq/\" >> freefem++.pref

3d-Leman.edp: freefem++.pref

# To check the scripts against their reference values
regtests.edp: regtests.m4 ../../etc/config/m4/regtests.m4
	m4 -DASSERT regtests.m4 > regtests.edp

FORCE:
