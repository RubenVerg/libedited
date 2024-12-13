
BUILT_SOURCES = edited/vi.h edited/emacs.h edited/common.h edited/fcns.h edited/help.h edited/func.h

AHDR= edited/vi.h edited/emacs.h edited/common.h 
ASRC= $(srcdir)/vi.c $(srcdir)/emacs.c $(srcdir)/common.c

edited/vi.h: Makefile $(srcdir)/vi.c
	AWK=$(AWK) sh $(srcdir)/makelist -h $(srcdir)/vi.c > $@

edited/emacs.h: Makefile $(srcdir)/emacs.c
	AWK=$(AWK) sh $(srcdir)/makelist -h $(srcdir)/emacs.c > $@

edited/common.h: Makefile $(srcdir)/common.c
	AWK=$(AWK) sh $(srcdir)/makelist -h $(srcdir)/common.c > $@

edited/fcns.h: Makefile $(AHDR)
	AWK=$(AWK) sh $(srcdir)/makelist -fh $(AHDR) > $@

edited/help.h: Makefile $(ASRC)
	AWK=$(AWK) sh $(srcdir)/makelist -bh $(ASRC) > $@

edited/func.h: Makefile $(AHDR)
	AWK=$(AWK) sh $(srcdir)/makelist -fc $(AHDR) > $@

CLEANFILES = $(BUILT_SOURCES)

lib_LTLIBRARIES    = libedited.la
libedited_la_SOURCES = chared.c common.c el.c eln.c emacs.c hist.c keymacro.c map.c chartype.c parse.c \
							prompt.c read.c refresh.c search.c sig.c terminal.c tty.c vi.c \
							reallocarr.c wcsdup.c \
							tokenizer.c tokenizern.c \
							history.c historyn.c \
							filecomplete.c readline.c chared.h literal.c \
							style.c \
							edited/edited.h edited/el.h edited/hist.h edited/keymacro.h edited/map.h edited/chartype.h \
							edited/parse.h edited/prompt.h edited/read.h edited/refresh.h edited.search.h edited/sig.h \
							edited/sys.h edited/terminal.h edited/tty.h edited/vis.h edited/filecomplete.h \
							edited/readline.h edited/literal.h edited/style.h

if !HAVE_STRLCPY
libedited_la_SOURCES += strlcpy.c
endif
if !HAVE_STRLCAT
libedited_la_SOURCES += strlcat.c
endif
if !HAVE_GETLINE
libedited_la_SOURCES += getline.c
endif
if !HAVE_VIS
libedited_la_SOURCES += vis.c
endif
if !HAVE_UNVIS
libedited_la_SOURCES += unvis.c
endif

EXTRA_DIST = makelist shlib_version
nobase_include_HEADERS = edited/edited.h edited/readline.h

nodist_libedited_la_SOURCES = $(BUILT_SOURCES)

libedited_la_LDFLAGS = -no-undefined -version-info $(LT_VERSION)

