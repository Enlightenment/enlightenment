teamwork_src = [
	'e_mod_config.c',
	'e_mod_main.c',
	'e_mod_tw.c',
	'e_mod_main.h',
]

if config_h.has('HAVE_WAYLAND') == true
	teamwork_src += [
		'wl.c',
		'wl_teamwork.c',
		'wl_teamwork.h'
	]
endif
if config_h.has('HAVE_WAYLAND_ONLY') == false
	teamwork_src += [
		'x11.c'
	]
endif

module += [teamwork_src, 'e-module-teamwork.edj']
