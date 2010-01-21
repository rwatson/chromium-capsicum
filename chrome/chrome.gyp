# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,

    'variables': {
      'version_py_path': 'tools/build/version.py',
      'version_path': 'VERSION',
    },
    'version_py_path': '<(version_py_path)',
    'version_path': '<(version_path)',
    'version_full':
        '<!(python <(version_py_path) -f <(version_path) -t "@MAJOR@.@MINOR@.@BUILD@.@PATCH@")',
    'version_build_patch':
        '<!(python <(version_py_path) -f <(version_path) -t "@BUILD@.@PATCH@")',

    # Define the common dependencies that contain all the actual
    # Chromium functionality.  This list gets pulled in below by
    # the link of the actual chrome (or chromium) executable on
    # Linux or Mac, and into chrome.dll on Windows.
    'chromium_dependencies': [
      'common',
      'browser',
      'debugger',
      'renderer',
      'syncapi',
      'utility',
      'profile_import',
      'worker',
      '../printing/printing.gyp:printing',
      '../webkit/webkit.gyp:inspector_resources',
    ],
    'nacl_win64_dependencies': [
      'common_nacl_win64',
      'common_constants_win64',
    ],
    'allocator_target': '../base/allocator/allocator.gyp:allocator',
    'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/chrome',
    'protoc_out_dir': '<(SHARED_INTERMEDIATE_DIR)/protoc_out',
    'chrome_strings_grds': [
      # Localizable resources.
      'app/resources/locale_settings.grd',
      'app/chromium_strings.grd',
      'app/generated_resources.grd',
      'app/google_chrome_strings.grd',
    ],
    'chrome_resources_grds': [
      # Data resources.
      'browser/browser_resources.grd',
      'common/common_resources.grd',
      'renderer/renderer_resources.grd',
    ],
    'grit_info_cmd': ['python', '../tools/grit/grit_info.py',],
    'repack_locales_cmd': ['python', 'tools/build/repack_locales.py',],
    # TODO: remove this helper when we have loops in GYP
    'apply_locales_cmd': ['python', 'tools/build/apply_locales.py',],
    'conditions': [
      ['OS=="win"', {
        'nacl_defines': [
          'NACL_WINDOWS=1',
          'NACL_LINUX=0',
          'NACL_OSX=0',
        ],
      },],
      ['OS=="linux"', {
        'nacl_defines': [
          'NACL_WINDOWS=0',
          'NACL_LINUX=1',
          'NACL_OSX=0',
        ],
      },],
      ['OS=="mac"', {
        'tweak_info_plist_path': 'tools/build/mac/tweak_info_plist',
        'nacl_defines': [
          'NACL_WINDOWS=0',
          'NACL_LINUX=0',
          'NACL_OSX=1',
        ],
        'conditions': [
          ['branding=="Chrome"', {
            'mac_bundle_id': 'com.google.Chrome',
            'mac_creator': 'rimZ',
          }, {  # else: branding!="Chrome"
            'mac_bundle_id': 'org.chromium.Chromium',
            'mac_creator': 'Cr24',
          }],  # branding
        ],  # conditions
      }],  # OS=="mac"
      ['target_arch=="ia32"', {
        'nacl_defines': [
          # TODO(gregoryd): consider getting this from NaCl's common.gypi
          'NACL_TARGET_SUBARCH=32',
          'NACL_BUILD_SUBARCH=32',
        ],
      }],
      ['target_arch=="x64"', {
        'nacl_defines': [
          # TODO(gregoryd): consider getting this from NaCl's common.gypi
          'NACL_TARGET_SUBARCH=64',
          'NACL_BUILD_SUBARCH=64',
        ],
      }],
    ],  # conditions
  },  # variables
  'includes': [
    # Place some targets in gypi files to reduce contention on this file.
    # By using an include, we keep everything in a single xcodeproj file.
    # Note on Win64 targets: targets that end with win64 be used
    # on 64-bit Windows only. Targets that end with nacl_win64 should be used
    # by Native Client only.
    'chrome_browser.gypi',
    'chrome_common.gypi',
    'chrome_dll.gypi',
    'chrome_exe.gypi',
    'chrome_renderer.gypi',
    'chrome_tests.gypi',
    'common_constants.gypi',
    'nacl.gypi',
  ],
  'targets': [
    {
      # TODO(mark): It would be better if each static library that needed
      # to run grit would list its own .grd files, but unfortunately some
      # of the static libraries currently have circular dependencies among
      # generated headers.
      'target_name': 'chrome_resources',
      'type': 'none',
      'msvs_guid': 'B95AB527-F7DB-41E9-AD91-EB51EE0F56BE',
      'variables': {
        'chrome_resources_inputs': [
          '<!@(<(grit_info_cmd) --inputs <(chrome_resources_grds))',
        ],
      },
      'rules': [
        {
          'rule_name': 'grit',
          'extension': 'grd',
          'variables': {
            'conditions': [
              ['branding=="Chrome"', {
                # TODO(mmoss) The .grd files look for _google_chrome, but for
                # consistency they should look for GOOGLE_CHROME_BUILD like C++.
                # Clean this up when Windows moves to gyp.
                'chrome_build': '_google_chrome',
                'branded_env': 'CHROMIUM_BUILD=google_chrome',
              }, {  # else: branding!="Chrome"
                'chrome_build': '_chromium',
                'branded_env': 'CHROMIUM_BUILD=chromium',
              }],
            ],
          },
          'inputs': [
            '<@(chrome_resources_inputs)',
          ],
          'outputs': [
            '<(grit_out_dir)/grit/<(RULE_INPUT_ROOT).h',
            '<(grit_out_dir)/<(RULE_INPUT_ROOT).pak',
            # TODO(bradnelson): move to something like this instead
            #'<!@(<(grit_info_cmd) --outputs \'<(grit_out_dir)\' <(chrome_resources_grds))',
            # This currently cannot work because gyp only evaluates the
            # outputs once (rather than per case where the rule applies).
            # This means you end up with all the outputs from all the grd
            # files, which angers scons and confuses vstudio.
            # One alternative would be to turn this into several actions,
            # but that would be rather verbose.
          ],
          'action': ['python', '../tools/grit/grit.py', '-i',
            '<(RULE_INPUT_PATH)',
            'build', '-o', '<(grit_out_dir)',
            '-D', '<(chrome_build)',
            '-E', '<(branded_env)',
          ],
          'conditions': [
            ['chromeos==1', {
              'action': ['-D', 'chromeos'],
            }],
            ['use_titlecase_in_grd_files==1', {
              'action': ['-D', 'use_titlecase'],
            }],
          ],
          'message': 'Generating resources from <(RULE_INPUT_PATH)',
        },
      ],
      'sources': [
        '<@(chrome_resources_grds)',
        '<@(chrome_resources_inputs)',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(grit_out_dir)',
        ],
      },
      'conditions': [
        ['OS=="win"', {
          'dependencies': ['../build/win/system.gyp:cygwin'],
        }],
      ],
    },
    {
      # TODO(mark): It would be better if each static library that needed
      # to run grit would list its own .grd files, but unfortunately some
      # of the static libraries currently have circular dependencies among
      # generated headers.
      'target_name': 'chrome_strings',
      'msvs_guid': 'D9DDAF60-663F-49CC-90DC-3D08CC3D1B28',
      'conditions': [
        ['OS=="win"', {
          # HACK(nsylvain): We want to enforce a fake dependency on
          # intaller_util_string.  install_util depends on both
          # chrome_strings and installer_util_strings, but for some reasons
          # Incredibuild does not enforce it (most likely a bug). By changing
          # the type and making sure we depend on installer_util_strings, it
          # will always get built before installer_util.
          'type': 'dummy_executable',
          'dependencies': ['../build/win/system.gyp:cygwin',
                           'installer/installer.gyp:installer_util_strings',],
        }, {
          'type': 'none',
        }],
      ],
      'variables': {
        'chrome_strings_inputs': [
          '<!@(<(grit_info_cmd) --inputs <(chrome_strings_grds))',
        ],
      },
      'rules': [
        {
          'rule_name': 'grit',
          'extension': 'grd',
          'variables': {
            'conditions': [
              ['branding=="Chrome"', {
                # TODO(mmoss) The .grd files look for _google_chrome, but for
                # consistency they should look for GOOGLE_CHROME_BUILD like C++.
                # Clean this up when Windows moves to gyp.
                'chrome_build': '_google_chrome',
              }, {  # else: branding!="Chrome"
                'chrome_build': '_chromium',
              }],
            ],
          },
          'inputs': [
            '<@(chrome_strings_inputs)',
          ],
          'outputs': [
            '<(grit_out_dir)/grit/<(RULE_INPUT_ROOT).h',
            # TODO: remove this helper when we have loops in GYP
            '>!@(<(apply_locales_cmd) \'<(grit_out_dir)/<(RULE_INPUT_ROOT)_ZZLOCALE.pak\' <(locales))',
            # TODO(bradnelson): move to something like this
            #'<!@(<(grit_info_cmd) --outputs \'<(grit_out_dir)\' <(chrome_strings_grds))',
            # See comment in chrome_resources as to why.
          ],
          'action': ['python', '../tools/grit/grit.py', '-i',
                    '<(RULE_INPUT_PATH)',
                    'build', '-o', '<(grit_out_dir)',
                    '-D', '<(chrome_build)'],
          'conditions': [
            ['chromeos==1', {
              'action': ['-D', 'chromeos'],
            }],
            ['use_titlecase_in_grd_files==1', {
              'action': ['-D', 'use_titlecase'],
            }],
          ],
          'message': 'Generating resources from <(RULE_INPUT_PATH)',
        },
      ],
      'sources': [
        '<@(chrome_strings_grds)',
        '<@(chrome_strings_inputs)',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(grit_out_dir)',
        ],
      },
    },
    {
      # theme_resources also generates a .cc file, so it can't use the rules above.
      'target_name': 'theme_resources',
      'type': 'none',
      'msvs_guid' : 'A158FB0A-25E4-6523-6B5A-4BB294B73D31',
      'variables': {
        'grit_path': '../tools/grit/grit.py',
      },
      'actions': [
        {
          'action_name': 'theme_resources',
          'variables': {
            'input_path': 'app/theme/theme_resources.grd',
            'conditions': [
              ['branding=="Chrome"', {
                # TODO(mmoss) The .grd files look for _google_chrome, but for
                # consistency they should look for GOOGLE_CHROME_BUILD like C++.
                # Clean this up when Windows moves to gyp.
                'chrome_build': '_google_chrome',
              }, {  # else: branding!="Chrome"
                'chrome_build': '_chromium',
              }],
            ],
          },
          'inputs': [
            '<!@(<(grit_info_cmd) --inputs <(input_path))',
          ],
          'outputs': [
            '<!@(<(grit_info_cmd) --outputs \'<(grit_out_dir)\' <(input_path))',
          ],
          'action': [
            'python', '<(grit_path)',
            '-i', '<(input_path)', 'build',
            '-o', '<(grit_out_dir)',
            '-D', '<(chrome_build)'
          ],
          'conditions': [
            ['chromeos==1', {
              'action': ['-D', 'chromeos'],
            }],
            ['use_titlecase_in_grd_files==1', {
              'action': ['-D', 'use_titlecase'],
            }],
          ],
          'message': 'Generating resources from <(input_path)',
        },
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(grit_out_dir)',
        ],
      },
      'conditions': [
        ['OS=="win"', {
          'dependencies': ['../build/win/system.gyp:cygwin'],
        }],
      ],
    },
    {
      'target_name': 'default_extensions',
      'type': 'none',
      'msvs_guid': 'DA9BAB64-91DC-419B-AFDE-6FF8C569E83A',
      'conditions': [
        ['OS=="win"', {
          'copies': [
            {
              'destination': '<(PRODUCT_DIR)/extensions',
              'files': [
                'browser/extensions/default_extensions/external_extensions.json'
              ]
            }
          ],
        }],
      ],
    },
    {
      'target_name': 'debugger',
      'type': '<(library)',
      'msvs_guid': '57823D8C-A317-4713-9125-2C91FDFD12D6',
      'dependencies': [
        'chrome_resources',
        'chrome_strings',
        'theme_resources',
        '../skia/skia.gyp:skia',
        '../third_party/icu/icu.gyp:icui18n',
        '../third_party/icu/icu.gyp:icuuc',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'browser/debugger/debugger_remote_service.cc',
        'browser/debugger/debugger_remote_service.h',
        'browser/debugger/debugger_wrapper.cc',
        'browser/debugger/debugger_wrapper.h',
        'browser/debugger/devtools_client_host.h',
        'browser/debugger/devtools_manager.cc',
        'browser/debugger/devtools_manager.h',
        'browser/debugger/devtools_protocol_handler.cc',
        'browser/debugger/devtools_protocol_handler.h',
        'browser/debugger/devtools_remote.h',
        'browser/debugger/devtools_remote_listen_socket.cc',
        'browser/debugger/devtools_remote_listen_socket.h',
        'browser/debugger/devtools_remote_message.cc',
        'browser/debugger/devtools_remote_message.h',
        'browser/debugger/devtools_remote_service.cc',
        'browser/debugger/devtools_remote_service.h',
        'browser/debugger/devtools_window.cc',
        'browser/debugger/devtools_window.h',
        'browser/debugger/extension_ports_remote_service.cc',
        'browser/debugger/extension_ports_remote_service.h',
        'browser/debugger/inspectable_tab_proxy.cc',
        'browser/debugger/inspectable_tab_proxy.h',
      ],
      'conditions': [
        ['OS=="linux" or OS=="freebsd"', {
          'dependencies': [
            '../build/linux/system.gyp:gtk',
          ],
        }],
      ],
    },
    {
      'target_name': 'plugin',
      'type': '<(library)',
      'msvs_guid': '20A560A0-2CD0-4D9E-A58B-1F24B99C087A',
      'dependencies': [
        'common',
        'chrome_resources',
        'chrome_strings',
        '../media/media.gyp:media',
        '../skia/skia.gyp:skia',
        '../third_party/icu/icu.gyp:icui18n',
        '../third_party/icu/icu.gyp:icuuc',
        '../third_party/libxml/libxml.gyp:libxml',
        '../third_party/npapi/npapi.gyp:npapi',
        '../third_party/hunspell/hunspell.gyp:hunspell',
        '../webkit/webkit.gyp:glue',
      ],
      'include_dirs': [
        '<(INTERMEDIATE_DIR)',
      ],
      'sources': [
        # All .cc, .h, .m, and .mm files under plugins except for tests and
        # mocks.
        'plugin/chrome_plugin_host.cc',
        'plugin/chrome_plugin_host.h',
        'plugin/npobject_proxy.cc',
        'plugin/npobject_proxy.h',
        'plugin/npobject_stub.cc',
        'plugin/npobject_stub.h',
        'plugin/npobject_util.cc',
        'plugin/npobject_util.h',
        'plugin/plugin_channel.cc',
        'plugin/plugin_channel.h',
        'plugin/plugin_channel_base.cc',
        'plugin/plugin_channel_base.h',
        'plugin/plugin_interpose_util_mac.mm',
        'plugin/plugin_interpose_util_mac.h',
        'plugin/plugin_main.cc',
        'plugin/plugin_main_linux.cc',
        'plugin/plugin_main_mac.mm',
        'plugin/plugin_thread.cc',
        'plugin/plugin_thread.h',
        'plugin/webplugin_delegate_stub.cc',
        'plugin/webplugin_delegate_stub.h',
        'plugin/webplugin_proxy.cc',
        'plugin/webplugin_proxy.h',
      ],
      # These are layered in conditionals in the event other platforms
      # end up using this module as well.
      'conditions': [
        ['OS=="win"', {
          'defines': [
            '__STD_C',
            '_CRT_SECURE_NO_DEPRECATE',
            '_SCL_SECURE_NO_DEPRECATE',
          ],
          'include_dirs': [
            'third_party/wtl/include',
          ],
        }],
        ['OS=="win" or (OS=="linux" and target_arch!="arm")', {
          'dependencies': [
            '../gpu/gpu.gyp:command_buffer_service',
          ],
          'sources': [
            'plugin/command_buffer_stub.cc',
            'plugin/command_buffer_stub.h',
          ],
        },],
        ['OS=="linux" or OS=="freebsd"', {
          'dependencies': [
            '../build/linux/system.gyp:gtk',
          ],
        }],
      ],
    },
    {
      'target_name': 'utility',
      'type': '<(library)',
      'msvs_guid': '4D2B38E6-65FF-4F97-B88A-E441DF54EBF7',
      'dependencies': [
        '../base/base.gyp:base',
        '../skia/skia.gyp:skia',
      ],
      'sources': [
        'utility/utility_main.cc',
        'utility/utility_thread.cc',
        'utility/utility_thread.h',
      ],
      'include_dirs': [
        '..',
      ],
      'conditions': [
        ['OS=="linux" or OS=="freebsd"', {
          'dependencies': [
            '../build/linux/system.gyp:gtk',
          ],
        }],
      ],
    },
    {
      'target_name': 'profile_import',
      'type': '<(library)',
      'dependencies': [
        '../base/base.gyp:base',
      ],
      'sources': [
        'profile_import/profile_import_main.cc',
        'profile_import/profile_import_thread.cc',
        'profile_import/profile_import_thread.h',
      ],
    },
    {
      'target_name': 'worker',
      'type': '<(library)',
      'msvs_guid': 'C78D02D0-A366-4EC6-A248-AA8E64C4BA18',
      'dependencies': [
        '../base/base.gyp:base',
        '../third_party/WebKit/WebKit/chromium/WebKit.gyp:webkit',
      ],
      'sources': [
        'worker/nativewebworker_impl.cc',
        'worker/nativewebworker_impl.h',
        'worker/nativewebworker_stub.cc',
        'worker/nativewebworker_stub.h',
        'worker/websharedworker_stub.cc',
        'worker/websharedworker_stub.h',
        'worker/webworker_stub_base.cc',
        'worker/webworker_stub_base.h',
        'worker/webworker_stub.cc',
        'worker/webworker_stub.h',
        'worker/webworkerclient_proxy.cc',
        'worker/webworkerclient_proxy.h',
        'worker/worker_main.cc',
        'worker/worker_thread.cc',
        'worker/worker_thread.h',
        'worker/worker_webkitclient_impl.cc',
        'worker/worker_webkitclient_impl.h',
      ],
      'include_dirs': [
        '..',
      ],
    },
    {
      # Provides a syncapi dynamic library target from checked-in binaries,
      # or from compiling a stub implementation.
      'target_name': 'syncapi',
      'type': '<(library)',
      'sources': [
        'browser/sync/engine/syncapi.cc',
      ],
      'include_dirs': [
        '..',
        '<(protoc_out_dir)',
      ],
      'defines' : [
        '_CRT_SECURE_NO_WARNINGS',
        '_USE_32BIT_TIME_T',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../build/temp_gyp/googleurl.gyp:googleurl',
        '../third_party/icu/icu.gyp:icuuc',
        '../third_party/libjingle/libjingle.gyp:libjingle',
        '../third_party/sqlite/sqlite.gyp:sqlite',
        'common_constants',
        'notifier',
        'sync',
        'sync_proto',
      ],
    },
    {
      # Protobuf compiler / generate rule for sync.proto
      'target_name': 'sync_proto',
      'type': 'none',
      'actions': [
        {
          'action_name': 'compiling sync.proto',
          'inputs': [
            '<(PRODUCT_DIR)/<(EXECUTABLE_PREFIX)protoc<(EXECUTABLE_SUFFIX)',
            'browser/sync/protocol/sync.proto',
          ],
          'outputs': [
            '<(protoc_out_dir)/chrome/browser/sync/protocol/sync.pb.cc',
            '<(protoc_out_dir)/chrome/browser/sync/protocol/sync.pb.h',
          ],
          'action': [
            '<(PRODUCT_DIR)/<(EXECUTABLE_PREFIX)protoc<(EXECUTABLE_SUFFIX)',
            '--proto_path=browser/sync/protocol',
            'browser/sync/protocol/sync.proto',
            '--cpp_out=<(protoc_out_dir)/chrome/browser/sync/protocol',
          ],
        },
      ],
      'dependencies': [
        '../third_party/protobuf2/protobuf.gyp:protobuf_lite',
        '../third_party/protobuf2/protobuf.gyp:protoc#host',
      ],
      'export_dependent_settings': [
        '../third_party/protobuf2/protobuf.gyp:protobuf_lite',
      ],
    },
    {
      'target_name': 'notifier',
      'type': '<(library)',
      'sources': [
        'browser/sync/notifier/base/async_dns_lookup.cc',
        'browser/sync/notifier/base/async_dns_lookup.h',
        'browser/sync/notifier/base/async_network_alive.h',
        'browser/sync/notifier/base/fastalloc.h',
        'browser/sync/notifier/base/linux/network_status_detector_task_linux.cc',
        'browser/sync/notifier/base/mac/network_status_detector_task_mac.h',
        'browser/sync/notifier/base/mac/network_status_detector_task_mac.cc',
        'browser/sync/notifier/base/nethelpers.cc',
        'browser/sync/notifier/base/nethelpers.h',
        'browser/sync/notifier/base/network_status_detector_task.cc',
        'browser/sync/notifier/base/network_status_detector_task.h',
        'browser/sync/notifier/base/network_status_detector_task_mt.cc',
        'browser/sync/notifier/base/network_status_detector_task_mt.h',
        'browser/sync/notifier/base/posix/time_posix.cc',
        'browser/sync/notifier/base/signal_thread_task.h',
        'browser/sync/notifier/base/ssl_adapter.h',
        'browser/sync/notifier/base/ssl_adapter.cc',
        'browser/sync/notifier/base/static_assert.h',
        'browser/sync/notifier/base/task_pump.cc',
        'browser/sync/notifier/base/task_pump.h',
        'browser/sync/notifier/base/time.cc',
        'browser/sync/notifier/base/time.h',
        'browser/sync/notifier/base/timer.cc',
        'browser/sync/notifier/base/timer.h',
        'browser/sync/notifier/base/utils.h',
        'browser/sync/notifier/base/win/async_network_alive_win32.cc',
        'browser/sync/notifier/base/win/time_win32.cc',
        'browser/sync/notifier/communicator/auth_task.cc',
        'browser/sync/notifier/communicator/auth_task.h',
        'browser/sync/notifier/communicator/auto_reconnect.cc',
        'browser/sync/notifier/communicator/auto_reconnect.h',
        'browser/sync/notifier/communicator/connection_options.cc',
        'browser/sync/notifier/communicator/connection_options.h',
        'browser/sync/notifier/communicator/connection_settings.cc',
        'browser/sync/notifier/communicator/connection_settings.h',
        'browser/sync/notifier/communicator/const_communicator.h',
        'browser/sync/notifier/communicator/login.cc',
        'browser/sync/notifier/communicator/login.h',
        'browser/sync/notifier/communicator/login_failure.cc',
        'browser/sync/notifier/communicator/login_failure.h',
        'browser/sync/notifier/communicator/login_settings.cc',
        'browser/sync/notifier/communicator/login_settings.h',
        'browser/sync/notifier/communicator/product_info.cc',
        'browser/sync/notifier/communicator/product_info.h',
        'browser/sync/notifier/communicator/single_login_attempt.cc',
        'browser/sync/notifier/communicator/single_login_attempt.h',
        'browser/sync/notifier/communicator/ssl_socket_adapter.cc',
        'browser/sync/notifier/communicator/ssl_socket_adapter.h',
        'browser/sync/notifier/communicator/talk_auth_task.cc',
        'browser/sync/notifier/communicator/talk_auth_task.h',
        'browser/sync/notifier/communicator/xmpp_connection_generator.cc',
        'browser/sync/notifier/communicator/xmpp_connection_generator.h',
        'browser/sync/notifier/communicator/xmpp_log.cc',
        'browser/sync/notifier/communicator/xmpp_log.h',
        'browser/sync/notifier/communicator/xmpp_socket_adapter.cc',
        'browser/sync/notifier/communicator/xmpp_socket_adapter.h',
        'browser/sync/notifier/gaia_auth/gaiaauth.cc',
        'browser/sync/notifier/gaia_auth/gaiaauth.h',
        'browser/sync/notifier/gaia_auth/gaiahelper.cc',
        'browser/sync/notifier/gaia_auth/gaiahelper.h',
        'browser/sync/notifier/gaia_auth/inet_aton.h',
        'browser/sync/notifier/gaia_auth/sigslotrepeater.h',
        'browser/sync/notifier/gaia_auth/win/win32window.cc',
        'browser/sync/notifier/listener/listen_task.cc',
        'browser/sync/notifier/listener/listen_task.h',
        'browser/sync/notifier/listener/mediator_thread.h',
        'browser/sync/notifier/listener/mediator_thread_impl.cc',
        'browser/sync/notifier/listener/mediator_thread_impl.h',
        'browser/sync/notifier/listener/mediator_thread_mock.h',
        'browser/sync/notifier/listener/send_update_task.cc',
        'browser/sync/notifier/listener/send_update_task.h',
        'browser/sync/notifier/listener/subscribe_task.cc',
        'browser/sync/notifier/listener/subscribe_task.h',
        'browser/sync/notifier/listener/talk_mediator.h',
        'browser/sync/notifier/listener/talk_mediator_impl.cc',
        'browser/sync/notifier/listener/talk_mediator_impl.h',
      ],
      'include_dirs': [
        '..',
        '<(protoc_out_dir)',
      ],
      'defines' : [
        '_CRT_SECURE_NO_WARNINGS',
        '_USE_32BIT_TIME_T',
        'kXmppProductName="chromium-sync"',
      ],
      'dependencies': [
        '../third_party/expat/expat.gyp:expat',
        '../third_party/libjingle/libjingle.gyp:libjingle',
        'sync_proto',
      ],
      'conditions': [
        ['OS=="linux" or OS == "freebsd"', {
          'sources!': [
            'browser/sync/notifier/base/network_status_detector_task_mt.cc',
          ],
          'dependencies': [
            '../build/linux/system.gyp:gtk'
          ],
        }],
      ],
    },
    {
      'target_name': 'sync',
      'type': '<(library)',
      'sources': [
        '<(protoc_out_dir)/chrome/browser/sync/protocol/sync.pb.cc',
        '<(protoc_out_dir)/chrome/browser/sync/protocol/sync.pb.h',
        'browser/sync/engine/all_status.cc',
        'browser/sync/engine/all_status.h',
        'browser/sync/engine/apply_updates_command.cc',
        'browser/sync/engine/apply_updates_command.h',
        'browser/sync/engine/auth_watcher.cc',
        'browser/sync/engine/auth_watcher.h',
        'browser/sync/engine/authenticator.cc',
        'browser/sync/engine/authenticator.h',
        'browser/sync/engine/build_and_process_conflict_sets_command.cc',
        'browser/sync/engine/build_and_process_conflict_sets_command.h',
        'browser/sync/engine/build_commit_command.cc',
        'browser/sync/engine/build_commit_command.h',
        'browser/sync/engine/change_reorder_buffer.cc',
        'browser/sync/engine/change_reorder_buffer.h',
        'browser/sync/engine/conflict_resolver.cc',
        'browser/sync/engine/conflict_resolver.h',
        'browser/sync/engine/download_updates_command.cc',
        'browser/sync/engine/download_updates_command.h',
        'browser/sync/engine/get_commit_ids_command.cc',
        'browser/sync/engine/get_commit_ids_command.h',
        'browser/sync/engine/model_changing_syncer_command.cc',
        'browser/sync/engine/model_changing_syncer_command.h',
        'browser/sync/engine/model_safe_worker.h',
        'browser/sync/engine/net/gaia_authenticator.cc',
        'browser/sync/engine/net/gaia_authenticator.h',
        'browser/sync/engine/net/http_return.h',
        'browser/sync/engine/net/server_connection_manager.cc',
        'browser/sync/engine/net/server_connection_manager.h',
        'browser/sync/engine/net/syncapi_server_connection_manager.cc',
        'browser/sync/engine/net/syncapi_server_connection_manager.h',
        'browser/sync/engine/net/url_translator.cc',
        'browser/sync/engine/net/url_translator.h',
        'browser/sync/engine/post_commit_message_command.cc',
        'browser/sync/engine/post_commit_message_command.h',
        'browser/sync/engine/process_commit_response_command.cc',
        'browser/sync/engine/process_commit_response_command.h',
        'browser/sync/engine/process_updates_command.cc',
        'browser/sync/engine/process_updates_command.h',
        'browser/sync/engine/resolve_conflicts_command.cc',
        'browser/sync/engine/resolve_conflicts_command.h',
        'browser/sync/engine/syncapi.h',
        'browser/sync/engine/syncer.cc',
        'browser/sync/engine/syncer.h',
        'browser/sync/engine/syncer_command.cc',
        'browser/sync/engine/syncer_command.h',
        'browser/sync/engine/syncer_end_command.cc',
        'browser/sync/engine/syncer_end_command.h',
        'browser/sync/engine/syncer_proto_util.cc',
        'browser/sync/engine/syncer_proto_util.h',
        'browser/sync/engine/syncer_thread.cc',
        'browser/sync/engine/syncer_thread.h',
        'browser/sync/engine/syncer_types.h',
        'browser/sync/engine/syncer_util.cc',
        'browser/sync/engine/syncer_util.h',
        'browser/sync/engine/syncproto.h',
        'browser/sync/engine/update_applicator.cc',
        'browser/sync/engine/update_applicator.h',
        'browser/sync/engine/verify_updates_command.cc',
        'browser/sync/engine/verify_updates_command.h',
        'browser/sync/protocol/service_constants.h',
        'browser/sync/sessions/session_state.cc',
        'browser/sync/sessions/session_state.h',
        'browser/sync/sessions/status_controller.cc',
        'browser/sync/sessions/status_controller.h',
        'browser/sync/sessions/sync_session.cc',
        'browser/sync/sessions/sync_session.h',
        'browser/sync/sessions/sync_session_context.h',
        'browser/sync/syncable/blob.h',
        'browser/sync/syncable/dir_open_result.h',
        'browser/sync/syncable/directory_backing_store.cc',
        'browser/sync/syncable/directory_backing_store.h',
        'browser/sync/syncable/directory_event.h',
        'browser/sync/syncable/directory_manager.cc',
        'browser/sync/syncable/directory_manager.h',
        'browser/sync/syncable/path_name_cmp.h',
        'browser/sync/syncable/syncable-inl.h',
        'browser/sync/syncable/syncable.cc',
        'browser/sync/syncable/syncable.h',
        'browser/sync/syncable/syncable_changes_version.h',
        'browser/sync/syncable/syncable_columns.h',
        'browser/sync/syncable/syncable_id.cc',
        'browser/sync/syncable/syncable_id.h',
        'browser/sync/util/character_set_converters.h',
        'browser/sync/util/character_set_converters_posix.cc',
        'browser/sync/util/character_set_converters_win.cc',
        'browser/sync/util/closure.h',
        'browser/sync/util/crypto_helpers.cc',
        'browser/sync/util/crypto_helpers.h',
        'browser/sync/util/dbgq.h',
        'browser/sync/util/event_sys-inl.h',
        'browser/sync/util/event_sys.h',
        'browser/sync/util/extensions_activity_monitor.cc',
        'browser/sync/util/extensions_activity_monitor.h',
        'browser/sync/util/fast_dump.h',
        'browser/sync/util/row_iterator.h',
        'browser/sync/util/signin.h',
        'browser/sync/util/sync_types.h',
        'browser/sync/util/user_settings.cc',
        'browser/sync/util/user_settings.h',
        'browser/sync/util/user_settings_posix.cc',
        'browser/sync/util/user_settings_win.cc',
      ],
      'include_dirs': [
        '..',
        '<(protoc_out_dir)',
      ],
      'defines' : [
        'SYNC_ENGINE_VERSION_STRING="Unknown"',
        '_CRT_SECURE_NO_WARNINGS',
        '_USE_32BIT_TIME_T',
      ],
      'dependencies': [
        '../skia/skia.gyp:skia',
        '../third_party/libjingle/libjingle.gyp:libjingle',
        'sync_proto',
      ],
      'conditions': [
        ['OS=="win"', {
          'sources' : [
            'browser/sync/util/data_encryption.cc',
            'browser/sync/util/data_encryption.h',
          ],
        }],
        ['OS=="linux"', {
          'dependencies': [
            '../build/linux/system.gyp:gtk'
          ],
        }],
        [ 'OS == "freebsd"', {
          'dependencies': [
            '../build/linux/system.gyp:gtk'
          ],
          'sources/': [['exclude', '^browser/sync/util/path_helpers_linux.cc$']],
        }],
        ['OS=="mac"', {
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/IOKit.framework',
            ],
          },
        }],
      ],
    },
  ],
  'conditions': [
    ['OS=="mac"',
      { 'targets': [
        {
          'target_name': 'helper_app',
          'type': 'executable',
          'product_name': '<(mac_product_name) Helper',
          'mac_bundle': 1,
          'dependencies': [
            'chrome_dll',
            'interpose_dependency_shim',
            'infoplist_strings_tool',
          ],
          'sources': [
            # chrome_exe_main.mm's main() is the entry point for the "chrome"
            # (browser app) target.  All it does is jump to chrome_dll's
            # ChromeMain.  This is appropriate for helper processes too,
            # because the logic to discriminate between process types at run
            # time is actually directed by the --type command line argument
            # processed by ChromeMain.  Sharing chrome_exe_main.mm with the
            # browser app will suffice for now.
            'app/chrome_exe_main.mm',
            'app/helper-Info.plist',
          ],
          # TODO(mark): Come up with a fancier way to do this.  It should only
          # be necessary to list helper-Info.plist once, not the three times it
          # is listed here.
          'mac_bundle_resources!': [
            'app/helper-Info.plist',
          ],
          # TODO(mark): For now, don't put any resources into this app.  Its
          # resources directory will be a symbolic link to the browser app's
          # resources directory.
          'mac_bundle_resources/': [
            ['exclude', '.*'],
          ],
          'xcode_settings': {
            'CHROMIUM_BUNDLE_ID': '<(mac_bundle_id)',
            'CHROMIUM_SHORT_NAME': '<(branding)',
            'INFOPLIST_FILE': 'app/helper-Info.plist',
          },
          'copies': [
            {
              'destination': '<(PRODUCT_DIR)/<(mac_product_name) Helper.app/Contents/MacOS',
              'files': [
                '<(PRODUCT_DIR)/libplugin_carbon_interpose.dylib',
              ],
            },
          ],
          'actions': [
            {
              # Generate the InfoPlist.strings file
              'action_name': 'Generating InfoPlist.strings files',
              'variables': {
                'tool_path': '<(PRODUCT_DIR)/infoplist_strings_tool',
                # Unique dir to write to so the [lang].lproj/InfoPlist.strings
                # for the main app and the helper app don't name collide.
                'output_path': '<(INTERMEDIATE_DIR)/helper_infoplist_strings',
              },
              'conditions': [
                [ 'branding == "Chrome"', {
                  'variables': {
                     'branding_name': 'google_chrome_strings',
                  },
                }, { # else branding!="Chrome"
                  'variables': {
                     'branding_name': 'chromium_strings',
                  },
                }],
              ],
              'inputs': [
                '<(tool_path)',
                '<(version_path)',
                # TODO: remove this helper when we have loops in GYP
                '>!@(<(apply_locales_cmd) \'<(grit_out_dir)/<(branding_name)_ZZLOCALE.pak\' <(locales))',
              ],
              'outputs': [
                # TODO: remove this helper when we have loops in GYP
                '>!@(<(apply_locales_cmd) -d \'<(output_path)/ZZLOCALE.lproj/InfoPlist.strings\' <(locales))',
              ],
              'action': [
                '<(tool_path)',
                '-b', '<(branding_name)',
                '-v', '<(version_path)',
                '-g', '<(grit_out_dir)',
                '-o', '<(output_path)',
                '-t', 'helper',
                '<@(locales)',
              ],
              'message': 'Generating the language InfoPlist.strings files',
              'process_outputs_as_mac_bundle_resources': 1,
            },
          ],
          'postbuilds': [
            {
              # The framework (chrome_dll) defines its load-time path
              # (DYLIB_INSTALL_NAME_BASE) relative to the main executable
              # (chrome).  A different relative path needs to be used in
              # helper_app.
              'postbuild_name': 'Fix Framework Link',
              'action': [
                'install_name_tool',
                '-change',
                '@executable_path/../Versions/<(version_full)/<(mac_product_name) Framework.framework/<(mac_product_name) Framework',
                '@executable_path/../../../<(mac_product_name) Framework.framework/<(mac_product_name) Framework',
                '${BUILT_PRODUCTS_DIR}/${EXECUTABLE_PATH}'
              ],
            },
            {
              # Modify the Info.plist as needed.  The script explains why this
              # is needed.  This is also done in the chrome and chrome_dll
              # targets.  In this case, -b0 and -k0 are used because Breakpad
              # and Keystone keys are never placed into the helper, only into
              # the framework.  -s0 is used because Subversion keys are only
              # placed into the main app.
              'postbuild_name': 'Tweak Info.plist',
              'action': ['<(tweak_info_plist_path)',
                         '-b0',
                         '-k0',
                         '-s0',
                         '<(branding)',
                         '<(mac_bundle_id)'],
            },
          ],
          'conditions': [
            ['mac_breakpad==1', {
              'variables': {
                # A real .dSYM is needed for dump_syms to operate on.
                'mac_real_dsym': 1,
              },
            }],
          ],
        },  # target helper_app
        {
          # Convenience target to build a disk image.
          'target_name': 'build_app_dmg',
          # Don't place this in the 'all' list; most won't want it.
          # In GYP, booleans are 0/1, not True/False.
          'suppress_wildcard': 1,
          'type': 'none',
          'dependencies': [
            'chrome',
          ],
          'variables': {
            'build_app_dmg_script_path': 'tools/build/mac/build_app_dmg',
          },
          'actions': [
            {
              'inputs': [
                '<(build_app_dmg_script_path)',
                '<(PRODUCT_DIR)/<(branding).app',
              ],
              'outputs': [
                '<(PRODUCT_DIR)/<(branding).dmg',
              ],
              'action_name': 'build_app_dmg',
              'action': ['<(build_app_dmg_script_path)', '<@(branding)'],
            },
          ],  # 'actions'
        },
        {
          # Dummy target to allow chrome to require plugin_carbon_interpose to
          # build without actually linking to the resulting library.
          'target_name': 'interpose_dependency_shim',
          'type': 'executable',
          'dependencies': [
            'plugin_carbon_interpose',
          ],
          # In release, we end up with a strip step that is unhappy if there is
          # no binary. Rather than check in a new file for this temporary hack,
          # just generate a source file on the fly.
          'actions': [
            {
              'action_name': 'generate_stub_main',
              'process_outputs_as_sources': 1,
              'inputs': [],
              'outputs': [ '<(INTERMEDIATE_DIR)/dummy_main.c' ],
              'action': [
                'bash', '-c',
                'echo "int main() { return 0; }" > <(INTERMEDIATE_DIR)/dummy_main.c'
              ],
            },
          ],
        },
        {
          # dylib for interposing Carbon calls in the plugin process.
          'target_name': 'plugin_carbon_interpose',
          'type': 'shared_library',
          'dependencies': [
            'chrome_dll',
          ],
          'sources': [
            'browser/plugin_carbon_interpose_mac.cc',
          ],
          'include_dirs': [
            '..',
          ],
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/Carbon.framework',
            ],
          },
          'xcode_settings': {
            'DYLIB_COMPATIBILITY_VERSION': '<(version_build_patch)',
            'DYLIB_CURRENT_VERSION': '<(version_build_patch)',
            'DYLIB_INSTALL_NAME_BASE': '@executable_path',
          },
          'postbuilds': [
            {
              # The framework (chrome_dll) defines its load-time path
              # (DYLIB_INSTALL_NAME_BASE) relative to the main executable
              # (chrome).  A different relative path needs to be used in
              # plugin_carbon_interpose, which runs in the helper_app.
              'postbuild_name': 'Fix Framework Link',
              'action': [
                'install_name_tool',
                '-change',
                '@executable_path/../Versions/<(version_full)/<(mac_product_name) Framework.framework/<(mac_product_name) Framework',
                '@executable_path/../../../<(mac_product_name) Framework.framework/<(mac_product_name) Framework',
                '${BUILT_PRODUCTS_DIR}/${EXECUTABLE_PATH}'
              ],
            },
          ],
        },
        {
          'target_name': 'infoplist_strings_tool',
          'type': 'executable',
          'dependencies': [
            'chrome_strings',
            '../base/base.gyp:base',
          ],
          'include_dirs': [
            '<(grit_out_dir)',
          ],
          'sources': [
            'tools/mac_helpers/infoplist_strings_util.mm',
          ],
        },
      ],  # targets
    }, { # else: OS != "mac"
      'targets': [
        {
          'target_name': 'convert_dict',
          'type': 'executable',
          'msvs_guid': '42ECD5EC-722F-41DE-B6B8-83764C8016DF',
          'dependencies': [
            '../base/base.gyp:base',
            '../base/base.gyp:base_i18n',
            'convert_dict_lib',
            '../third_party/hunspell/hunspell.gyp:hunspell',
          ],
          'sources': [
            'tools/convert_dict/convert_dict.cc',
          ],
        },
        {
          'target_name': 'convert_dict_lib',
          'product_name': 'convert_dict',
          'type': 'static_library',
          'msvs_guid': '1F669F6B-3F4A-4308-E496-EE480BDF0B89',
          'include_dirs': [
            '..',
          ],
          'sources': [
            'tools/convert_dict/aff_reader.cc',
            'tools/convert_dict/aff_reader.h',
            'tools/convert_dict/dic_reader.cc',
            'tools/convert_dict/dic_reader.h',
            'tools/convert_dict/hunspell_reader.cc',
            'tools/convert_dict/hunspell_reader.h',
          ],
        },
        {
          'target_name': 'flush_cache',
          'type': 'executable',
          'msvs_guid': '4539AFB3-B8DC-47F3-A491-6DAC8FD26657',
          'dependencies': [
            '../base/base.gyp:base',
            '../base/base.gyp:test_support_base',
          ],
          'sources': [
            'tools/perf/flush_cache/flush_cache.cc',
          ],
        },
      ],
    },],  # OS!="mac"
    ['OS=="linux"',
      { 'targets': [
        {
          'target_name': 'linux_symbols',
          'type': 'none',
          'conditions': [
            ['linux_dump_symbols==1', {
              'actions': [
                {
                  'action_name': 'dump_symbols',
                  'inputs': [
                    '<(DEPTH)/build/linux/dump_app_syms',
                    '<(DEPTH)/build/linux/dump_signature.py',
                    '<(PRODUCT_DIR)/dump_syms',
                    '<(PRODUCT_DIR)/chrome',
                  ],
                  'outputs': [
                    '<(PRODUCT_DIR)/chrome.breakpad.<(target_arch)',
                  ],
                  'action': ['<(DEPTH)/build/linux/dump_app_syms',
                             '<(PRODUCT_DIR)/dump_syms',
                             '<(linux_strip_binary)',
                             '<(PRODUCT_DIR)/chrome',
                             '<@(_outputs)'],
                  'message': 'Dumping breakpad symbols to <(_outputs)',
                  'process_outputs_as_sources': 1,
                },
              ],
              'dependencies': [
                'chrome',
                '../breakpad/breakpad.gyp:dump_syms',
              ],
            }],
          ],
        }
      ],
    },],  # OS=="linux"
    ['OS=="win"',
      { 'targets': [
        {
          # TODO(sgk):  remove this when we change the buildbots to
          # use the generated build\all.sln file to build the world.
          'target_name': 'pull_in_all',
          'type': 'none',
          'dependencies': [
            'installer/mini_installer.gyp:*',
            'installer/installer.gyp:*',
            '../app/app.gyp:*',
            '../base/base.gyp:*',
            '../ipc/ipc.gyp:*',
            '../media/media.gyp:*',
            '../net/net.gyp:*',
            '../printing/printing.gyp:*',
            '../rlz/rlz.gyp:*',
            '../sdch/sdch.gyp:*',
            '../skia/skia.gyp:*',
            '../testing/gmock.gyp:*',
            '../testing/gtest.gyp:*',
            '../third_party/bsdiff/bsdiff.gyp:*',
            '../third_party/bspatch/bspatch.gyp:*',
            '../third_party/bzip2/bzip2.gyp:*',
            '../third_party/cld/cld.gyp:cld',
            '../third_party/codesighs/codesighs.gyp:*',
            '../third_party/icu/icu.gyp:*',
            '../third_party/libjpeg/libjpeg.gyp:*',
            '../third_party/libpng/libpng.gyp:*',
            '../third_party/libxml/libxml.gyp:*',
            '../third_party/libxslt/libxslt.gyp:*',
            '../third_party/lzma_sdk/lzma_sdk.gyp:*',
            '../third_party/modp_b64/modp_b64.gyp:*',
            '../third_party/npapi/npapi.gyp:*',
            '../third_party/sqlite/sqlite.gyp:*',
            '../third_party/zlib/zlib.gyp:*',
            '../webkit/tools/test_shell/test_shell.gyp:*',
            '../webkit/webkit.gyp:*',

            '../build/temp_gyp/googleurl.gyp:*',

            '../breakpad/breakpad.gyp:*',
            '../courgette/courgette.gyp:*',
            '../gears/gears.gyp:*',
            '../rlz/rlz.gyp:*',
            '../sandbox/sandbox.gyp:*',
            '../tools/memory_watcher/memory_watcher.gyp:*',
            '../v8/tools/gyp/v8.gyp:v8_shell',
          ],
          'conditions': [
            ['win_use_allocator_shim==1', {
              'dependencies': [
                '../base/allocator/allocator.gyp:*',
              ],
            }],
            ['chrome_frame_define==1', {
              'dependencies': [
                '../chrome_frame/chrome_frame.gyp:*',
              ],
            }],
          ],
        },
        {
          'target_name': 'chrome_dll_version',
          'type': 'none',
          #'msvs_guid': '414D4D24-5D65-498B-A33F-3A29AD3CDEDC',
          'dependencies': [
            '../build/util/build_util.gyp:lastchange',
          ],
          'direct_dependent_settings': {
            'include_dirs': [
              '<(SHARED_INTERMEDIATE_DIR)/chrome_dll_version',
            ],
          },
          'actions': [
            {
              'action_name': 'version',
              'variables': {
                'lastchange_path':
                  '<(SHARED_INTERMEDIATE_DIR)/build/LASTCHANGE',
                'template_input_path': 'app/chrome_dll_version.rc.version',
              },
              'conditions': [
                [ 'branding == "Chrome"', {
                  'variables': {
                     'branding_path': 'app/theme/google_chrome/BRANDING',
                  },
                }, { # else branding!="Chrome"
                  'variables': {
                     'branding_path': 'app/theme/chromium/BRANDING',
                  },
                }],
              ],
              'inputs': [
                '<(template_input_path)',
                '<(version_path)',
                '<(branding_path)',
                '<(lastchange_path)',
              ],
              'outputs': [
                '<(SHARED_INTERMEDIATE_DIR)/chrome_dll_version/chrome_dll_version.rc',
              ],
              'action': [
                'python',
                '<(version_py_path)',
                '-f', '<(version_path)',
                '-f', '<(branding_path)',
                '-f', '<(lastchange_path)',
                '<(template_input_path)',
                '<@(_outputs)',
              ],
              'message': 'Generating version information in <(_outputs)'
            },
          ],
        },
        {
          'target_name': 'automation',
          'type': '<(library)',
          'msvs_guid': '1556EF78-C7E6-43C8-951F-F6B43AC0DD12',
          'dependencies': [
            'theme_resources',
            '../skia/skia.gyp:skia',
          ],
          'include_dirs': [
            '..',
          ],
          'sources': [
             'test/automation/autocomplete_edit_proxy.cc',
             'test/automation/autocomplete_edit_proxy.h',
             'test/automation/automation_constants.h',
             'test/automation/automation_handle_tracker.cc',
             'test/automation/automation_handle_tracker.h',
             'test/automation/automation_messages.h',
             'test/automation/automation_messages_internal.h',
             'test/automation/automation_proxy.cc',
             'test/automation/automation_proxy.h',
             'test/automation/browser_proxy.cc',
             'test/automation/browser_proxy.h',
             'test/automation/tab_proxy.cc',
             'test/automation/tab_proxy.h',
             'test/automation/window_proxy.cc',
             'test/automation/window_proxy.h',
          ],
        },
        {
          'target_name': 'crash_service',
          'type': 'executable',
          'msvs_guid': '89C1C190-A5D1-4EC4-BD6A-67FF2195C7CC',
          'dependencies': [
            'common_constants',
            '../base/base.gyp:base',
            '../breakpad/breakpad.gyp:breakpad_handler',
            '../breakpad/breakpad.gyp:breakpad_sender',
          ],
          'include_dirs': [
            '..',
          ],
          'sources': [
            'tools/crash_service/crash_service.cc',
            'tools/crash_service/crash_service.h',
            'tools/crash_service/main.cc',
          ],
          'msvs_settings': {
            'VCLinkerTool': {
              'SubSystem': '2',         # Set /SUBSYSTEM:WINDOWS
            },
          },
        },
        {
          'target_name': 'generate_profile',
          'type': 'executable',
          'msvs_guid': '2E969AE9-7B12-4EDB-8E8B-48C7AE7BE357',
          'dependencies': [
            'browser',
            'debugger',
            'renderer',
            'syncapi',
            '../base/base.gyp:base',
            '../skia/skia.gyp:skia',
          ],
          'include_dirs': [
            '..',
          ],
          'sources': [
            'tools/profiles/generate_profile.cc',
            'tools/profiles/thumbnail-inl.h',
          ],
          'conditions': [
            ['OS=="win"', {
              'conditions': [
                ['win_use_allocator_shim==1', {
                  'dependencies': [
                    '<(allocator_target)',
                  ],
                }],
              ],
              'configurations': {
                'Debug_Base': {
                  'msvs_settings': {
                    'VCLinkerTool': {
                      'LinkIncremental': '<(msvs_large_module_debug_link_mode)',
                    },
                  },
                },
              },
            }],
          ],
        },
      ]},  # 'targets'
    ],  # OS=="win"
    ['OS=="linux" or OS=="freebsd"', {
      'targets': [{
        'target_name': 'packed_resources',
        'type': 'none',
        'variables': {
          'repack_path': '../tools/data_pack/repack.py',
        },
        'actions': [
          # TODO(mark): These actions are duplicated for the Mac in the
          # chrome_dll target.  Can they be unified?
          #
          # Mac needs 'process_outputs_as_mac_bundle_resources' to be set,
          # and the option is only effective when the target type is native
          # binary. Hence we cannot build the Mac bundle resources here.
          {
            'action_name': 'repack_chrome',
            'variables': {
              'pak_inputs': [
                '<(grit_out_dir)/browser_resources.pak',
                '<(grit_out_dir)/common_resources.pak',
                '<(grit_out_dir)/renderer_resources.pak',
                '<(grit_out_dir)/theme_resources.pak',
                '<(SHARED_INTERMEDIATE_DIR)/app/app_resources/app_resources.pak',
                '<(SHARED_INTERMEDIATE_DIR)/net/net_resources.pak',
                '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_resources.pak',
              ],
            },
            'inputs': [
              '<(repack_path)',
              '<@(pak_inputs)',
            ],
            'outputs': [
              '<(INTERMEDIATE_DIR)/repack/chrome.pak',
            ],
            'action': ['python', '<(repack_path)', '<@(_outputs)',
                       '<@(pak_inputs)'],
          },
          {
            'action_name': 'repack_locales',
            'variables': {
              'conditions': [
                ['branding=="Chrome"', {
                  'branding_flag': ['-b', 'google_chrome',],
                }, {  # else: branding!="Chrome"
                  'branding_flag': ['-b', 'chromium',],
                }],
              ],
            },
            'inputs': [
              'tools/build/repack_locales.py',
              # NOTE: Ideally the common command args would be shared amongst
              # inputs/outputs/action, but the args include shell variables
              # which need to be passed intact, and command expansion wants
              # to expand the shell variables. Adding the explicit quoting
              # here was the only way it seemed to work.
              '>!@(<(repack_locales_cmd) -i <(branding_flag) -g \'<(grit_out_dir)\' -s \'<(SHARED_INTERMEDIATE_DIR)\' -x \'<(INTERMEDIATE_DIR)\' <(locales))',
            ],
            'outputs': [
              '>!@(<(repack_locales_cmd) -o -g \'<(grit_out_dir)\' -s \'<(SHARED_INTERMEDIATE_DIR)\' -x \'<(INTERMEDIATE_DIR)\' <(locales))',
            ],
            'action': [
              '<@(repack_locales_cmd)',
              '<@(branding_flag)',
              '-g', '<(grit_out_dir)',
              '-s', '<(SHARED_INTERMEDIATE_DIR)',
              '-x', '<(INTERMEDIATE_DIR)',
              '<@(locales)',
            ],
          },
        ],
        # We'll install the resource files to the product directory.
        'copies': [
          {
            'destination': '<(PRODUCT_DIR)/locales',
            'files': [
              '>!@(<(repack_locales_cmd) -o -g \'<(grit_out_dir)\' -s \'<(SHARED_INTERMEDIATE_DIR)\' -x \'<(INTERMEDIATE_DIR)\' <(locales))',
            ],
          },
          {
            'destination': '<(PRODUCT_DIR)',
            'files': [
              '<(INTERMEDIATE_DIR)/repack/chrome.pak'
            ],
          },
        ],
      }],  # targets
    }],  # OS=="linux" or OS=="freebsd"
  ],  # 'conditions'
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
