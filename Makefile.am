#
# starter-configurable-adapter - Makefile.am
#
# Copyright (C) 2026 Christian Brunello <brncrs@gmail.com>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

ACLOCAL_AMFLAGS = -I m4

SUBDIRS = include lib src plugins examples doc

EXTRA_DIST = COPYING.LESSER

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = starter-configurable-adapter.pc

.PHONY: doc
doc:
	$(MAKE) $(AM_MAKEFLAGS) -C doc pdf
