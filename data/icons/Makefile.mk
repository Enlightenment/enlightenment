iconsfilesdir = $(datadir)/enlightenment/data/icons
iconsfiles_DATA = \
data/icons/xterm.png \
data/icons/web_browser.png \
data/icons/audio_player.png \
data/icons/mail_client.png \
data/icons/video_player.png \
data/icons/text_editor.png \
data/icons/image_viewer.png \
data/icons/audio_player2.png

EXTRA_DIST += $(iconsfiles_DATA)

pixmapsdir = $(datadir)/pixmaps
pixmaps_DATA = \
data/icons/enlightenment-askpass.png

EXTRA_DIST += $(pixmaps_DATA)
