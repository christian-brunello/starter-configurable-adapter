#
# libstarter-configurable-adapter - Makefile.am
#
# Copyright (C) 2026 Christian Brunello <brncrs@gmail.com>
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, see <http://www.gnu.org/licenses/>.

lib_LTLIBRARIES = libstarter-configurable-adapter.la

libstarter_configurable_adapter_la_SOURCES = \
	plugin-info.c \
	plugin.c

libstarter_configurable_adapter_la_CPPFLAGS = \
	$(AM_CPPFLAGS) $(STCA_CFLAGS) \
	-I$(top_srcdir)/include \
	-I$(top_srcdir)/include/starter/configurable-adapter

libstarter_configurable_adapter_la_LIBADD = $(STCA_LIBS)
libstarter_configurable_adapter_la_LDFLAGS = -version-info 0:0:0
