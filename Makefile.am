bin_PROGRAMS = esmtp

dist_man_MANS = esmtp.1

esmtp_SOURCES = esmtp.h main.c parser.y lexer.l

EXTRA_DIST = README.mutt sample.esmtprc

AM_YFLAGS = -d

install-exec-hook:
	$(mkinstalldirs) $(DESTDIR)$(sbindir) $(DESTDIR)$(libdir)
	$(LN_S) $(bindir)/esmtp $(DESTDIR)$(sbindir)/sendmail
	$(LN_S) $(bindir)/esmtp $(DESTDIR)$(libdir)/sendmail
	$(LN_S) $(bindir)/esmtp $(DESTDIR)$(bindir)/mailq
	$(LN_S) $(bindir)/esmtp $(DESTDIR)$(bindir)/newaliases

install-data-hook:
	$(LN_S) esmtp.1 $(DESTDIR)$(man1dir)/sendmail.1
	$(LN_S) esmtp.1 $(DESTDIR)$(man1dir)/mailq.1
	$(LN_S) esmtp.1 $(DESTDIR)$(man1dir)/newaliases.1
