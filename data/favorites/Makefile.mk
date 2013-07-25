datafavsdir = $(datadir)/enlightenment/data/favorites
datafavs_DATA = \
data/favorites/.order \
data/favorites/desktop.desktop \
data/favorites/home.desktop \
data/favorites/root.desktop \
data/favorites/tmp.desktop

EXTRA_DIST += $(datafavs_DATA)
