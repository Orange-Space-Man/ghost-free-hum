from pathlib import Path
import re

root = Path(__file__).resolve().parents[2]
port = root / 'ghost-free-hum'
validation = root / 'ghost-free-hum-validation'

addresses = {
    'sineTable': 0x80218238, 'cosineTable': 0x80219238, 'arctangentTable': 0x8021d238,
    'titleArchive': 0x8021e000, 'initialSaveFlags': 0x8020c128, 'memoryCardBanner': 0x80335d60,
    'fontResource': 0x80306560, 'fontAsciiTable': 0x80394188,
    'trigReductionTable': 0x80217c28, 'trigPolynomialTable': 0x80218040, 'trigConstants': 0x804dd900,
    'actorModelTable': 0x8034e46c, 'actorAuxiliaryTable': 0x80389b68, 'actorKeyTable': 0x80215910,
    'actorTypeTable': 0x8033b590, 'actorAnimationTable': 0x8034a6c4,
    'collisionInitialPrimary': 0x8020c7b8, 'collisionInitialSecondary': 0x8020c7cc,
    'collisionInitialExtended': 0x8020c7e0, 'collisionModelTable': 0x8038dd00, 'collisionTypeTable': 0x8038e118,
    'characterSavedTypes': 0x80338580, 'enemyCreationPools': 0x80360db0, 'enemyCreationCounts': 0x80363b50,
    'enemyConstructors': 0x80364260, 'strategyTypes': 0x80364258, 'strategyFactories': 0x80365a60,
    'doorModels': 0x802ff868, 'doorAnimations': 0x802ff944, 'doorConfigurations': 0x802ff95c,
    'doorCollisionBoxes': 0x802ffa88, 'doorConfigurationFlags': 0x802ff96e,
    'fileDetailDigits': 0x803343c4, 'fileDetailAreas': 0x8033455c,
    'j3dDefaultFormats': 0x80392d10, 'j3dDefaultRegisters': 0x804d7b08, 'j3dDefaultColors': 0x804dd5b0,
    'voicePredictorCoefficients': 0x80390be0, 'dspResamplingTable': 0x80390c20,
    'voiceChannelAndOutputLevels': 0x804dd168, 'voiceSineConstants': 0x804dd388,
    'voicePitchTable': 0x80391020, 'voiceFinePitchTable': 0x80391948, 'voiceVibratoParameters': 0x80391a48,
    'streamPredictorCoefficients': 0x80391b70, 'streamVolumeDivisor': 0x804dd17c, 'streamOutputLevel': 0x804dd16c,
    'playerTurningParameters': 0x804d2b30, 'playerTurnCompletionThreshold': 0x804db5b8,
    'nullCharacterConstructor': 0x800ebd50, 'ordinaryCharacterConstructor': 0x800ebd90,
    'barrierFactory': 0x800efc28, 'batGeneratorFactory': 0x800ef0c4, 'alternateBatGeneratorFactory': 0x800ef0fc,
    'goldRatGeneratorFactory': 0x800eeec8, 'fireGeneratorFactory': 0x800ef864, 'fakeDoorFactory': 0x800efff8,
    'roomObserverFactory': 0x800ef388, 'bombFactory': 0x800f06cc, 'heartEscapeFactory': 0x800f0854,
    'negativeRandomFactory': 0x800f081c, 'elementJewelFactory': 0x800f065c, 'goldFactory': 0x800f0ee8,
    'pearlFactory': 0x800f0694, 'crownFactory': 0x800f08fc, 'jewelFactory': 0x800eff18,
    'negativeItemFactory': 0x800f09a4, 'heartItemFactory': 0x800f0dd0, 'keyFactory': 0x800eee90,
    'itemFactory': 0x800f0f58, 'noMoveFactory': 0x800f13d4, 'spearFactory': 0x800f0e40,
    'pathConditionFactory': 0x800f0d98, 'generatorDoorFactory': 0x800f05b4,
    'immediateConditionFactory': 0x800f0110, 'roomEmptyConditionFactory': 0x800f0d60,
    'roomClearedConditionFactory': 0x800f0bd8, 'playerInRoomConditionFactory': 0x800f0b30,
    'namedAbsentConditionFactory': 0x800f0b68, 'occupiedUnclearedRoomConditionFactory': 0x800f0abc,
    'visibleFlagConditionFactory': 0x800f0cb8, 'flagOnConditionFactory': 0x800f08c4, 'flagOffConditionFactory': 0x800f088c,
}
lookup = {address: name for name, address in addresses.items()}
header = '#pragma once\n\n#include <cstdint>\n\nnamespace glme01 {\n'
header += ''.join(f'    inline constexpr std::uint32_t {name} = 0x{address:08x};\n' for name, address in addresses.items())
header += '}\n'
(port / 'src/resources/glme01addresses.h').write_text(header)

names = {'fieldD0': 'animationFrame', 'fieldD4': 'animationFrameStep', 'fieldD8': 'animationSpeed',
         'fieldDC': 'blendFramesRemaining', 'fieldDE': 'blendFramesTotal'}
files = list((port / 'src').rglob('*.cpp')) + list((port / 'src').rglob('*.h'))
files += list((validation / 'tests').glob('*.cpp')) + list((validation / 'tools').glob('*.cpp'))
replacements = 0
for path in files:
    if path.name == 'glme01addresses.h': continue
    text = path.read_text()
    old = text
    def replace_address(match):
        global replacements
        value = int(match[0], 16)
        if value not in lookup: return match[0]
        replacements += 1
        return 'glme01::' + lookup[value]
    text = re.sub(r'\b0x[0-9a-fA-F]{8}\b', replace_address, text)
    if text != old: text = '#include "resources/glme01addresses.h"\n' + text
    for before, after in names.items():
        text = re.sub(r'\b' + before + r'\b', after, text)
    if path.name in ('moveactor.cpp', 'moveactor.h'):
        text = re.sub(r'\bfieldCC\b', 'animationClock', text)
        text = re.sub(r'\bfieldAC\b', 'poseLinkedActor', text)
    if path.name.endswith('tests.cpp'):
        text = re.sub(r'\b(actor|linked|player)(\.|->)fieldAC\b', r'\1\2poseLinkedActor', text)
        text = re.sub(r'\b(actor|player)(\.|->)fieldCC\b', r'\1\2animationClock', text)
    if text != old: path.write_text(text)

print(f'Named {len(addresses)} recovered addresses across {replacements} references; renamed actor animation fields.')
