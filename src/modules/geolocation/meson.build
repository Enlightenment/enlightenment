geolocation_dist = [
	'e-module-geolocation.edj',
	'module.desktop',
]

geolocation_src = [
	'e_mod_main.c'
]

geolocation_dir = join_paths(dir_module_e, 'geolocation', module_arch)
if get_option('geolocation') == true
	config_h.set('USE_MODULE_GEOLOCATION', '1')

	install_data(geolocation_dist,
		install_dir: join_paths(dir_module_e, 'geolocation')
	)

	gen_geo = custom_target('geolocation_gen',
		input: 'org.freedesktop.GeoClue2.xml',
		command: [eldbus_codegen, '@INPUT@', '-O', '@OUTDIR@'],
		output: [
			'eldbus_geo_clue2_client.c',
			'eldbus_geo_clue2_client.h',
			'eldbus_geo_clue2_location.c',
			'eldbus_geo_clue2_location.h',
			'eldbus_geo_clue2_manager.c',
			'eldbus_geo_clue2_manager.h',
			'eldbus_utils.h'
		]
	)

        module_files += join_paths(geolocation_dir, 'geolocation.so')
	shared_module('geolocation',
                [geolocation_src, gen_geo],
                include_directories: include_directories(module_includes),
                name_prefix: '',
                dependencies: module_deps,
                install_dir: geolocation_dir,
                install: true
        )
endif

