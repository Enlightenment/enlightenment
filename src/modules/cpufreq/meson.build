src = files(
  'e_mod_main.c',
  'e_mod_config.c',
  'e_mod_main.h'
)

if get_option(m) == true
  executable('freqset',
             'freqset.c',
             c_args     : suid_cflags,
             link_args  : suid_ldflags,
             install_dir: _dir_bin,
             install    : true
            )
  suid_exes += join_paths(_dir_bin, 'freqset')
endif

