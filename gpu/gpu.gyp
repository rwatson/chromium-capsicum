# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
    # This is defined here because we need to compile this set of files
    # twice with different defines. Once so it calls real GL, again so it
    # calls mock GL for the unit tests.
    'gpu_source_files': [
      'command_buffer/service/gles2_cmd_decoder.h',
      'command_buffer/service/gles2_cmd_decoder_autogen.h',
      'command_buffer/service/gles2_cmd_decoder.cc',
      'command_buffer/service/gles2_cmd_validation.h',
      'command_buffer/service/gles2_cmd_validation.cc',
      'command_buffer/service/gles2_cmd_validation_autogen.h',
      'command_buffer/service/gles2_cmd_validation_implementation_autogen.h',
      'command_buffer/service/gl_utils.h',
    ],
  },
  'includes': [
    '../build/common.gypi',
  ],
  'targets': [
    {
      'target_name': 'gl_libs',
      'type': 'static_library',
        'include_dirs': [
          '../third_party/glew/include',
        ],
        'defines': [
          'GLEW_STATIC',
        ],
      'all_dependent_settings': {
        'include_dirs': [
          '../third_party/glew/include',
        ],
        'defines': [
          'GLEW_STATIC',
        ],
      },
      'sources': [
        '../third_party/glew/src/glew.c',
      ],
      'conditions': [
        [ 'OS=="linux" or OS=="freebsd"',
          {
            'all_dependent_settings': {
              'defines': [
                'GL_GLEXT_PROTOTYPES',
              ],
              'ldflags': [
                '-L<(PRODUCT_DIR)',
              ],
              'libraries': [
                '-lX11',
              ],
            },
          },
        ],
        [ 'OS=="mac"',
          {
            'direct_dependent_settings': {
              'libraries': [
                '$(SDKROOT)/System/Library/Frameworks/OpenGL.framework',
              ],
            },
          },
        ],
        [ 'OS=="win"',
          {
            'all_dependent_settings': {
              'libraries': [
                '-lOpenGL32.lib',
              ],
            },
          },
        ],
      ],
    },
    {
      'target_name': 'command_buffer_common',
      'type': 'static_library',
      'include_dirs': [
        'command_buffer/common',
        '..',
      ],
      'all_dependent_settings': {
        'include_dirs': [
          'command_buffer/common',
          '..',
        ],
      },
      'dependencies': [
        '../base/base.gyp:base',
      ],
      'sources': [
        'command_buffer/common/bitfield_helpers.h',
        'command_buffer/common/buffer.h',
        'command_buffer/common/cmd_buffer_common.h',
        'command_buffer/common/cmd_buffer_common.cc',
        'command_buffer/common/command_buffer.h',
        'command_buffer/common/command_buffer_mock.h',
        'command_buffer/common/constants.h',
        'command_buffer/common/gles2_cmd_ids_autogen.h',
        'command_buffer/common/gles2_cmd_ids.h',
        'command_buffer/common/gles2_cmd_format_autogen.h',
        'command_buffer/common/gles2_cmd_format.cc',
        'command_buffer/common/gles2_cmd_format.h',
        'command_buffer/common/gles2_cmd_utils.cc',
        'command_buffer/common/gles2_cmd_utils.h',
        'command_buffer/common/logging.h',
        'command_buffer/common/mocks.h',
        'command_buffer/common/resource.cc',
        'command_buffer/common/resource.h',
        'command_buffer/common/types.h',
      ],
    },
    {
      # Library helps make GLES2 command buffers.
      'target_name': 'gles2_cmd_helper',
      'type': 'static_library',
      'dependencies': [
        'command_buffer_common',
      ],
      'sources': [
        'command_buffer/client/gles2_cmd_helper.cc',
        'command_buffer/client/gles2_cmd_helper.h',
        'command_buffer/client/gles2_cmd_helper_autogen.h',
      ],
    },
    {
      # Library emulates GLES2 using command_buffers.
      'target_name': 'gles2_implementation',
      'type': 'static_library',
      'dependencies': [
        'gles2_cmd_helper',
      ],
      'sources': [
        'command_buffer/client/gles2_implementation_autogen.h',
        'command_buffer/client/gles2_implementation.cc',
        'command_buffer/client/gles2_implementation_gen.h',
        'command_buffer/client/gles2_implementation.h',
      ],
    },
    {
      # Stub to expose gles2_implementation as a namespace rather than a class
      # so GLES2 C++ programs can work with no changes.
      'target_name': 'gles2_lib',
      'type': 'static_library',
      'dependencies': [
        'gles2_implementation',
      ],
      'sources': [
        'command_buffer/client/gles2_lib.cc',
        'command_buffer/client/gles2_lib.h',
      ],
    },
    {
      # Stub to expose gles2_implemenation in C instead of C++.
      # so GLES2 C programs can work with no changes.
      'target_name': 'gles2_c_lib',
      'type': 'static_library',
      'dependencies': [
        'gles2_lib',
      ],
      'sources': [
        'command_buffer/client/gles2_c_lib.h',
        'command_buffer/client/gles2_c_lib.cc',
        'command_buffer/client/gles2_c_lib_autogen.h',
      ],
    },
    {
      'target_name': 'command_buffer_common_unittests',
      'type': 'none',
      'include_dirs': [
        'command_buffer/common',
      ],
      'dependencies': [
        'gles2_lib',
        'gles2_implementation',
        'gles2_cmd_helper',
      ],
      'direct_dependent_settings': {
        'sources': [
          'command_buffer/common/bitfield_helpers_test.cc',
        ],
      },
    },
    {
      'target_name': 'command_buffer_client',
      'type': 'static_library',
      'dependencies': [
        'command_buffer_common',
      ],
      'sources': [
        'command_buffer/client/cmd_buffer_helper.cc',
        'command_buffer/client/cmd_buffer_helper.h',
        'command_buffer/client/fenced_allocator.cc',
        'command_buffer/client/fenced_allocator.h',
        'command_buffer/client/id_allocator.cc',
        'command_buffer/client/id_allocator.h',
      ],
    },
    {
      'target_name': 'command_buffer_client_unittests',
      'type': 'none',
      'direct_dependent_settings': {
        'sources': [
          'command_buffer/client/cmd_buffer_helper_test.cc',
          'command_buffer/client/fenced_allocator_test.cc',
          'command_buffer/client/id_allocator_test.cc',
        ],
      },
    },
    {
      'target_name': 'command_buffer_service_impl',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'all_dependent_settings': {
        'include_dirs': [
          '..',
        ],
      },
      'dependencies': [
        'command_buffer_common',
      ],
      'sources': [
        'command_buffer/service/common_decoder.cc',
        'command_buffer/service/common_decoder.h',
        'command_buffer/service/cmd_buffer_engine.h',
        'command_buffer/service/command_buffer_service.cc',
        'command_buffer/service/command_buffer_service.h',
        'command_buffer/service/cmd_parser.cc',
        'command_buffer/service/cmd_parser.h',
        'command_buffer/service/gpu_processor.h',
        'command_buffer/service/gpu_processor.cc',
        'command_buffer/service/gpu_processor_mock.h',
        'command_buffer/service/mocks.h',
        'command_buffer/service/precompile.cc',
        'command_buffer/service/precompile.h',
        'command_buffer/service/resource.cc',
        'command_buffer/service/resource.h',
      ],
      'conditions': [
        ['OS == "linux" or OS == "freebsd"',
          {
            'sources': [
              'command_buffer/service/gpu_processor_linux.cc',
              'command_buffer/service/x_utils.cc',
              'command_buffer/service/x_utils.h',
            ],
            'dependencies': [
              'gl_libs',
              '../build/linux/system.gyp:gtk',
            ]
          },
        ],
        ['OS == "win"',
          {
            'sources': [
              'command_buffer/service/gpu_processor_win.cc',
            ],
          },
        ],
      ],
    },
    {
      'target_name': 'command_buffer_service',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'all_dependent_settings': {
        'include_dirs': [
          '..',
        ],
      },
      'dependencies': [
        'command_buffer_service_impl',
        'gl_libs',
      ],
      'sources': [
        '<@(gpu_source_files)',
      ],
    },
    {
      'target_name': 'command_buffer_service_unittests',
      'type': 'none',
      'direct_dependent_settings': {
        'sources': [
          '<@(gpu_source_files)',
          'command_buffer/service/cmd_parser_test.cc',
          'command_buffer/service/command_buffer_service_unittest.cc',
          'command_buffer/service/common_decoder_unittest.cc',
          'command_buffer/service/gpu_processor_unittest.cc',
          'command_buffer/service/resource_test.cc',
          'command_buffer/service/gl_interface.h',
          'command_buffer/service/gl_interface.cc',
          'command_buffer/service/gl_mock.h',
          'command_buffer/service/gl_mock.cc',
          'command_buffer/service/gles2_cmd_decoder_unittest.cc',
          'command_buffer/service/gles2_cmd_decoder_unittest_autogen.h',
          'command_buffer/common/gles2_cmd_format_test.cc',
          'command_buffer/common/gles2_cmd_format_test_autogen.h',
          'command_buffer/common/gles2_cmd_id_test.cc',
          'command_buffer/common/gles2_cmd_id_test_autogen.h',
        ],
      },
    },
    {
      'target_name': 'gpu_plugin',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        'command_buffer_service',
      ],
      'include_dirs': [
        '..',
      ],
      'all_dependent_settings': {
        'include_dirs': [
          '..',
        ],
      },
      'sources': [
        'gpu_plugin/gpu_plugin.cc',
        'gpu_plugin/gpu_plugin.h',
      ],
    },
    {
      'target_name': 'gpu_all_unittests',
      'type': 'executable',
      'dependencies': [
        '../testing/gmock.gyp:gmock',
        '../testing/gmock.gyp:gmockmain',
        '../testing/gtest.gyp:gtest',
        'command_buffer_client',
        'command_buffer_client_unittests',
        'command_buffer_common',
        'command_buffer_common_unittests',
        'command_buffer_service_impl',
        'command_buffer_service_unittests',
      ],
    },
    {
      'target_name': 'gles2_demo_lib',
      'type': 'static_library',
      'dependencies': [
        'command_buffer_client',
        'gles2_lib',
        'gles2_c_lib',
      ],
      'sources': [
        'command_buffer/client/gles2_demo_c.h',
        'command_buffer/client/gles2_demo_c.c',
        'command_buffer/client/gles2_demo_cc.h',
        'command_buffer/client/gles2_demo_cc.cc',
      ],
    },
  ],
  'conditions': [
    ['OS == "win"',
      {
        'targets': [
          {
            'target_name': 'gles2_demo',
            'type': 'executable',
            'dependencies': [
              'command_buffer_service',
              'gles2_demo_lib',
            ],
            'sources': [
              'command_buffer/client/gles2_demo.cc',
            ],
          },
        ],
      },
    ],
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
