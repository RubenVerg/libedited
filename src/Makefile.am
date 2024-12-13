
BUILT_SOURCES = vi.h emacs.h common.h fcns.h help.h func.h

AHDR= vi.h emacs.h common.h 
ASRC= $(srcdir)/vi.c $(srcdir)/emacs.c $(srcdir)/common.c

vi.h: Makefile $(srcdir)/vi.c
	AWK=$(AWK) sh $(srcdir)/makelist -h $(srcdir)/vi.c > $@

emacs.h: Makefile $(srcdir)/emacs.c
	AWK=$(AWK) sh $(srcdir)/makelist -h $(srcdir)/emacs.c > $@

common.h: Makefile $(srcdir)/common.c
	AWK=$(AWK) sh $(srcdir)/makelist -h $(srcdir)/common.c > $@

fcns.h: Makefile $(AHDR)
	AWK=$(AWK) sh $(srcdir)/makelist -fh $(AHDR) > $@

help.h: Makefile $(ASRC)
	AWK=$(AWK) sh $(srcdir)/makelist -bh $(ASRC) > $@

func.h: Makefile $(AHDR)
	AWK=$(AWK) sh $(srcdir)/makelist -fc $(AHDR) > $@

CLEANFILES = $(BUILT_SOURCES)

lib_LTLIBRARIES    = libedited.la
libedited_la_SOURCES = chared.c common.c el.c eln.c emacs.c hist.c keymacro.c map.c chartype.c parse.c \
							prompt.c read.c refresh.c search.c sig.c terminal.c tty.c vi.c \
							reallocarr.c wcsdup.c \
							tokenizer.c tokenizern.c \
							history.c historyn.c \
							filecomplete.c readline.c chared.h literal.c \
							edited/edited.h edited/el.h edited/hist.h edited/keymacro.h edited/map.h edited/chartype.h \
							edited/parse.h edited/prompt.h edited/read.h edited/refresh.h edited.search.h edited/sig.h \
							edited/sys.h edited/terminal.h edited/tty.h edited/vis.h edited/filecomplete.h \
							edited/readline.h edited/literal.h

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

