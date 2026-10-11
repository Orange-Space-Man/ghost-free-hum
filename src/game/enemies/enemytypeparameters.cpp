#include "game/enemies/enemytypeparameters.h"
#include "resources/roomarchivereader.h"
#include "game/enemies/enemytypecatalog.h"

#include <stdexcept>

gameparameters::Parameters enemytypes::defaults() {
    gameparameters::Parameters result;
    result.add("mLife", 4, 0);
    result.add("mHitDamage", 4, 0);
    result.add("mSpeed", 4, 0);
    result.add("mSpeedUnseen", 4, 0);
    result.add("mSpeedFight", 4, 0);
    result.add("mEyesight", 4, 0);
    result.add("mLightBindFrame", 4, 10);
    result.add("mMinLightBindRange", 4, 0x42c80000);
    result.add("mMaxLightBindRange", 4, 0x43c80000);
    result.add("mNumAtkKarakai", 4, 0);
    result.add("mNumAtkOrooro", 4, 0);
    result.add("mHikiPower", 4, 0);
    result.add("mEffectiveDeg", 4, 0);
    result.add("mTsuriHeight", 4, 0xbf800000);
    result.add("mDisappearFrame", 4, 0);
    result.add("mAttackPattern1", 4, 4);
    result.add("mActAfterAttackSu", 4, 1);
    result.add("mActAfterAttackFa", 4, 1);
    result.add("mTsuriType", 4, 0);
    result.add("mElement", 4, 0);
    result.add("mCheckBox", 2, 0);
    result.add("mNormalItemTblId", 4, 0);
    result.add("mTsuriItemTblId", 4, 0);
    result.add("mPointerRange", 4, 0);
    result.add("mBrightColor", 4, 0xffffffff);
    result.add("mAmbColor", 4, 0x404040ff);
    return result;
}

enemytypes::Manager::Manager() {
    for (auto& type : types) type = defaults();
}

const gameparameters::Parameters& enemytypes::Manager::parameters(unsigned type) const { return types.at(type); }

bool enemytypes::Manager::loadParameters(const std::vector<std::uint8_t>& executable,
    const std::vector<std::uint8_t>& archive, std::string& error) {
    error.clear();
    try {
        Catalog catalog;
        if (!catalog.read(executable, error)) return false;
        for (unsigned type = 0; type < types.size(); ++type) {
            std::vector<std::uint8_t> data;
            const std::string path = "param/ctp/" + catalog.name(type) + ".prm";
            bool missing = false;
            if (roomarchive::read(archive, path, data, error, &missing)) {
                if (!types[type].load(data, error)) return false;
            } else {
                if (!missing) return false;
                if ((catalog.flags(type) & 6) == 2) types[type].setBits(25, 0x323232ff);
                error.clear();
            }
        }
        return true;
    } catch (const std::exception& exception) { error = exception.what(); return false; }
}
