from pathlib import Path
import re
import textwrap
import json

root = Path(__file__).resolve().parents[2]
port = root / 'ghost-free-hum'
validation = root / 'ghost-free-hum-validation'
source = {p.name: p.read_text(encoding='utf-8-sig') for p in (root / 'src').iterdir() if p.suffix in ('.cpp', '.h')}
original = dict(source)

def write(path, text):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text.rstrip() + '\n', encoding='utf-8')

def remove_block(text, start):
    begin = text.index(start)
    brace = text.index('{', begin)
    depth = 1
    end = brace + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[:begin] + text[end:], text[begin:end]

# Separate executable resource access from the title's resource loader.
startup, reader = remove_block(source['startupresources.cpp'], 'bool startupresources::readDolRange(')
source['startupresources.cpp'] = startup.replace('readDolRange(', 'dolresources::read(')
helpers = original['startupresources.cpp'].split('namespace {', 1)[1].split('\n}\n', 1)[0]
helpers = helpers[:helpers.index('    std::string tag(')]
source['dolresources.cpp'] = '#include "dolresources.h"\n#include <stdexcept>\n\nnamespace {' + helpers + '\n}\n\n' + reader.replace('startupresources::readDolRange', 'dolresources::read')
source['dolresources.h'] = '''#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace dolresources {
    bool read(const std::vector<std::uint8_t>& executable, std::uint32_t address,
        std::size_t size, std::vector<std::uint8_t>& output, std::string& error);
}
'''
source['startupresources.h'] = re.sub(r'    bool readDolRange\([^;]+;\n', '', source['startupresources.h'])
for name, text in list(source.items()):
    if 'startupresources::readDolRange' in text:
        text = text.replace('startupresources::readDolRange', 'dolresources::read')
        text = '#include "dolresources.h"\n' + text
    if name == 'startupresources.cpp':
        text = '#include "dolresources.h"\n' + text
    if name.startswith(('jaudio', 'j3d')) and 'startupresources::' not in text:
        text = text.replace('#include "startupresources.h"\n', '')
    source[name] = text

# Move the game session out of the executable entry point so checks call the same implementation.
app = source.pop('polterportapplication.cpp')
app = app.replace('#include "debugoverlay.h"\n', '').replace('#include "mastersettings.h"\n', '')
app = re.sub(r'        std::string information =.*?        debugoverlay::setScene\(information\);\n', '', app, flags=re.S)
app = app.replace('!mastersettings::capturesInput()', 'true')
app = re.sub(r'            const double position =.*?            opening.draw\(present, interpolation\);', '            opening.draw(present);', app, flags=re.S)
app = app.replace('        mastersettings::hide();\n', '')
app, _ = remove_block(app, '        if (mastersettings::handleMessage(')
app = re.sub(r'^.*mastersettings::load.*\n', '', app, flags=re.M)
app = app.replace(', mastersettings::restore30Fps()', '')
app = app.replace('PolterportApplication', 'GhostFreeHumApplication').replace('Polterport', 'ghost-free-hum')
includes, rest = app.split('namespace {\n', 1)
fields, rest = rest.split('    void prepareOpening(', 1)
functions = '    void prepareOpening(' + rest.split('    LRESULT CALLBACK windowProcedure', 1)[0]
declarations = []
for match in re.finditer(r'^    (void|LRESULT) (\w+)\(([^\n]*)\) \{', functions, re.M):
    declarations.append('        ' + match.group(1) + ' ' + match.group(2) + '(' + match.group(3) + ');')
functions = textwrap.dedent(functions)
functions = re.sub(r'^(void|LRESULT) (\w+)\(', r'\1 gameapplication::Application::\2(', functions, flags=re.M)
functions = functions.replace('unsigned cutscene = 0x4c', 'unsigned cutscene').replace('unsigned destination = 1', 'unsigned destination')
header_includes = '\n'.join(line for line in includes.splitlines() if line.startswith('#include "'))
source['gameapplication.h'] = '#pragma once\n\n' + header_includes + '''
#include <windows.h>

namespace gameapplication {
    class Application {
    public:
        void initialize(HINSTANCE instance, const std::filesystem::path& assetPath, bool live, bool preload);
        int run(int show);
''' + '\n'.join(declarations) + '\n\n    protected:\n' + textwrap.indent(textwrap.dedent(fields), '        ') + '        HWND window = nullptr;\n    };\n}\n'
initial = app[app.index('        currentAssets = assetPath;'):app.index('        if (!capturePath.empty()) {\n            if (captureNewGameFlow)')]
initial = initial.replace('if (capturePath.empty())', 'if (live)').replace('audioEnabled = capturePath.empty();', 'audioEnabled = live;')
initial = initial.replace('HWND window = CreateWindowW', 'window = CreateWindowW')
initial = initial.replace('        if (captureDebug) SendMessageW(window, WM_KEYDOWN, VK_F2, 0);\n', '')
initial = initial.replace('if (capturePath.empty() || captureOpeningFlow || captureMenuFlow)', 'if (preload)')
initial = initial.replace('windowClass.lpfnWndProc = windowProcedure;', 'activeApplication = this;\n        windowClass.lpfnWndProc = windowProcedure;')
loop = app[app.index('        ShowWindow(window, show);'):app.index('    } catch (const std::exception& exception) {', app.index('        ShowWindow(window, show);'))]
loop = loop.replace('1.0 / 60.0', '1.0 / gameframepacing::hostTicksPerSecond')
source['gameapplication.cpp'] = '#include "gameapplication.h"\n' + '\n'.join(line for line in includes.splitlines() if line.startswith('#include <')) + '''

namespace {
    gameapplication::Application* activeApplication = nullptr;

    LRESULT CALLBACK windowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
        try {
            return activeApplication->handleMessage(window, message, wParam, lParam);
        } catch (const std::exception& exception) {
            activeApplication->close();
            MessageBoxA(window, exception.what(), "ghost-free-hum", MB_OK | MB_ICONERROR);
            DestroyWindow(window);
            return 0;
        }
    }
}

''' + functions + '\nvoid gameapplication::Application::initialize(HINSTANCE instance, const std::filesystem::path& assetPath, bool live, bool preload) {\n' + textwrap.indent(textwrap.dedent(initial), '    ') + '}\n\nint gameapplication::Application::run(int show) {\n' + textwrap.indent(textwrap.dedent(loop), '    ') + '    return 0;\n}\n'
source['ghostfreehum.cpp'] = '''#include "gameapplication.h"

#include <shellapi.h>
#include <stdexcept>

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    gameapplication::Application application;
    try {
        std::filesystem::path assets;
        int count = 0;
        auto arguments = CommandLineToArgvW(GetCommandLineW(), &count);
        if (arguments) {
            const bool valid = count == 1 || (count == 3 && std::wstring(arguments[1]) == L"--assets");
            if (count == 3 && valid) assets = arguments[2];
            LocalFree(arguments);
            if (!valid) throw std::runtime_error("Expected --assets followed by an imported package path.");
        }
        if (assets.empty()) assets = gameassetsetup::prepare();
        if (assets.empty()) return 0;
        application.initialize(instance, assets, true, true);
        return application.run(show);
    } catch (const std::exception& exception) {
        application.close();
        MessageBoxA(nullptr, exception.what(), "ghost-free-hum", MB_OK | MB_ICONERROR);
        return 1;
    }
}
'''
capture = app[app.index('int WINAPI wWinMain'):]
capture = capture.replace('int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show)', 'int CaptureApplication::run(HINSTANCE instance)')
start = capture.index('        currentAssets = assetPath;')
end = capture.index('        if (!capturePath.empty()) {\n            if (captureNewGameFlow)')
capture = capture[:start] + '        initialize(instance, assetPath, false, captureOpeningFlow || captureMenuFlow);\n        std::string error;\n' + capture[end:]
start = capture.index('        ShowWindow(window, show);')
end = capture.index('    } catch (const std::exception& exception) {', start)
capture = capture[:start] + '        throw std::runtime_error("A capture option is required.");\n' + capture[end:]
for flag, following in [('--debug-overlay', '--capture-settings'), ('--capture-settings', '--capture-opening'), ('--opening-interpolation', '--capture-menu-flow')]:
    start = capture.index('                } else if (value == L"' + flag + '"')
    end = capture.index('                } else if (value == L"' + following + '"', start)
    capture = capture[:start] + capture[end:]
capture = re.sub(r'^.*(?:float openingInterpolation|bool captureSettings|bool captureDebug).*\n', '', capture, flags=re.M)
capture, _ = remove_block(capture, '            if (captureSettings) {')
capture = capture.replace('opening.draw(false, openingInterpolation)', 'opening.draw(false)').replace('opening.draw(false, 0.5f)', 'opening.draw(false)')
capture = capture.replace('                SendMessageW(window, WM_KEYDOWN, VK_OEM_3, 0);\n', '')
capture = '#include "gameapplication.h"\n' + '\n'.join(line for line in includes.splitlines() if line.startswith('#include <')) + '''

class CaptureApplication : public gameapplication::Application {
public:
    int run(HINSTANCE instance);
};

''' + capture + '''
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    CaptureApplication application;
    return application.run(instance);
}
'''

for name in ('mastersettings.cpp', 'mastersettings.h', 'debugoverlay.cpp', 'debugoverlay.h'):
    source.pop(name)
source['gamerenderer.cpp'] = re.sub(r'^.*mastersettings.*\n', '', source['gamerenderer.cpp'], flags=re.M)
source['gameframepacing.h'] = '''#pragma once

namespace gameframepacing {
    inline constexpr unsigned presentationFramesPerSecond = 30;
    inline constexpr unsigned hostTicksPerSecond = 60;

    class Clock {
    public:
        bool ready(double seconds);

    private:
        double next = 0;
        bool started = false;
    };
}
'''
source['gameframepacing.cpp'] = '''#include "gameframepacing.h"

#include <cmath>
#include <stdexcept>

bool gameframepacing::Clock::ready(double seconds) {
    if (!std::isfinite(seconds)) throw std::runtime_error("Invalid presentation time.");
    if (!started) {
        next = seconds;
        started = true;
    }
    if (seconds < next) return false;
    constexpr double period = 1.0 / presentationFramesPerSecond;
    next += (std::floor((seconds - next) / period) + 1) * period;
    return true;
}
'''
source['openingscene.h'] = source['openingscene.h'].replace('bool present, float interpolation = 0', 'bool present')
source['openingscene.cpp'] = source['openingscene.cpp'].replace('bool present, float interpolation', 'bool present')
source['openingscene.cpp'] = source['openingscene.cpp'].replace('    interpolation = std::clamp(interpolation, 0.0f, 1.0f);\n    const float step = interpolation * timeline.animationStep;\n    const float sceneFrame = timeline.drawFrame + ((timeline.flags & 3) ? 0 : step);', '    const float sceneFrame = timeline.drawFrame;')
source['openingscene.cpp'] = source['openingscene.cpp'].replace(', step)', ', 0)').replace('state.drawFrame + step', 'state.drawFrame')

# Fold implementation-only fragments into their owners without changing header APIs.
merges = {
    'actorregistry.cpp': ['actorregistrycontacts.cpp', 'actorregistrypose.cpp'],
    'moveactor.cpp': ['actorframemotion.cpp'],
    'playeractor.cpp': ['playeractorstate.cpp'],
    'enemystrategy.cpp': ['enemystrategydecorator.cpp', 'enemynomovestrategy.cpp'],
    'enemyjewelstrategy.cpp': ['enemygoldstrategy.cpp', 'enemycrownstrategy.cpp'],
    'jaudioeffectconfig.cpp': ['jaudioeffectparameter.cpp'],
    'j3dmodelinstance.cpp': ['j3ddrawbuffer.cpp'],
}
for destination, fragments in merges.items():
    for fragment in fragments:
        source[destination] += '\n' + source.pop(fragment)

resources = {'gameassetpackage', 'roomdiscreader', 'roomarchivereader', 'dolresources'}
platform = {'gameapplication', 'ghostfreehum', 'gameassetsetup', 'gameframepacing', 'startupaudio', 'windowpadinput'}
renderers = {'gamerenderer', 'gameshadowrenderer', 'modeltexturerenderer', 'modelgeometryrenderer', 'modelmaterialrenderer', 'modelrenderstate', 'modelbatchrenderer'}

def directory(name):
    stem = Path(name).stem
    if stem in resources: return 'resources'
    if stem in platform: return 'platform/windows'
    if stem in renderers: return 'platform/d3d11'
    if stem.startswith('j3d'): return 'jlib/j3d'
    if stem.startswith('jaudio') and stem not in ('jaudiopositionowners', 'jaudiopositionalplayback'): return 'jlib/jaudio'
    if stem.startswith('jaudio'): return 'game/audio'
    if stem.startswith('player'): return 'game/player'
    if stem.startswith(('enemy', 'item')): return 'game/enemies'
    if stem.startswith(('actor', 'moveactor', 'character')): return 'game/actors'
    if stem.startswith(('particle', 'element')) or 'effect' in stem: return 'game/effects'
    if stem.startswith(('room', 'door', 'mapcollision', 'rotating', 'cobwebplacement')): return 'game/world'
    if stem.startswith(('s3d', 'model')): return 'game/models'
    if stem.startswith(('fileselect', 'menulayout', 'gamefont')): return 'game/menu'
    if stem.startswith(('opening', 'startup', 'scenetransition')): return 'game/startup'
    if stem.startswith(('jmp', 'event', 'mission', 'runtimeflags')): return 'game/events'
    if stem.startswith(('gamesave', 'gamecard')): return 'game/save'
    if stem.startswith('controller'): return 'game/input'
    if stem.startswith('originalfloat'): return 'game/math'
    return 'game/scene'

paths = {name: directory(name) + '/' + name for name in source}
def includes_for(text):
    return re.sub(r'#include "([^"/]+)"', lambda m: '#include "' + paths.get(m[1], m[1]) + '"', text)

for name, text in source.items():
    write(port / 'src' / paths[name], includes_for(text.replace('PolterportAssetImport', 'GhostFreeHumAssetImport').replace('Polterport', 'ghost-free-hum')))
for test in (root / 'tests').glob('*.cpp'):
    if test.stem == 'mastersettingstests': continue
    text = test.read_text(encoding='utf-8-sig')
    if 'startupresources::readDolRange' in text:
        text = '#include "dolresources.h"\n' + text.replace('startupresources::readDolRange', 'dolresources::read')
    if test.stem == 'gameframepacingtests':
        text = text.replace('for (bool restore : {false, true})', 'for (unsigned pass = 0; pass < 2; ++pass)').replace(', restore)', ')').replace('(restore ? 300U : 600U)', '300U')
        text = re.sub(r'^.*!restore.*\n', '', text, flags=re.M)
    write(validation / 'tests' / test.name, includes_for(text))
write(validation / 'tools/captureapplication.cpp', includes_for(capture))
write(validation / 'tools/actorstrategycoverage.cpp', includes_for((root / 'tools/actorstrategycoverage.cpp').read_text()))

runtime = platform | renderers | {'fileselectscene', 'openingscene', 'gameplayscene'}
groups = {'ResourceIO': [], 'JLib': [], 'Game': [], 'WindowsRuntime': []}
for name in source:
    if not name.endswith('.cpp') or name == 'ghostfreehum.cpp': continue
    stem = Path(name).stem
    target = 'ResourceIO' if stem in resources else 'JLib' if paths[name].startswith('jlib/') else 'WindowsRuntime' if stem in runtime else 'Game'
    groups[target].append('src/' + paths[name])
cmake = '''cmake_minimum_required(VERSION 3.24)
project(ghost-free-hum VERSION 0.2.0 LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "The current platform implementation requires Windows and Direct3D 11.")
endif()
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

'''
for target, files in groups.items():
    cmake += 'add_library(' + target + ' STATIC\n' + ''.join('    ' + path + '\n' for path in sorted(files)) + ')\n\n'
cmake += '''target_link_libraries(JLib PUBLIC ResourceIO)
target_link_libraries(Game PUBLIC JLib)
target_link_libraries(WindowsRuntime PUBLIC Game comdlg32 user32 gdi32 d3d11 dxgi d3dcompiler shell32 xaudio2 ole32 xinput9_1_0)

foreach(target ResourceIO JLib Game WindowsRuntime)
    target_include_directories(${target} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_compile_features(${target} PUBLIC cxx_std_17)
    set_target_properties(${target} PROPERTIES CXX_EXTENSIONS OFF)
    target_compile_definitions(${target} PUBLIC UNICODE _UNICODE NOMINMAX)
    if(MSVC)
        target_compile_options(${target} PUBLIC /EHsc /fp:precise)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
    endif()
endforeach()

add_executable(ghost-free-hum WIN32 src/platform/windows/ghostfreehum.cpp)
target_link_libraries(ghost-free-hum PRIVATE WindowsRuntime)
set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT ghost-free-hum)
install(TARGETS ghost-free-hum RUNTIME DESTINATION .)
'''
write(port / 'CMakeLists.txt', cmake)
oldcmake = (root / 'CMakeLists.txt').read_text()
tests = re.findall(r'add_test\(NAME (\w+) COMMAND (\w+)\)', oldcmake)
testtargets = sorted({p.stem for p in (validation / 'tests').glob('*.cpp')})
cmake = '''cmake_minimum_required(VERSION 3.24)
project(ghost-free-hum-validation LANGUAGES CXX)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(PORT_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../ghost-free-hum" CACHE PATH "Clean port source directory")
add_subdirectory("${PORT_SOURCE_DIR}" port)
enable_testing()

'''
for target in testtargets:
    cmake += f'add_executable({target} tests/{target}.cpp)\ntarget_link_libraries({target} PRIVATE WindowsRuntime)\n'
for name, target in tests:
    if target in testtargets: cmake += f'add_test(NAME {name} COMMAND {target})\n'
cmake += '''
add_executable(scene-capture WIN32 tools/captureapplication.cpp)
target_link_libraries(scene-capture PRIVATE WindowsRuntime)
add_executable(actorstrategycoverage tools/actorstrategycoverage.cpp)
target_link_libraries(actorstrategycoverage PRIVATE WindowsRuntime)
'''
write(validation / 'CMakeLists.txt', cmake)
write(validation / 'migration-map.json', json.dumps({'sourcePaths': paths, 'mergedImplementations': merges}, indent=2))
notices = (root / 'THIRD_PARTY_NOTICES.txt').read_text()
notices = re.sub(r'Dear ImGui.*?license and copyright notice: third_party/imgui/LICENSE.txt\.\n\n', '', notices, flags=re.S | re.I)
write(port / 'THIRD_PARTY_NOTICES.txt', notices.replace('Polterport', 'ghost-free-hum'))
agents = (root / 'AGENTS.md').read_text().replace('Polterport', 'ghost-free-hum')
agents = re.sub(r'- Target 60 fps.*\n', '- Target original 30 fps presentation. Preserve simulation, event, animation and audio clocks. Do not reintroduce ImGui or enhancement settings during the faithful-port cleanup.\n', agents)
agents = re.sub(r'- Evolve this existing project.*\n', '- Work in this clean port. Keep tests and developer capture tools in the sibling ghost-free-hum-validation project. Keep the old LMD sources untouched as a reference. Follow original boot-to-title and manager dependencies.\n', agents)
write(port / 'AGENTS.md', agents)
write(port / '.gitignore', '/build/\n/out/\n/.vs/\n/CMakeUserPresets.json\n/assets/\n/saves/\n*.iso\n*.gcm\n*.ppak\n*.gci\n*.exe\n*.pdb\n*.obj\n*.lib\n*.log\n')
write(validation / '.gitignore', '/build/\n/captures/\n/logs/\n*.ppak\n*.iso\n*.gci\n')
print(f'Created {len(source)} source/header files, {len(testtargets)} validation targets; original project untouched.')
