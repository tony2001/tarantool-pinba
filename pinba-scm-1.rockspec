package = 'pinba'
version = 'scm-1'
source  = {
    url    = 'git://github.com/tony2001/tarantool-pinba.git',
    branch = 'master',
}
description = {
    summary  = "Tarantool module for Pinba",
    homepage = 'https://github.com/tony2001/tarantool-pinba.git',
    license  = 'BSD',
}
dependencies = {
    'lua >= 5.1'
}
external_dependencies = {
    TARANTOOL = {
        header = "tarantool/module.h"
    }
}
build = {
    type = 'builtin',

    modules = {
        ['pinba.lib'] = {
            sources = { 'pinba/lib.c', 'pinba/pinba.pb-c.c', 'pinba/protobuf-c.c' },
            incdirs = {
                "$(TARANTOOL_INCDIR)"
            }
        },
        ['pinba.client'] = 'pinba/client.lua',
    }
}

-- vim: syntax=lua
