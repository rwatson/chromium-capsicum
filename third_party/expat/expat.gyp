# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'target_defaults': {
    'defines': [
      '_LIB',
      'XML_STATIC',  # Compile for static linkage.
    ],
    'include_dirs': [
      'files/lib',
    ],
    'dependencies': [
    ]
  },
  'conditions': [
    ['OS=="linux" or OS=="freebsd"', {
      # On Linux, we implicitly already depend on expat via fontconfig;
      # let's not pull it in twice.
      'targets': [
        {
          'target_name': 'expat',
          'type': 'settings',
          'link_settings': {
            'libraries': [
              '-lexpat',
            ],
          },
        },
      ],
    }, {  # OS != linux
      'targets': [
        {
          'target_name': 'expat',
          'type': '<(library)',
          'sources': [
            'files/lib/expat.h',
            'files/lib/xmlparse.c',
            'files/lib/xmlrole.c',
            'files/lib/xmltok.c',
          ],

          # Prefer adding a dependency to expat and relying on the following
          # direct_dependent_settings rule over manually adding the include
          # path.  This is because you'll want any translation units that
          # #include these files to pick up the #defines as well.
          'direct_dependent_settings': {
            'include_dirs': [
              'files/lib'
            ],
            'defines': [
              'XML_STATIC',  # Tell dependants to expect static linkage.
            ],
          },
          'conditions': [
            ['OS=="win"', {
              'defines': [
                'COMPILED_FROM_DSP',
              ],
            }],
            ['OS=="mac" or OS=="freebsd"', {
              'defines': [
                'HAVE_EXPAT_CONFIG_H',
              ],
            }],
          ],
        },
      ],
    }],
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
