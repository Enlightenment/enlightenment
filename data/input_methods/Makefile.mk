imcfilesdir = $(datadir)/enlightenment/data/input_methods
imcfiles_DATA = \
data/input_methods/scim.imc \
data/input_methods/uim.imc \
data/input_methods/iiimf.imc \
data/input_methods/ibus.imc \
data/input_methods/gcin.imc \
data/input_methods/hime.imc \
data/input_methods/fcitx.imc

EXTRA_DIST += $(imcfiles_DATA)
