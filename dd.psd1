@{
    schema = 1
    project = @{
        name = 'potato'
        type = 'gui'
        'default-target' = 'app'
    }
    dependencies = @{ owner = 'dd' }
    build = @{
        'x64-windows' = @{
            debug = 'debug'
            release = 'release'
        }
    }
    targets = @(
        @{
            id = 'lib'
            kind = 'library'
            'cmake-target' = 'webvis'
            'test-label' = 'potato'
            'debug-path' = 'build/debug/{libprefix}webvis{lib}'
            'release-path' = 'build/release/{libprefix}webvis{lib}'
            platforms = @('x64-windows')
        },
        @{
            id = 'app'
            kind = 'gui'
            'cmake-target' = 'potato'
            'test-label' = 'potato'
            'debug-path' = 'Exe/potato-64d{exe}'
            'release-path' = 'Exe/potato-64{exe}'
            platforms = @('x64-windows')
        }
    )
    commands = @{
        layout = @{
            description = "Run the layout engine over an HTML file in the project and report the box tree. Offline."
            script = 'cmake/project-command.ps1'
            effects = 'read'
            'supports-dry-run' = $true
            'timeout-secs' = 600
            parameters = @{
                file = @{ type = 'string'; description = 'HTML file inside the project.' }
                width = @{ type = 'integer'; description = 'Viewport width in pixels.' }
                dump = @{ type = 'integer'; description = 'Dump depth.' }
                trace = @{ type = 'boolean'; description = 'Verbose layout trace (-v).' }
                config = @{ type = 'string'; default = 'release'; choices = @('debug', 'release') }
            }
        }
        'analyze-wiki-css' = @{
            description = "Fetch Wikipedia's vector-2022 stylesheet and extract its :root custom properties. Network."
            script = 'cmake/project-command.ps1'
            effects = 'read'
            'supports-dry-run' = $true
            'timeout-secs' = 300
            parameters = @{}
        }
    }
}
