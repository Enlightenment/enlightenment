music_control_src = [
	'e_mod_main.c',
	'ui.c',
	'e_mod_main.h',
	'private.h',
]

music_control_dir = join_paths(dir_module_e, 'music-control', module_arch)
if get_option('music-control') == true
	config_h.set('USE_MODULE_MUSIC_CONTROL', '1')

	gen_music_control = custom_target('music_control_gen',
		input: 'introspect.xml',
		command: [eldbus_codegen, '@INPUT@', '-O', '@OUTDIR@'],
		output: [
			'eldbus_media_player2_player.c',
			'eldbus_mpris_media_player2.c',
			'eldbus_media_player2_player.h',
			'eldbus_mpris_media_player2.h',
			'eldbus_utils.h',
		],
	)

        module_files += join_paths(music_control_dir, 'music-control.so')
	shared_module('music-control',
                [music_control_src, gen_music_control],
                include_directories: include_directories(module_includes),
                name_prefix: '',
                dependencies: module_deps,
                install_dir: music_control_dir,
                install: true
        )
endif

