MAINTAINERCLEANFILES += doc/e.dox

PHONIES += doc doc-clean

PACKAGE_DOCNAME = $(PACKAGE_TARNAME)-$(PACKAGE_VERSION)-doc

if EFL_BUILD_DOC

doc-clean:
	rm -rf doc/html/ doc/latex/ doc/man/ doc/xml/ $(top_builddir)/$(PACKAGE_DOCNAME).tar*

doc: all doc-clean
	@echo "entering doc/"
	$(efl_doxygen) doc/Doxyfile
	cp $(top_srcdir)/doc/img/* doc/html/
	rm -rf $(PACKAGE_DOCNAME).tar*
	$(MKDIR_P) $(PACKAGE_DOCNAME)/doc
	cp -R doc/html/ doc/latex/ doc/man/ $(PACKAGE_DOCNAME)/doc
	tar cf $(PACKAGE_DOCNAME).tar $(PACKAGE_DOCNAME)/
	bzip2 -9 $(PACKAGE_DOCNAME).tar
	rm -rf $(PACKAGE_DOCNAME)/
	@echo "Documentation Package: $(top_builddir)/$(PACKAGE_DOCNAME).tar.bz2"

else

doc:
	@echo "Documentation not built. Run ./configure --help"

endif

docfilesdir = $(datadir)/enlightenment/doc
docfiles_DATA = \
doc/documentation.html \
doc/illume2.html \
doc/FDO.txt \
doc/cache.txt \
doc/enlightenment.png \
doc/illume2.png

EXTRA_DIST += \
$(docfiles_DATA) \
doc/Doxyfile.in \
$(wildcard doc/img/*.*) \
doc/e.css \
doc/head.html \
doc/foot.html \
doc/e.dox.in
