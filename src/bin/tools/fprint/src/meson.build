inc = include_directories('../../../../..')
deps = [
    dep_elementary
]

xml_codegen_result = []

foreach file : ['device', 'manager']
  xml_codegen_result += custom_target('dbus_codegen_'+file,
    input: file+'.xml',
    output: ['eldbus_fprint_' + file + '.h', 'eldbus_fprint_' + file + '.c'],
    command: [eldbus_codegen, '-O', meson.current_build_dir(), '@INPUT@'],
  )
endforeach


src = [
  'enlightenment_fprint.c',
]

executable('enlightenment_fprint',
  src + xml_codegen_result,
  include_directories: inc,
  dependencies       : deps,
  gui_app            : true,
  install_dir        : join_paths(dir_bin),
  install            : true)
