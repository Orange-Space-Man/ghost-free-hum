#include "moneyscatterframe.h"

void moneyscatter::updateFrame(Owner& coins, billscatter::Scene& bills, leafscatter::Scene& leaves,
    const Vector& cameraPosition, const Vector& cameraDirection) {
    coins.updateCoins(cameraPosition, cameraDirection);
    bills.update();
    leaves.update();
}
