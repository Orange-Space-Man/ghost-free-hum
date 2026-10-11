from pathlib import Path
import re

root = Path(__file__).resolve().parents[2]
port = root / 'ghost-free-hum'

def remove_function(text, start):
    begin = text.index(start)
    end = text.index('{', begin) + 1
    depth = 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[:begin] + text[end:]

scene = port / 'src/game/scene/gameplayscene.cpp'
scene.write_text(remove_function(scene.read_text(), 'std::string gameplay::Scene::debugInfo()'))
header = scene.with_suffix('.h')
header.write_text(header.read_text().replace('        std::string debugInfo();\n', ''))

# Keep measurement in the external capture driver, not inside every shipping draw.
opening = port / 'src/game/startup/openingscene.cpp'
text = opening.read_text()
text = re.sub(r'^.*const auto (?:begin|animated|rendered) = std::chrono::steady_clock::now\(\);\n', '', text, flags=re.M)
text = re.sub(r'    renderMilliseconds = \{.*?\};\n', '', text, flags=re.S)
text = text.replace('#include <chrono>\n', '')
opening.write_text(text)
header = opening.with_suffix('.h')
header.write_text(header.read_text().replace('        std::array<double, 2> renderMilliseconds{};\n', ''))
capture = root / 'ghost-free-hum-validation/tools/captureapplication.cpp'
text = capture.read_text()
text = text.replace('                        timings << "Frame " << frame << " animation ms " << opening.renderMilliseconds[0]\n                            << " rendering submission ms " << opening.renderMilliseconds[1]\n                            << " complete frame ms "', '                        timings << "Frame " << frame << " complete frame ms "')
capture.write_text(text)

for path in (port / 'src').rglob('*.cpp'):
    text = path.read_text()
    if path.name in ('playercontrollerstate.cpp', 'gamecardstorage.cpp'):
        text = text.replace('#define NOMINMAX\n', '')
    # Merged fragments belong to one translation unit with one include block.
    if path.name in ('actorregistry.cpp', 'moveactor.cpp', 'playeractor.cpp', 'enemystrategy.cpp',
                     'enemyjewelstrategy.cpp', 'jaudioeffectconfig.cpp', 'j3dmodelinstance.cpp'):
        includes = re.findall(r'^#include [^\n]+', text, re.M)
        includes = list(dict.fromkeys(includes))
        text = re.sub(r'^#include [^\n]+\n', '', text, flags=re.M)
        local = [line for line in includes if '"' in line]
        standard = [line for line in includes if '<' in line]
        text = '\n'.join(local) + '\n\n' + '\n'.join(standard) + '\n\n' + text.lstrip()
    text = re.sub(r'\n{4,}', '\n\n\n', text)
    if text != path.read_text(): path.write_text(text)

# These recovered parameter roles are established by the consuming calculations.
path = port / 'src/game/player/playerorientationframe.cpp'
text = path.read_text()
enum = '''    enum Parameter : unsigned {
        reverseProjectionThreshold = 1,
        movementThreshold = 2,
        movementUpperThreshold = 3,
        turnLimit = 7,
        maximumSpeed = 8,
        alternateMaximumSpeed = 9,
        acceleration = 10,
        primaryStateAcceleration = 11,
        equipmentAcceleration = 12,
        scannerAcceleration = 13,
        secondaryStateAcceleration = 14,
        primaryStateDamping = 15,
        secondaryStateDamping = 16,
        defaultBlend = 29,
        targetSpeedMultiplier = 32
    };

'''
text = text.replace('namespace {\n', 'namespace {\n' + enum, 1)
names = dict((int(index), name) for name, index in re.findall(r'        (\w+) = (\d+)', enum))
text = re.sub(r'constants\[(\d+)\]', lambda match: 'constants[' + names[int(match[1])] + ']', text)
text = text.replace('constants[state.flags134C[1] ? 9 : 8]', 'constants[state.flags134C[1] ? alternateMaximumSpeed : maximumSpeed]')
path.write_text(text)
print('Removed retired overlay/profiling code and organized merged implementations.')
