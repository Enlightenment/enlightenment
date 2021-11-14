cmd = [ edje_cmd,
        '-id', join_paths(meson.current_source_dir(), 'images'),
        '@INPUT@', '@OUTPUT@'
      ]
src = 'enlightenment_fprint'
custom_target(src + '.edj',
              input      : src + '.edc',
              output     : src + '.edj',
              command    : cmd,
              install_dir: join_paths(dir_data, proj, 'themes'),
              install    : true
             )
