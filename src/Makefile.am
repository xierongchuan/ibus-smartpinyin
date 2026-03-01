# vim:set noet ts=4:
#
# ibus-smartpinyin - Intelligent Pinyin engine based on libpinyin for IBus
#
# Copyright (c) 2008-2010 Peng Huang <shawn.p.huang@gmail.com>
#
## This program is free software: you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation, either version 3 of the License, or
## (at your option) any later version.
##
## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with this program.  If not, see <http://www.gnu.org/licenses/>.


libexec_PROGRAMS = ibus-engine-smartpinyin
ibus_engine_smartpinyin_built_c_sources = \
	$(NULL)
ibus_engine_smartpinyin_built_h_sources = \
	PYPunctTable.h \
	PYSimpTradConverterTable.h \
	$(NULL)
ibus_engine_smartpinyin_c_sources = \
	PYConfig.cc \
	PYEditor.cc \
	PYEngine.cc \
	PYFallbackEditor.cc \
	PYHalfFullConverter.cc \
	PYMain.cc \
	PYPinyinProperties.cc \
	PYPunctEditor.cc \
	PYSimpTradConverter.cc \
	$(NULL)
ibus_engine_smartpinyin_h_sources = \
	PYBus.h \
	PYConfig.h \
	PYEditor.h \
	PYEngine.h \
	PYExtEditor.h \
	PYFallbackEditor.h \
	PYHalfFullConverter.h \
	PYLookupTable.h \
	PYObject.h \
	PYPinyinProperties.h \
	PYPointer.h \
	PYProperty.h \
	PYPunctEditor.h \
	PYRawEditor.h \
	PYSignal.h \
	PYSimpTradConverter.h \
	PYString.h \
	PYText.h \
	PYTypes.h \
	PYUtil.h \
	PYTableDatabase.h \
	PYTableEditor.h \
	PYEnglishDatabase.h \
	PYEnglishEditor.h \
	PYPEnglishCandidates.h \
	PYLibPinyin.h \
	PYPPhoneticEditor.h \
	PYPPinyinEditor.h \
	PYPFullPinyinEditor.h \
	PYPDoublePinyinEditor.h \
	PYPBopomofoEditor.h \
	PYPPinyinEngine.h \
	PYPBopomofoEngine.h \
	PYPConfig.h \
	PYPEnhancedCandidates.h \
	PYPLibPinyinCandidates.h \
	PYPTradCandidates.h \
	PYPLuaTriggerCandidates.h \
	PYPLuaConverterCandidates.h \
	PYPSuggestionEditor.h \
	PYPSuggestionCandidates.h \
	PYPEmojiTable.h \
	PYPEmojiCandidates.h \
	PYUserPhraseDatabase.h \
	PYPUserPhraseCandidates.h \
	PYXMLUtil.h \
	$(NULL)

ibus_engine_smartpinyin_c_sources += \
	PYPConfig.cc \
	PYLibPinyin.cc \
	PYPPhoneticEditor.cc \
	PYPPinyinEditor.cc \
	PYPFullPinyinEditor.cc \
	PYPDoublePinyinEditor.cc \
	PYPBopomofoEditor.cc \
	PYPPinyinEngine.cc \
	PYPBopomofoEngine.cc \
	PYPLibPinyinCandidates.cc \
	PYPTradCandidates.cc \
	PYPSuggestionEditor.cc \
	PYPSuggestionCandidates.cc \
	PYPEmojiCandidates.cc \
	PYUserPhraseDatabase.cc \
	PYPUserPhraseCandidates.cc \
	PYXMLUtil.cc \
	$(NULL)


if IBUS_BUILD_LUA_EXTENSION
ibus_engine_smartpinyin_c_sources += \
	PYExtEditor.cc \
	PYPLuaTriggerCandidates.cc \
	PYPLuaConverterCandidates.cc \
	$(NULL)
endif

if ENABLE_CLOUD_INPUT_MODE
ibus_engine_smartpinyin_h_sources += \
	PYPCloudCandidates.h \
	$(NULL)
ibus_engine_smartpinyin_c_sources += \
	PYPCloudCandidates.cc \
	$(NULL)
endif

if IBUS_BUILD_TABLE_INPUT_MODE
ibus_engine_smartpinyin_c_sources += \
	PYTableDatabase.cc \
	PYTableEditor.cc \
	$(NULL)
endif

if IBUS_BUILD_ENGLISH_INPUT_MODE
ibus_engine_smartpinyin_c_sources += \
	PYEnglishDatabase.cc \
	PYEnglishEditor.cc \
	PYPEnglishCandidates.cc \
	$(NULL)
endif

ibus_engine_smartpinyin_SOURCES = \
	$(ibus_engine_smartpinyin_c_sources) \
	$(ibus_engine_smartpinyin_h_sources) \
	$(ibus_engine_smartpinyin_built_c_sources) \
	$(ibus_engine_smartpinyin_built_h_sources) \
	$(NULL)


ibus_engine_smartpinyin_CXXFLAGS = \
	@IBUS_CFLAGS@ \
	@SQLITE_CFLAGS@ \
	@LIBPINYIN_CFLAGS@ \
	@OPENCC_CFLAGS@ \
	-DGETTEXT_PACKAGE=\"@GETTEXT_PACKAGE@\" \
	-DLOCALEDIR=\"$(localedir)\" \
	-DPKGDATADIR=\"$(pkgdatadir)\" \
	-DLIBEXECDIR=\"$(libexecdir)\" \
	$(NULL)

if HAVE_BOOST
ibus_engine_smartpinyin_CXXFLAGS += \
	@BOOST_CPPFLAGS@ \
	$(NULL)
else
ibus_engine_smartpinyin_CXXFLAGS += \
	-std=c++0x \
	$(NULL)
endif

ibus_engine_smartpinyin_LDADD = \
	@IBUS_LIBS@ \
	@SQLITE_LIBS@ \
	@LIBPINYIN_LIBS@ \
	@OPENCC_LIBS@ \
	$(NULL)

ibus_engine_smartpinyin_CXXFLAGS += \
	-DLIBPINYIN_DATADIR=\"@LIBPINYIN_DATADIR@\/data\" \
	$(NULL)

if IBUS_BUILD_LUA_EXTENSION
ibus_engine_smartpinyin_CXXFLAGS +=  \
	@LUA_CFLAGS@ \
	-DIBUS_BUILD_LUA_EXTENSION \
	-I$(top_srcdir)/lua/ \
	$(NULL)
endif

if IBUS_BUILD_LUA_EXTENSION
    ibus_engine_smartpinyin_LDADD += \
	@LUA_LIBS@ \
	-L../lua/ \
	-lpylua \
	$(NULL)
endif

if ENABLE_CLOUD_INPUT_MODE
ibus_engine_smartpinyin_CXXFLAGS += \
	@LIBSOUP_CFLAGS@ \
	@JSONGLIB_CFLAGS@ \
	-DENABLE_CLOUD_INPUT_MODE \
	$(NULL)
endif

if ENABLE_CLOUD_INPUT_MODE
ibus_engine_smartpinyin_LDADD += \
	@LIBSOUP_LIBS@ \
	@JSONGLIB_LIBS@ \
	$(NULL)
endif

if IBUS_BUILD_ENGLISH_INPUT_MODE
ibus_engine_smartpinyin_CXXFLAGS += \
	-DIBUS_BUILD_ENGLISH_INPUT_MODE \
	$(NULL)
endif

if IBUS_BUILD_TABLE_INPUT_MODE
ibus_engine_smartpinyin_CXXFLAGS += \
	-DIBUS_BUILD_TABLE_INPUT_MODE \
	$(NULL)
endif

if ENABLE_LIBNOTIFY
ibus_engine_smartpinyin_CXXFLAGS += \
	@LIBNOTIFY_CFLAGS@ \
	-DENABLE_LIBNOTIFY \
	$(NULL)

ibus_engine_smartpinyin_LDADD += \
	@LIBNOTIFY_LIBS@ \
	$(NULL)
endif

BUILT_SOURCES = \
	$(ibus_engine_built_c_sources) \
	$(ibus_engine_built_h_sources) \
	$(NULL)

component_DATA = \
	smartpinyin.xml \
	$(NULL)
componentdir = @datadir@/ibus/component

engine_DATA = \
	default.xml \
	$(NULL)
enginedir = ${pkgdatadir}

EXTRA_DIST = \
	smartpinyin.inputmethod.xml.in \
	default.inputmethod.xml.in \
	$(NULL)

CLEANFILES = \
	smartpinyin.xml \
	default.xml \
	$(NULL)

PYPunctTable.h:
	$(AM_V_GEN) \
	$(PYTHON) $(top_srcdir)/scripts/genpuncttable.py > $@ || \
		( $(RM) $@; exit 1 )

PYSimpTradConverterTable.h:
	$(AM_V_GEN) \
	$(PYTHON) $(top_srcdir)/scripts/update-simptrad-table.py > $@ || \
		( $(RM) $@; exit 1 )

update-simptrad-table:
	$(RM) $(top_srcdir)/scripts/ZhConversion.* PYSimpTradConverterTable.h
	$(MAKE) -C $(top_srcdir)/scripts ZhConversion.py
	$(MAKE) PYSimpTradConverterTable.h

%.xml: %.inputmethod.xml.in
	sed -e "s|\@LIBEXECDIR\@|$(libexecdir)|g" \
	    -e "s|\@PKGDATADIR\@|$(pkgdatadir)|g" \
	$< > $@

test: ibus-engine-smartpinyin
	$(ENV) \
		G_DEBUG=fatal_criticals \
		$(builddir)/ibus-engine-smartpinyin
