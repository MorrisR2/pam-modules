# This file is part of pam-modules.
# Copyright (C) 2005, Sergey Poznyakoff
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
# MA 02110-1301 USA 

info_TEXINFOS=pam-modules.texi
pam_modules_TEXINFOS=fdl.texi

EXTRA_DIST = gendocs_template

clean-local:
	rm -rf manual

# The rendering level is one of PUBLISH, DISTRIB or PROOF.
# Just call `make RENDITION=PROOF [target]' if you want PROOF rendition.
RENDITION = DISTRIB

MAKEINFOFLAGS=-D$(RENDITION)

GENDOCS=gendocs.sh

TEXI2DVI=texi2dvi -t '@set $(RENDITION)' -E

# Make sure you set TEXINPUTS.
# TEXINPUTS=/usr/share/texmf/pdftex/plain/misc/ is ok for most distributions
manual:
	TEXINPUTS=$(srcdir):$(top_srcdir)/build-aux:$(TEXINPUTS) \
	 MAKEINFO="$(MAKEINFO) $(MAKEINFOFLAGS)" \
	 TEXI2DVI="$(TEXI2DVI) -t @finalout" \
	 $(GENDOCS) $(PACKAGE) '$(PACKAGE_NAME) manual'
